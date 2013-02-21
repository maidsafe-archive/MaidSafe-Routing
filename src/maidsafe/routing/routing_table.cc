/*******************************************************************************
 *  Copyright 2012 maidsafe.net limited                                        *
 *                                                                             *
 *  The following source code is property of maidsafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the licence   *
 *  file licence.txt found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of maidsafe.net. *
 ******************************************************************************/

#include "maidsafe/routing/routing_table.h"

#include <algorithm>
#include <bitset>
#include <limits>
#include <map>

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/return_codes.h"


namespace maidsafe {

namespace routing {

RoutingTable::RoutingTable(bool client_mode,
                           const NodeId& node_id,
                           const asymm::Keys& keys,
                           NetworkStatistics& network_statistics)
    : kClientMode_(client_mode),
      kNodeId_(node_id),
      kConnectionId_(kClientMode_ ? NodeId(NodeId::kRandomId) : kNodeId_),
      kKeys_(keys),
      kMaxSize_(kClientMode_ ? Parameters::max_routing_table_size_for_client :
                               Parameters::max_routing_table_size),
      kThresholdSize_(kClientMode_ ? Parameters::max_routing_table_size_for_client :
                                     Parameters::routing_table_size_threshold),
      mutex_(),
      furthest_group_node_id_(),
      remove_node_functor_(),
      network_status_functor_(),
      remove_furthest_node_(),
      connected_group_change_functor_(),
      subscribe_to_group_change_update_(),
      close_node_replaced_functor_(),
      nodes_(),
      group_matrix_(kNodeId_, client_mode),
      network_statistics_(network_statistics) {}


void RoutingTable::InitialiseFunctors(NetworkStatusFunctor network_status_functor,
    std::function<void(const NodeInfo&, bool)> remove_node_functor,
    RemoveFurthestUnnecessaryNode remove_furthest_node,
    ConnectedGroupChangeFunctor connected_group_change_functor,
    CloseNodeReplacedFunctor close_node_replaced_functor,
    SubscribeToGroupChangeUpdate subscribe_to_group_change_update,
    UnsubscribeGroupUpdate unsubscribe_group_update) {
  // TODO(Prakash#5#): 2012-10-25 - Consider asserting network_status_functor != nullptr here.
  if (!network_status_functor)
    LOG(kWarning) << "NULL network_status_functor passed.";
  assert(remove_node_functor);
  assert(remove_furthest_node);
  if (!kClientMode_ && !connected_group_change_functor)
    LOG(kWarning) << "NULL close_node_replaced_functor passed.";
  // TODO(Prakash#5#): 2012-10-25 - Handle once we change to matrix.
//  assert(close_node_replaced_functor);
//  if (!remove_node_functor_) {
    network_status_functor_ = network_status_functor;
    remove_node_functor_ = remove_node_functor;
    remove_furthest_node_ = remove_furthest_node;
    connected_group_change_functor_ = connected_group_change_functor;
    close_node_replaced_functor_ = close_node_replaced_functor;
    subscribe_to_group_change_update_ = subscribe_to_group_change_update;
    unsubscribe_group_update_ = unsubscribe_group_update;
//  }
}

bool RoutingTable::AddNode(const NodeInfo& peer) {
  return AddOrCheckNode(peer, true);
}

bool RoutingTable::CheckNode(const NodeInfo& peer) {
  return AddOrCheckNode(peer, false);
}

bool RoutingTable::AddOrCheckNode(NodeInfo peer, bool remove) {
  if (peer.node_id.IsZero() || peer.node_id == kNodeId_) {
    LOG(kError) << "Attempt to add an invalid node " << DebugId(peer.node_id);
    return false;
  }
  if (remove && !asymm::ValidateKey(peer.public_key)) {
    LOG(kInfo) << "Invalid public key for node " << DebugId(peer.node_id);
    return false;
  }

  bool return_value(false), remove_furthest_node(false);
  std::vector<NodeInfo> new_closest_nodes, new_connected_close_nodes;
  NodeInfo out_of_connected_close_nodes;
  NodeInfo removed_node;
  uint16_t routing_table_size(0);

  if (remove)
    SetBucketIndex(peer);

  {
    std::unique_lock<std::mutex> lock(mutex_);
    auto found(Find(peer.node_id, lock));
    if (found.first) {
      LOG(kVerbose) << "Node " << DebugId(peer.node_id) << " already in routing table.";
      return false;
    }

    if (MakeSpaceForNodeToBeAdded(peer, remove, removed_node, lock)) {
      if (remove) {
        assert(peer.bucket != NodeInfo::kInvalidBucket);
        nodes_.push_back(peer);
        UpdateCloseNodeChange(lock,
                              new_connected_close_nodes,
                              out_of_connected_close_nodes,
                              new_closest_nodes);
        if (nodes_.size() > Parameters::greedy_fraction)
          remove_furthest_node = true;
      }
      return_value = true;
    }
    routing_table_size = static_cast<uint16_t>(nodes_.size());
  }

  if (return_value && remove) {  // Firing functors on Add only
    UpdateNetworkStatus(routing_table_size);

    if (!removed_node.node_id.IsZero()) {
      LOG(kVerbose) << "Routing table removed node id : " << DebugId(removed_node.node_id)
                    << ", connection id : " << DebugId(removed_node.connection_id);
      if (remove_node_functor_)
        remove_node_functor_(removed_node, false);
    }

    if (!new_connected_close_nodes.empty()) {
      if (connected_group_change_functor_)
        connected_group_change_functor_(new_connected_close_nodes);
      if (subscribe_to_group_change_update_) {
        if (out_of_connected_close_nodes.node_id != NodeId())
          subscribe_to_group_change_update_(false, out_of_connected_close_nodes);
      }
    }

    if (subscribe_to_group_change_update_)
      subscribe_to_group_change_update_(true, NodeInfo());

    if (!new_closest_nodes.empty()) {
      network_statistics_.UpdateLocalAverageDistance(group_matrix_.GetUniqueNodes());
      if (close_node_replaced_functor_)
        close_node_replaced_functor_(new_closest_nodes);
    }

    if (peer.nat_type == rudp::NatType::kOther) {  // Usable as bootstrap endpoint
//      if (new_bootstrap_endpoint_)
//        new_bootstrap_endpoint_(peer.endpoint);
    }
    if (remove_furthest_node) {
      LOG(kVerbose) << "[" << DebugId(kNodeId_) <<  "] Removing furthest node....";
      if (remove_furthest_node_)
        remove_furthest_node_();
    }
    LOG(kInfo) << PrintRoutingTable();
  }
  return return_value;
}

NodeInfo RoutingTable::DropNode(const NodeId& node_to_drop, bool routing_only) {
  std::vector<NodeInfo> new_closest_nodes, new_connected_close_nodes;
  NodeInfo dropped_node, out_of_connected_close_nodes;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    auto found(Find(node_to_drop, lock));
    if (found.first) {
      dropped_node = *found.second;
      nodes_.erase(found.second);
      UpdateCloseNodeChange(lock,
                            new_connected_close_nodes,
                            out_of_connected_close_nodes,
                            new_closest_nodes);
    }
  }

  if (unsubscribe_group_update_)
    unsubscribe_group_update_(node_to_drop);

  if (!new_connected_close_nodes.empty()) {
    if (connected_group_change_functor_)
      connected_group_change_functor_(new_connected_close_nodes);
    if (subscribe_to_group_change_update_) {
      if (out_of_connected_close_nodes.node_id != NodeId())
        subscribe_to_group_change_update_(false, out_of_connected_close_nodes);
    }
  }

  if (subscribe_to_group_change_update_)
      subscribe_to_group_change_update_(true, NodeInfo());

  if (!new_closest_nodes.empty()) {
    network_statistics_.UpdateLocalAverageDistance(group_matrix_.GetUniqueNodes());
    if (close_node_replaced_functor_)
      close_node_replaced_functor_(new_closest_nodes);
  }

  if (!dropped_node.node_id.IsZero()) {
    assert(nodes_.size() <= std::numeric_limits<uint16_t>::max());
    UpdateNetworkStatus(static_cast<uint16_t>(nodes_.size()));
  }

  if (!dropped_node.node_id.IsZero()) {
    LOG(kVerbose) << "Routing table dropped node id : " << DebugId(dropped_node.node_id)
                  << ", connection id : " << DebugId(dropped_node.connection_id);
    if (remove_node_functor_ && !routing_only)
      remove_node_functor_(dropped_node, false);
  }
  LOG(kInfo) << PrintRoutingTable();
  return dropped_node;
}

bool RoutingTable::IsThisNodeGroupLeader(const NodeId& target_id, NodeInfo& connected_peer) {
  NodeId connected_peer_id;
  std::unique_lock<std::mutex> lock(mutex_);
  if (!group_matrix_.IsThisNodeGroupLeader(target_id, connected_peer_id)) {
    auto found(Find(connected_peer_id, lock));
    if (found.first) {
      connected_peer = *found.second;
      return false;
    }
  }
  return true;
}

bool RoutingTable::ClosestToId(const NodeId& node_id) {
  {
    std::unique_lock<std::mutex> lock(mutex_);

    if (node_id == kNodeId_)
      return false;

    if (nodes_.empty())
      return true;

    if (nodes_.size() == 1) {
      if (nodes_.at(0).node_id == node_id)
        return true;
      else
        return NodeId::CloserToTarget(kNodeId_, nodes_.at(0).node_id, node_id);
    }

    PartialSortFromTarget(node_id, 2, lock);
    uint16_t index(0);
    if (nodes_.at(0).node_id == node_id)
      index = 1;
    if (!NodeId::CloserToTarget(kNodeId_, nodes_.at(index).node_id, node_id))
      return false;

    if (!group_matrix_.ClosestToId(node_id))
      return false;
  }

  if (IsNodeIdInGroupRange(node_id) != GroupRangeStatus::kInRange)
    return false;

  return true;
}

GroupRangeStatus RoutingTable::IsNodeIdInGroupRange(const NodeId& target_id) {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    if (group_matrix_.IsNodeIdInGroupRange(target_id))
      return GroupRangeStatus::kInRange;
  }
  NodeId radius_id(kNodeId_ ^ FurthestCloseNode());
  {
    std::unique_lock<std::mutex> lock(mutex_);
    NodeId distance_id(kNodeId_ ^ target_id);
    crypto::BigInt radius((radius_id.ToStringEncoded(NodeId::kHex) + 'h').c_str());
    crypto::BigInt distance((distance_id.ToStringEncoded(NodeId::kHex) + 'h').c_str());

    if (distance > Parameters::proximity_factor * radius)
      return GroupRangeStatus::kOutwithRange;

    return GroupRangeStatus::kInProximalRange;
  }
}

NodeInfo RoutingTable::GetConnectedPeerFromGroupMatrixClosestTo(const NodeId& target_node_id) {
  std::unique_lock<std::mutex> lock(mutex_);
  NodeInfo node_info;
  node_info = group_matrix_.GetConnectedPeerClosestTo(target_node_id);
  return node_info;
}

std::vector<NodeInfo> RoutingTable::GetMatrixNodes() {
  std::unique_lock<std::mutex> lock(mutex_);
  // TODO(Alison) - prefer GetUniqueNodes() or GetClosestNodes(number)?
  return group_matrix_.GetUniqueNodes();
}

bool RoutingTable::IsConnected(const NodeId& node_id) {
  if (Contains(node_id))
    return true;

  return group_matrix_.Contains(node_id);
}

bool RoutingTable::GetNodeInfo(const NodeId& node_id, NodeInfo& peer) const {
  std::unique_lock<std::mutex> lock(mutex_);
  auto found(Find(node_id, lock));
  if (found.first)
    peer = *found.second;
  return found.first;
}

bool RoutingTable::IsThisNodeInRange(const NodeId& target_id, const uint16_t range) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (nodes_.size() < range)
    return true;
  NthElementSortFromTarget(kNodeId_, range, lock);
  return NodeId::CloserToTarget(target_id, nodes_[range - 1].node_id, kNodeId_);
}

bool RoutingTable::IsThisNodeClosestTo(const NodeId& target_id, bool ignore_exact_match) {
  if (target_id.IsZero()) {
    LOG(kError) << "Invalid target_id passed.";
    return false;
  }
  NodeInfo closest_node(GetClosestNode(target_id, ignore_exact_match));
  return (closest_node.bucket == NodeInfo::kInvalidBucket) ? true :
         NodeId::CloserToTarget(kNodeId_, closest_node.node_id, target_id);
}

bool RoutingTable::Contains(const NodeId& node_id) const {
  std::unique_lock<std::mutex> lock(mutex_);
  return Find(node_id, lock).first;
}

bool RoutingTable::ConfirmGroupMembers(const NodeId& node1, const NodeId& node2) {
  NodeId difference = kNodeId_ ^ FurthestCloseNode();
  return (node1 ^ node2) < difference;
}

void RoutingTable::GroupUpdateFromConnectedPeer(const NodeId& peer,
                                                const std::vector<NodeInfo>& nodes) {
  std::unique_lock<std::mutex> lock(mutex_);
  group_matrix_.UpdateFromConnectedPeer(peer, nodes);
}

void RoutingTable::UpdateCloseNodeChange(
    std::unique_lock<std::mutex>& lock,
    std::vector<NodeInfo>& new_connected_close_nodes,
    NodeInfo& out_of_connected_closest_nodes,
    std::vector<NodeInfo>& new_close_nodes) {
  assert(lock.owns_lock());
//  if (nodes_.size() == 1) {
//    std::vector<NodeInfo> old_row_ids(group_matrix_.GetConnectedPeers());
//    group_matrix_.Clear();
//    group_matrix_.AddConnectedPeer(nodes_.at(0));
//    return;
//  }

  auto count(std::min(Parameters::closest_nodes_size, static_cast<uint16_t>(nodes_.size())));
  PartialSortFromTarget(kNodeId_, count, lock);
  for (auto i(0); i < count; ++i)
    new_connected_close_nodes.push_back(nodes_.at(i));

  std::vector<NodeInfo> old_connected_close_nodes(group_matrix_.GetConnectedPeers());

  std::vector<NodeInfo> old_close_nodes(
      group_matrix_.GetClosestNodes(Parameters::closest_nodes_size));

  std::sort(old_connected_close_nodes.begin(), old_connected_close_nodes.end(),
            [&](const NodeInfo& lhs, const NodeInfo& rhs) {
              return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, kNodeId_);
            });
  std::sort(new_connected_close_nodes.begin(), new_connected_close_nodes.end(),
            [&](const NodeInfo& lhs, const NodeInfo& rhs) {
              return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, kNodeId_);
            });

  // Remove old row from matrix, if found
  std::vector<NodeInfo> difference_result;
  std::set_difference(old_connected_close_nodes.begin(),
                      old_connected_close_nodes.end(),
                      new_connected_close_nodes.begin(),
                      new_connected_close_nodes.end(),
                      std::back_inserter(difference_result),
                      [&](const NodeInfo& lhs, const NodeInfo& rhs) {
                        return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, kNodeId_);
                      });
  if (difference_result.size() >= 2) {
    assert(false);
  } else if (difference_result.size() == 1) {  // Update matrix
    group_matrix_.RemoveConnectedPeer(difference_result.at(0));
    out_of_connected_closest_nodes = difference_result.at(0);
  }

  // Add new row to matrix, if needed
  difference_result.clear();
  std::set_difference(new_connected_close_nodes.begin(),
                      new_connected_close_nodes.end(),
                      old_connected_close_nodes.begin(),
                      old_connected_close_nodes.end(),
                      std::back_inserter(difference_result),
                      [&](const NodeInfo& lhs, const NodeInfo& rhs) {
                        return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, kNodeId_);
                      });
  if (difference_result.size() >= 2) {
    assert(false);
  } else if (difference_result.size() == 1) {  // Update matrix
    group_matrix_.AddConnectedPeer(difference_result.at(0));
  } else if (out_of_connected_closest_nodes.node_id == NodeId()) {
    new_connected_close_nodes.clear();
  }

  new_close_nodes = group_matrix_.GetClosestNodes(Parameters::closest_nodes_size);
  difference_result.clear();
  std::set_difference(new_close_nodes.begin(),
                      new_close_nodes.end(),
                      old_close_nodes.begin(),
                      old_close_nodes.end(),
                      std::back_inserter(difference_result),
                      [&](const NodeInfo& lhs, const NodeInfo& rhs) {
                        return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, kNodeId_);
                      });
  if (difference_result.empty()) {  // Update matrix
    new_close_nodes.clear();
  }
}

// bucket 0 is us, 511 is furthest bucket (should fill first)
void RoutingTable::SetBucketIndex(NodeInfo &node_info) const {
  std::string holder_raw_id(kNodeId_.string());
  std::string node_raw_id(node_info.node_id.string());
  int16_t byte_index(0);
  while (byte_index != NodeId::kSize) {
    if (holder_raw_id[byte_index] != node_raw_id[byte_index]) {
      std::bitset<8> holder_byte(static_cast<int>(holder_raw_id[byte_index]));
      std::bitset<8> node_byte(static_cast<int>(node_raw_id[byte_index]));
      int16_t bit_index(0);
      while (bit_index != 8U) {
        if (holder_byte[7U - bit_index] != node_byte[7U - bit_index])
          break;
        ++bit_index;
      }
      node_info.bucket = (8 * (NodeId::kSize - byte_index)) - bit_index - 1;
      return;
    }
    ++byte_index;
  }
  node_info.bucket = 0;
}

bool RoutingTable::CheckPublicKeyIsUnique(const NodeInfo& node,
                                          std::unique_lock<std::mutex>& lock) const {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  // If we already have a duplicate public key return false
  if (std::find_if(nodes_.begin(),
                   nodes_.end(),
                   [node](const NodeInfo& node_info) {
                     return asymm::MatchingKeys(node_info.public_key, node.public_key);
                   }) != nodes_.end()) {
    LOG(kInfo) << "Already have node with this public key";
    return false;
  }

  // If the endpoint is kNonRoutable then no need to check for endpoint duplication.
//  if (node.endpoint == rudp::kNonRoutable)
//    return true;
  // If we already have a duplicate endpoint return false
//  if (std::find_if(nodes_.begin(),
//                   nodes_.end(),
//                   [node](const NodeInfo& node_info) {
//                     return (node_info.endpoint == node.endpoint);
//                   }) != nodes_.end()) {
//    LOG(kInfo) << "Already have node with this endpoint";
//    return false;
//  }

  return true;
}

bool RoutingTable::MakeSpaceForNodeToBeAdded(const NodeInfo& node,
                                             bool remove,
                                             NodeInfo& removed_node,
                                             std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());

  if (remove && !CheckPublicKeyIsUnique(node, lock))
    return false;

  if (nodes_.size() < kMaxSize_)
    return true;

  PartialSortFromTarget(kNodeId_, Parameters::closest_nodes_size, lock);
  NodeInfo furthest_close_node = nodes_[Parameters::closest_nodes_size - 1];
  auto const furthest_close_node_iter = nodes_.begin() + (Parameters::closest_nodes_size - 1);

  if (NodeId::CloserToTarget(node.node_id, furthest_close_node.node_id, kNodeId_)) {
    if (remove) {
      assert(node.bucket <= furthest_close_node.bucket &&
             "close node replacement to higher bucket");
      removed_node = *furthest_close_node_iter;
      nodes_.erase(furthest_close_node_iter);
    }
    return true;
  }

  uint16_t size(Parameters::bucket_target_size + 1);
  for (auto it = furthest_close_node_iter; it != nodes_.end(); ++it) {
    if (node.bucket >= (*it).bucket)  // Stop searching as it's worthless
      return false;
    // Safety net
    if ((nodes_.end() - it) < size)  // Reached end of checkable area
      return false;

    if ((*it).bucket == (*(it + size)).bucket) {
      // Here we know the node should fit into a bucket if the bucket has too many nodes AND node to
      // add has a lower bucket index
      assert(node.bucket < (*it).bucket);
      if (remove) {
        removed_node = *it;
        nodes_.erase(it);
      }
      return true;
    }
  }
  return false;
}

uint16_t RoutingTable::PartialSortFromTarget(const NodeId& target,
                                             uint16_t number,
                                             std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  uint16_t count = std::min(number, static_cast<uint16_t>(nodes_.size()));
  std::partial_sort(nodes_.begin(),
                    nodes_.begin() + count,
                    nodes_.end(),
                    [target](const NodeInfo& lhs, const NodeInfo& rhs) {
                      return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target);
                    });
  return count;
}

void RoutingTable::NthElementSortFromTarget(const NodeId& target,
                                            uint16_t nth_element,
                                            std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  assert((nodes_.size() >= nth_element) &&
         "This should only be called when n is at max the size of RT");
  std::nth_element(nodes_.begin(),
                   nodes_.begin() + nth_element - 1,
                   nodes_.end(),
                   [target](const NodeInfo& lhs, const NodeInfo& rhs) {
                     return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target);
                   });
}

NodeId RoutingTable::FurthestCloseNode() {
  return GetNthClosestNode(kNodeId_, Parameters::closest_nodes_size).node_id;
}

NodeInfo RoutingTable::GetClosestNode(const NodeId& target_id, bool ignore_exact_match) {
  std::unique_lock<std::mutex> lock(mutex_);
  int sorted_count(PartialSortFromTarget(target_id, 2, lock));
  if (sorted_count == 0)
    return NodeInfo();
  if (ignore_exact_match && (nodes_[0].node_id == target_id))
    return (sorted_count == 1) ? NodeInfo() : nodes_[1];
  return nodes_[0];
}

NodeInfo RoutingTable::GetClosestNode(const NodeId& target_id,
                                      const std::vector<std::string>& exclude,
                                      bool ignore_exact_match) {
  std::vector<NodeInfo> closest_nodes(
      GetClosestNodeInfo(target_id, Parameters::closest_nodes_size, ignore_exact_match));
  for (auto node_info : closest_nodes) {
    if (std::find(exclude.begin(), exclude.end(), node_info.node_id.string()) == exclude.end())
      return node_info;
  }
  return NodeInfo();
}

NodeInfo RoutingTable::GetRemovableNode(std::vector<std::string> attempted) {
  std::map<uint32_t, uint16_t> bucket_rank_map;
  std::unique_lock<std::mutex> lock(mutex_);
  PartialSortFromTarget(kNodeId_, static_cast<uint16_t>(nodes_.size()), lock);

  auto const from_iterator(nodes_.begin() + Parameters::closest_nodes_size);

  for (auto it = from_iterator; it != nodes_.end(); ++it) {
    if (std::find(attempted.begin(), attempted.end(), ((*it).node_id.string())) ==
           attempted.end()) {
      auto bucket_iter = bucket_rank_map.find((*it).bucket);
      if (bucket_iter != bucket_rank_map.end()) {
        (*bucket_iter).second++;
      } else {
        bucket_rank_map.insert(bucket_rank_map.begin(), std::pair<int, int>((*it).bucket, 1));
      }
    }
  }

  int32_t max_bucket(0), max_bucket_count(1);
  for (auto it(bucket_rank_map.begin()); it != bucket_rank_map.end(); ++it) {
    if ((*it).second >= max_bucket_count) {
      max_bucket = (*it).first;
      max_bucket_count = (*it).second;
    }
  }

  LOG(kVerbose) << "[" << DebugId(kNodeId_) << "] max_bucket " << max_bucket
                << " count " << max_bucket_count;
  if (max_bucket_count == 1) {
    return nodes_[nodes_.size() - 1];
  }

  NodeInfo removable_node;
  for (auto it(nodes_.end() - 1) ; it != (from_iterator - 1); --it) {
    if (((*it).bucket == max_bucket) &&
        std::find(attempted.begin(), attempted.end(), (*it).node_id.string()) ==
            attempted.end()) {
      removable_node = (*it);
      break;
    }
  }
  LOG(kVerbose) << "[" << DebugId(kNodeId_) << "] Proposed removable ["
                << DebugId(removable_node.node_id) << "]";
  return removable_node;
}

void RoutingTable::GetNodesNeedingGroupUpdates(std::vector<NodeInfo>& nodes_needing_update) {
  std::unique_lock<std::mutex> lock(mutex_);
  int sorted_count(PartialSortFromTarget(kNodeId_, Parameters::closest_nodes_size , lock));
  if (sorted_count == 0)
    return;
  for (auto iter(nodes_.begin());
       iter != (nodes_.begin() + std::min(Parameters::closest_nodes_size,
                                          static_cast<uint16_t>(nodes_.size())));
       ++iter) {
    if (group_matrix_.IsRowEmpty(*iter))
      nodes_needing_update.push_back(*iter);
  }
}

NodeInfo RoutingTable::GetNthClosestNode(const NodeId& target_id, uint16_t node_number) {
  assert((node_number > 0) && "Node number starts with position 1");
  std::unique_lock<std::mutex> lock(mutex_);
  if (nodes_.size() < node_number) {
    NodeInfo node_info;
    node_info.node_id = (NodeId(NodeId::kMaxId) ^ kNodeId_);
    return node_info;
  }
  NthElementSortFromTarget(target_id, node_number, lock);
  return nodes_[node_number - 1];
}

std::vector<NodeId> RoutingTable::GetClosestNodes(const NodeId& target_id, uint16_t number_to_get) {
  std::vector<NodeId> close_nodes;
  std::unique_lock<std::mutex> lock(mutex_);
  int sorted_count(PartialSortFromTarget(target_id, number_to_get, lock));

  for (int i = 0; i != sorted_count; ++i)
    close_nodes.push_back(nodes_[i].node_id);
  return close_nodes;
}

std::vector<NodeInfo> RoutingTable::GetClosestMatrixNodes(const NodeId& target_id,
                                                          uint16_t number_to_get) {
  std::unique_lock<std::mutex> lock(mutex_);
  std::vector<NodeInfo> closest_matrix_nodes(group_matrix_.GetUniqueNodes());
  size_t sorting_size(std::min(static_cast<size_t>(number_to_get),
                               closest_matrix_nodes.size()));
  std::partial_sort(closest_matrix_nodes.begin(),
                    closest_matrix_nodes.begin() + sorting_size,
                    closest_matrix_nodes.end(),
                    [&target_id](const NodeInfo& lhs, const NodeInfo& rhs) {
                      return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target_id);
                  });
  closest_matrix_nodes.resize(sorting_size);
  return closest_matrix_nodes;
}

std::vector<NodeId> RoutingTable::GetGroup(const NodeId& target_id) {
  std::vector<NodeInfo> nodes;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    nodes = group_matrix_.GetUniqueNodes();
  }
  std::vector<NodeId> group;
  std::partial_sort(nodes.begin(),
                    nodes.begin() + Parameters::node_group_size,
                    nodes.end(),
                    [&](const NodeInfo& lhs, const NodeInfo& rhs) {
                      return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target_id);
                    });
  for (auto iter(nodes.begin()); iter != nodes.begin() + Parameters::node_group_size; ++iter)
    group.push_back(iter->node_id);
  return group;
}

std::vector<NodeInfo> RoutingTable::GetClosestNodeInfo(const NodeId& target_id,
                                                       uint16_t number_to_get,
                                                       bool ignore_exact_match) {
  std::unique_lock<std::mutex> lock(mutex_);
  int sorted_count(PartialSortFromTarget(target_id, number_to_get + 1, lock));
  if (sorted_count == 0)
    return std::vector<NodeInfo>();

  auto itr(nodes_.begin());
  if (ignore_exact_match && ((*itr).node_id == target_id)) {
    return (sorted_count == 1) ? std::vector<NodeInfo>() :
           std::vector<NodeInfo>(nodes_.begin() + 1, nodes_.begin() + sorted_count);
  }

  return std::vector<NodeInfo>(nodes_.begin(),
      nodes_.begin() + std::min(sorted_count, static_cast<int>(number_to_get)));
}

std::pair<bool, std::vector<NodeInfo>::iterator> RoutingTable::Find(
    const NodeId& node_id,
    std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  auto itr(std::find_if(nodes_.begin(),
                        nodes_.end(),
                        [&node_id](const NodeInfo& node_info) {
                          return node_info.node_id == node_id;
                        }));
  return std::make_pair(itr != nodes_.end(), itr);
}

std::pair<bool, std::vector<NodeInfo>::const_iterator> RoutingTable::Find(
    const NodeId& node_id,
    std::unique_lock<std::mutex>& lock) const {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  auto itr(std::find_if(nodes_.begin(),
                        nodes_.end(),
                        [&node_id](const NodeInfo& node_info) {
                          return node_info.node_id == node_id;
                        }));
  return std::make_pair(itr != nodes_.end(), itr);
}

void RoutingTable::UpdateNetworkStatus(uint16_t size) const {
#ifndef TESTING
  assert(network_status_functor_);
#else
  if (!network_status_functor_)
    return;
#endif
  network_status_functor_(static_cast<int>(size) * 100 / kMaxSize_);
  LOG(kVerbose) << DebugId(kNodeId_) << "Updating network status !!! " << (size * 100) / kMaxSize_;
}

size_t RoutingTable::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return nodes_.size();
}

std::string RoutingTable::PrintRoutingTable() {
  std::vector<NodeInfo> rt;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::sort(nodes_.begin(),
              nodes_.end(),
              [&](const NodeInfo& lhs, const NodeInfo& rhs) {
                return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, kNodeId_);
              });
    rt = nodes_;
  }
  std::string s = "\n\n[" + DebugId(kNodeId_) +
      "] This node's own routing table and peer connections:\n" +
      "Routing table size: " + std::to_string(nodes_.size()) + "\n";
  for (auto node : rt) {
    s += std::string("\tPeer ") + "[" + DebugId(node.node_id) + "]" + "-->";
    s += DebugId(node.connection_id) + " && xored ";
    s += DebugId(kNodeId_ ^ node.node_id) + " bucket ";
    s += std::to_string(node.bucket) + "\n";
  }
  s += "\n\n";
  return s;
}

void RoutingTable::PrintGroupMatrix() {}

}  // namespace routing

}  // namespace maidsafe
