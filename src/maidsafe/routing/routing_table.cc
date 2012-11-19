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

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/return_codes.h"


namespace maidsafe {

namespace routing {

RoutingTable::RoutingTable(const Fob& fob, bool client_mode)
    : kMaxSize_(client_mode ? Parameters::max_client_routing_table_size :
                              Parameters::max_routing_table_size),
      kClientMode_(client_mode),
      kFob_(fob),
      kNodeId_(NodeId(kFob_.identity)),
      kConnectionId_((client_mode ? NodeId(NodeId::kRandomId) : kNodeId_)),
      mutex_(),
      furthest_group_node_id_(),
      remove_node_functor_(),
      network_status_functor_(),
      close_node_replaced_functor_(),
      remove_furthest_node_(),
      nodes_() {}

void RoutingTable::InitialiseFunctors(
    NetworkStatusFunctor network_status_functor,
    std::function<void(const NodeInfo&, bool)> remove_node_functor,
    CloseNodeReplacedFunctor close_node_replaced_functor,
    RemoveFurthestUnnecessaryNode remove_furthest_node) {
  // TODO(Prakash#5#): 2012-10-25 - Consider asserting network_status_functor != nullptr here.
  if (!network_status_functor)
    LOG(kWarning) << "NULL network_status_functor passed.";
  assert(remove_node_functor);
  assert(remove_furthest_node);
  if (!kClientMode_ && !close_node_replaced_functor)
    LOG(kWarning) << "NULL close_node_replaced_functor passed.";
  // TODO(Prakash#5#): 2012-10-25 - Handle once we change to matrix.
//  assert(close_node_replaced_functor);
  if (!remove_node_functor_) {
    network_status_functor_ = network_status_functor;
    remove_node_functor_ = remove_node_functor;
    close_node_replaced_functor_ = close_node_replaced_functor;
    remove_furthest_node_ = remove_furthest_node;
  }
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
  std::vector<NodeInfo> new_close_nodes;
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
        new_close_nodes = CheckGroupChange(lock);
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

    if (!new_close_nodes.empty()) {
      if (close_node_replaced_functor_)
        close_node_replaced_functor_(new_close_nodes);
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
  std::vector<NodeInfo> new_close_nodes;
  NodeInfo dropped_node;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    auto found(Find(node_to_drop, lock));
    if (found.first) {
      dropped_node = *found.second;
      nodes_.erase(found.second);
      new_close_nodes = CheckGroupChange(lock);
    }
  }

  if (!dropped_node.node_id.IsZero()) {
    assert(nodes_.size() <= std::numeric_limits<uint16_t>::max());
    UpdateNetworkStatus(static_cast<uint16_t>(nodes_.size()));
  }

  if (!new_close_nodes.empty()) {
    if (close_node_replaced_functor_)
      close_node_replaced_functor_(new_close_nodes);
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

bool RoutingTable::IsConnected(const NodeId& node_id) const {
  std::unique_lock<std::mutex> lock(mutex_);
  return Find(node_id, lock).first;
}

bool RoutingTable::IsRemovable(const NodeId& node_id) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (nodes_.size() < Parameters::routing_table_size_threshold)
    return false;
  uint16_t sorted_count(PartialSortFromTarget(NodeId(), 
      static_cast<uint16_t>(nodes_.size()), lock));
  if (sorted_count == 0)
    return false;
  size_t size(std::count_if(nodes_.begin(),
                          nodes_.end(),
                          [&](const NodeInfo& node_info) {
                            return (node_info.node_id > node_id);
                          }));
  if ((size < Parameters::split_avoidance) ||
      (size >= (Parameters::max_routing_table_size - Parameters::split_avoidance))) {
    return false;
  }
  PartialSortFromTarget(node_id, static_cast<uint16_t>(nodes_.size()), lock);
  if (nodes_[0].node_id != node_id)
    return false;
  if (NodeId::CloserToTarget(kNodeId(), nodes_[1].node_id, node_id))
    return false;
  return true;
}


bool RoutingTable::ConfirmGroupMembers(const NodeId& node1, const NodeId& node2) {
  NodeId difference = NodeId(kFob().identity) ^ FurthestCloseNode();
  return (node1 ^ node2) < difference;
}

std::vector<NodeInfo> RoutingTable::CheckGroupChange(std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());

  PartialSortFromTarget(kNodeId_, Parameters::closest_nodes_size, lock);

  if (nodes_.size() > Parameters::closest_nodes_size) {
    NodeId new_furthest_group_node_id = nodes_[Parameters::closest_nodes_size - 1].node_id;
    if (furthest_group_node_id_ != new_furthest_group_node_id) {
      LOG(kVerbose) << "Group change !. old furthest_close_node : "
                    << DebugId(furthest_group_node_id_)
                    << "new_furthest_group_node_id : "
                    << DebugId(new_furthest_group_node_id);
      furthest_group_node_id_ = nodes_[Parameters::closest_nodes_size - 1].node_id;
      return std::vector<NodeInfo>(nodes_.begin(), nodes_.begin() + Parameters::closest_nodes_size);
    } else {  // No change
      return std::vector<NodeInfo>();
    }
  } else {
    if (!nodes_.empty())
       furthest_group_node_id_ = nodes_.back().node_id;
    return nodes_;
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
                   nodes_.begin() + nth_element,
                   nodes_.end(),
                   [target](const NodeInfo& lhs, const NodeInfo& rhs) {
                     return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target);
                   });
}

NodeId RoutingTable::FurthestCloseNode() {
  return GetNthClosestNode(NodeId(kFob().identity), Parameters::closest_nodes_size).node_id;
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

NodeInfo RoutingTable::GetClosestTo(const NodeId& node_id, bool backward) {
  std::unique_lock<std::mutex> lock(mutex_);
  uint16_t sorted_count(PartialSortFromTarget(kNodeId(), 
      static_cast<uint16_t>(nodes_.size()), lock));
  if (sorted_count == 0)
    return NodeInfo();
  auto iterator(std::find_if(nodes_.begin(),
                             nodes_.end(),
                             [&](const NodeInfo& node_info) {
                               return node_info.node_id == node_id;
                             }));
  size_t index(std::distance(nodes_.begin(), iterator));
  if (index == nodes_.size())
    return NodeInfo();
  if (backward && (index > 0))
    return nodes_[index - 1];
  if (!backward && (index < nodes_.size() - 1))
    return nodes_[index + 1];
  return NodeInfo();
}

NodeInfo RoutingTable::GetFurthestRemovableNode() {
  std::vector<NodeInfo> sorted_routing_table;
  std::unique_lock<std::mutex> lock(mutex_);
  uint16_t sorted_count(PartialSortFromTarget(kNodeId(), 
      static_cast<uint16_t>(nodes_.size()), lock));
  if (sorted_count == 0)
    return NodeInfo();
  sorted_routing_table = nodes_;
  for (size_t index(sorted_routing_table.size() - 1);
       index > Parameters::closest_nodes_size;
       --index) {
    PartialSortFromTarget(sorted_routing_table[index].node_id, 2, lock);
    if (NodeId::CloserToTarget(nodes_[1].node_id, kNodeId(), nodes_[0].node_id))
        return nodes_[0];
  }
  return NodeInfo();
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
  if (network_status_functor_)
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
      "Routing table size: " + boost::lexical_cast<std::string>(nodes_.size()) + "\n" +
      "Binary: " + (kNodeId_.ToStringEncoded(NodeId::kBinary)).substr(0, 32) + "\n";
  for (auto node : rt) {
    s += std::string("\tPeer ") + "[" + DebugId(node.node_id) + "]" + "-->";
    s += DebugId(node.connection_id) + " && ";
    s += (node.node_id.ToStringEncoded(NodeId::kBinary)).substr(0, 32) + " xored ";
    s += DebugId(kNodeId_ ^ node.node_id) + "\n";
  }
  s += "\n\n";
  return s;
}

}  // namespace routing

}  // namespace maidsafe
