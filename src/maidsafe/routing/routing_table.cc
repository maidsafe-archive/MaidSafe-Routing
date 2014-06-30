/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#include "maidsafe/routing/routing_table.h"

#include <algorithm>
#include <bitset>
#include <limits>
#include <map>

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/tools/network_viewer.h"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/matrix_change.h"

namespace maidsafe {

namespace routing {

RoutingTable::RoutingTable(bool client_mode, const NodeId& node_id, const asymm::Keys& keys,
                           NetworkStatistics& network_statistics)
    : kClientMode_(client_mode),
      kNodeId_(node_id),
      kConnectionId_(kClientMode_ ? NodeId(NodeId::IdType::kRandomId) : kNodeId_),
      kKeys_(keys),
      kMaxSize_(kClientMode_ ? Parameters::max_routing_table_size_for_client
                             : Parameters::max_routing_table_size),
      kThresholdSize_(kClientMode_ ? Parameters::max_routing_table_size_for_client
                                   : Parameters::routing_table_size_threshold),
      mutex_(),
      remove_node_functor_(),
      network_status_functor_(),
      nodes_(),
      ipc_message_queue_(),
      network_statistics_(network_statistics) {
#ifdef TESTING
  try {
    ipc_message_queue_.reset(new boost::interprocess::message_queue(
        boost::interprocess::open_only, network_viewer::kMessageQueueName.c_str()));
    if (static_cast<uint16_t>(ipc_message_queue_->get_max_msg_size()) <
        (Parameters::closest_nodes_size + 1) * Parameters::closest_nodes_size * 2 * NodeId::kSize) {
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::invalid_parameter));
    }
  }
  catch (const std::exception&) {
    ipc_message_queue_.reset();
  }
#endif
}

RoutingTable::~RoutingTable() {
  if (ipc_message_queue_) {
    network_viewer::MatrixRecord matrix_record(kNodeId_);
    std::string serialised_matrix(matrix_record.Serialise());
    ipc_message_queue_->try_send(serialised_matrix.c_str(), serialised_matrix.size(), 0);
  }
}

void RoutingTable::InitialiseFunctors(
    NetworkStatusFunctor network_status_functor,
    std::function<void(const NodeInfo&, bool)> remove_node_functor,    
    MatrixChangedFunctor matrix_change_functor) {
  assert(remove_node_functor);
  assert(network_status_functor);
  network_status_functor_ = network_status_functor;
  remove_node_functor_ = remove_node_functor;
  matrix_change_functor_ = matrix_change_functor;
}

bool RoutingTable::AddNode(const NodeInfo& peer) {
  return AddOrCheckNode(peer, true);
}

bool RoutingTable::CheckNode(const NodeInfo& peer) { return AddOrCheckNode(peer, false); }

bool RoutingTable::AddOrCheckNode(NodeInfo peer, bool remove) {
  if (peer.node_id.IsZero() || peer.node_id == kNodeId_) {
    LOG(kError) << "Attempt to add an invalid node " << DebugId(peer.node_id);
    return false;
  }
  if (remove && !asymm::ValidateKey(peer.public_key)) {
    LOG(kInfo) << "Invalid public key for node " << DebugId(peer.node_id);
    return false;
  }

  bool return_value(false);
  NodeInfo removed_node;
  uint16_t routing_table_size(0);
  bool close_node_added(false);

  if (remove)
    SetBucketIndex(peer);
  std::vector<NodeId> new_close_nodes, old_close_nodes;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    auto found(Find(peer.node_id, lock));
    if (found.first) {
      LOG(kVerbose) << "Node " << DebugId(peer.node_id) << " already in routing table.";
      return false;
    }

    auto close_nodes_size(PartialSortFromTarget(kNodeId_, Parameters::closest_nodes_size, lock));
    std::for_each (std::begin(nodes_), std::begin(nodes_) + close_nodes_size,
                   [&old_close_nodes, &new_close_nodes](const NodeInfo& node_info) {
                     old_close_nodes.push_back(node_info.node_id);
                     new_close_nodes.push_back(node_info.node_id);
                   });

    if (MakeSpaceForNodeToBeAdded(peer, remove, removed_node, lock)) {
      if (remove) {
        assert(peer.bucket != NodeInfo::kInvalidBucket);
        close_node_added = (nodes_.size() >= Parameters::closest_nodes_size) ?
            NodeId::CloserToTarget(peer.node_id,
                                   nodes_.at(Parameters::closest_nodes_size - 1).node_id,
                                   kNodeId()) : true;
        nodes_.push_back(peer);
        if (close_node_added) {
          if (nodes_.size() > Parameters::closest_nodes_size)
            new_close_nodes.pop_back();
          new_close_nodes.push_back(peer.node_id);
        }
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

    if (close_node_added) {
      network_statistics_.UpdateLocalAverageDistance(new_close_nodes);
      if (matrix_change_functor_)
        matrix_change_functor_(std::make_shared<MatrixChange>(kNodeId(), old_close_nodes,
                                                              new_close_nodes));
//      IpcSendGroupMatrix();
    }

    if (peer.nat_type == rudp::NatType::kOther) {  // Usable as bootstrap endpoint
                                                   // if (new_bootstrap_endpoint_)
                                                   // new_bootstrap_endpoint_(peer.endpoint);
    }
    LOG(kInfo) << PrintRoutingTable();
  }
  return return_value;
}

NodeInfo RoutingTable::DropNode(const NodeId& node_to_drop, bool routing_only) {
  NodeInfo dropped_node;
  NodeId potential_close_id;
  std::vector<NodeId> new_close_nodes, old_close_nodes;
  bool close_node_removal(false);
  {
    std::unique_lock<std::mutex> lock(mutex_);
    auto close_nodes_size(PartialSortFromTarget(kNodeId_, Parameters::closest_nodes_size + 1,
                                                lock));
    if (close_nodes_size == Parameters::closest_nodes_size + 1)
      potential_close_id = nodes_.at(Parameters::closest_nodes_size).node_id;
    std::for_each (std::begin(nodes_),
                   std::begin(nodes_) + std::min(close_nodes_size, Parameters::closest_nodes_size),
                   [&old_close_nodes, &new_close_nodes](const NodeInfo& node_info) {
                     old_close_nodes.push_back(node_info.node_id);
                     new_close_nodes.push_back(node_info.node_id);
                   });
    auto found(Find(node_to_drop, lock));
    if (found.first) {
      dropped_node = *found.second;
      nodes_.erase(found.second);
      }
    }

    close_node_removal = std::any_of(std::begin(old_close_nodes), std::end(old_close_nodes),
                                     [dropped_node](const NodeId node_id) {
                                       return node_id == dropped_node.node_id;
                                     });

  if (close_node_removal) {
    if (nodes_.size() >= Parameters::closest_nodes_size) {
      new_close_nodes.erase(std::remove(std::begin(new_close_nodes), std::end(new_close_nodes),
                                        dropped_node.node_id),
                            std::end(new_close_nodes));
      new_close_nodes.push_back(potential_close_id);
    }
    network_statistics_.UpdateLocalAverageDistance(new_close_nodes);
    if (matrix_change_functor_) {
      matrix_change_functor_(std::make_shared<MatrixChange>(kNodeId(), old_close_nodes,
                                                            new_close_nodes));
    }
//    IpcSendGroupMatrix();
  }

  if (!dropped_node.node_id.IsZero()) {
    assert(nodes_.size() <= std::numeric_limits<uint16_t>::max());
    UpdateNetworkStatus(static_cast<uint16_t>(nodes_.size()));
  }

  if (!dropped_node.node_id.IsZero()) {
    LOG(kVerbose) << DebugId(kNodeId()) << "Routing table dropped node id : "
                  << DebugId(dropped_node.node_id) << ", connection id : "
                  << DebugId(dropped_node.connection_id);
    if (remove_node_functor_ && !routing_only)
      remove_node_functor_(dropped_node, false);
  }
  LOG(kInfo) << PrintRoutingTable();
  return dropped_node;
}

GroupRangeStatus RoutingTable::IsNodeIdInGroupRange(const NodeId& group_id) const {
  return IsNodeIdInGroupRange(group_id, kNodeId_);
}

GroupRangeStatus RoutingTable::IsNodeIdInGroupRange(const NodeId& /*group_id*/,
                                                    const NodeId& /*node_id*/) const {
  return GroupRangeStatus::kInRange;
//  if (group_id == node_id)
//    return GroupRangeStatus::kInRange;


//  if (nodes_.size() <= Parameters::group_size)
//    return GroupRangeStatus::kInRange;

//  std::lock_guard<std::mutex> lock(mutex_);
//  PartialSortFromTarget(kNodeId(), Parameters::group_size + 1, lock);

//  size_t holders_size = std::min(nodes_.size(), Parameters::group_size);
//  std::vector<NodeInfo> holders_id;
//  auto iter(std::begin(nodes_));
//  for (; holders_id.size() < holders_size - 1; ++iter) {
//    if (iter->node_id != group_id)
//      holders_id.push_back(iter->node_id);
//  }

//  bool not_in_range(false);
//  if (!NodeId::CloserToTarget(kNodeId(), iter->node_id, group_id)) {
//    holders_id.push_back(iter->node_id);
//    not_in_range = true;
//  } else {
//    holders_id.push_back(kNodeId());
//  }

//  assert(holders_id.size() <= Parameters::group_size);

//  if (!client_mode_) {
//    auto this_node_range(GetProximalRange(group_id, kNodeId_, kNodeId_, radius_, new_holders,
//                                          not_in_range));
//    if (node_id == kNodeId_)
//      return this_node_range;
//    else if (this_node_range != GroupRangeStatus::kInRange)
//      BOOST_THROW_EXCEPTION(MakeError(RoutingErrors::not_in_range));  // not_in_group
//  } else {
//    if (node_id == kNodeId_)
//      return GroupRangeStatus::kInProximalRange;
//  }
//  return GetProximalRange(group_id, node_id, kNodeId_, radius_, new_holders, not_in_range);
}

NodeId RoutingTable::RandomConnectedNode() {
  std::unique_lock<std::mutex> lock(mutex_);
// Commenting out assert as peer starts treating this node as joined as soon as it adds
// it into its routing table.
//  assert(nodes_.size() > Parameters::closest_nodes_size &&
//         "Shouldn't call RandomConnectedNode when routing table size is <= closest_nodes_size");
  assert(!nodes_.empty());
  if (nodes_.empty())
    return NodeId();

  PartialSortFromTarget(kNodeId_, static_cast<uint16_t>(nodes_.size()), lock);
  size_t index(RandomUint32() % (nodes_.size()));
  return nodes_.at(index).node_id;
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
  if (target_id == kNodeId())
    return false;

  if (nodes_.empty())
    return true;

  if (target_id.IsZero()) {
    LOG(kError) << "Invalid target_id passed.";
    return false;
  }

  NodeInfo closest_node(GetClosestNode(target_id, ignore_exact_match));
  return (closest_node.bucket == NodeInfo::kInvalidBucket) ||
         NodeId::CloserToTarget(kNodeId_, closest_node.node_id, target_id);
}

bool RoutingTable::Contains(const NodeId& node_id) const {
  std::unique_lock<std::mutex> lock(mutex_);
  return Find(node_id, lock).first;
}

bool RoutingTable::ConfirmGroupMembers(const NodeId& node1, const NodeId& node2) {
  NodeId difference =
      kNodeId_ ^ GetNthClosestNode(kNodeId(), std::min(static_cast<uint16_t>(nodes_.size()),
                                                       Parameters::closest_nodes_size)).node_id;
  return (node1 ^ node2) < difference;
}

// bucket 0 is us, 511 is furthest bucket (should fill first)
void RoutingTable::SetBucketIndex(NodeInfo& node_info) const {
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
  if (std::find_if(nodes_.begin(), nodes_.end(), [node](const NodeInfo & node_info) {
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

bool RoutingTable::MakeSpaceForNodeToBeAdded(const NodeInfo& node, bool remove,
                                             NodeInfo& removed_node,
                                             std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());

  std::map<uint32_t, uint16_t> bucket_rank_map;

  if (remove && !CheckPublicKeyIsUnique(node, lock))
    return false;

  if (nodes_.size() < kMaxSize_)
    return true;

  if (client_mode()) {
    assert(nodes_.size() == kMaxSize_);
    if (NodeId::CloserToTarget(node.node_id, nodes_.at(kMaxSize_ - 1).node_id, kNodeId())) {
      removed_node = *nodes_.rbegin();
      nodes_.pop_back();
      return true;
    } else {
      return false;
    }
  }

  int32_t max_bucket(0), max_bucket_count(1);
  std::for_each(std::begin(nodes_) + Parameters::closest_nodes_size * 2, std::end(nodes_),
                [&bucket_rank_map, &max_bucket, &max_bucket_count](const NodeInfo& node_info) {
                  auto bucket_iter(bucket_rank_map.find(node_info.bucket));
                  if (bucket_iter != std::end(bucket_rank_map))
                    (*bucket_iter).second++;
                   else
                    bucket_rank_map.insert(std::make_pair(node_info.bucket, 1));

                   if (bucket_rank_map[node_info.bucket] >= max_bucket_count) {
                     max_bucket = node_info.bucket;
                     max_bucket_count = bucket_rank_map[node_info.bucket];
                   }
                });

  LOG(kVerbose) << "[" << DebugId(kNodeId_) << "] max_bucket " << max_bucket << " count "
                << max_bucket_count;

  // If no duplicate bucket exists, prioirity is given to closer nodes.
  if ((max_bucket_count == 1) && (nodes_.back().bucket < node.bucket))
    return false;

  for (auto it(nodes_.rbegin()); it != nodes_.rend(); ++it)
    if (it->bucket == max_bucket) {
      if (remove) {
        removed_node = *it;
        nodes_.erase(--(it.base()));
        LOG(kVerbose) << kNodeId_ << " Proposed removable " << removed_node.node_id;
      }
      return true;
    }
  return false;
}

uint16_t RoutingTable::PartialSortFromTarget(const NodeId& target, uint16_t number,
                                             std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  uint16_t count = std::min(number, static_cast<uint16_t>(nodes_.size()));
  std::partial_sort(nodes_.begin(), nodes_.begin() + count, nodes_.end(),
                    [target](const NodeInfo& lhs, const NodeInfo& rhs) {
    return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target);
  });
  return count;
}

void RoutingTable::NthElementSortFromTarget(const NodeId& target, uint16_t nth_element,
                                            std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  assert((nodes_.size() >= nth_element) &&
         "This should only be called when n is at max the size of RT");
#ifndef __GNUC__
  std::nth_element(nodes_.begin(), nodes_.begin() + nth_element - 1, nodes_.end(),
                   [target](const NodeInfo & lhs, const NodeInfo & rhs) {
                       return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target);
                   });
#else
// BEFORE_RELEASE use std::nth_element() for all platform when min required Gcc version is 4.8.3
// http://gcc.gnu.org/bugzilla/show_bug.cgi?id=58800 Bug fixed in gcc 4.8.3
  std::partial_sort(nodes_.begin(), nodes_.begin() + nth_element - 1, nodes_.end(),
                    [target](const NodeInfo & lhs, const NodeInfo & rhs) {
                        return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target);
                    });
#endif
}

NodeInfo RoutingTable::GetClosestNode(const NodeId& target_id, bool ignore_exact_match,
                                      const std::vector<std::string>& exclude) {
  auto closest_nodes(GetClosestNodes(target_id, Parameters::closest_nodes_size,
                                     ignore_exact_match));
  for (const auto& node_info : closest_nodes) {
    if (std::find(exclude.begin(), exclude.end(), node_info.node_id.string()) == exclude.end())
      return node_info;
  }
  return NodeInfo();
}

NodeInfo RoutingTable::GetNthClosestNode(const NodeId& target_id, uint16_t node_number) {
  assert((node_number > 0) && "Node number starts with position 1");
  std::unique_lock<std::mutex> lock(mutex_);
  if (nodes_.size() < node_number) {
    NodeInfo node_info;
    node_info.node_id = (NodeId(NodeId::IdType::kMaxId) ^ kNodeId_);
    return node_info;
  }
  NthElementSortFromTarget(target_id, node_number, lock);
  return nodes_[node_number - 1];
}

std::vector<NodeId> RoutingTable::GetGroup(const NodeId& target_id) {
  std::vector<NodeId> group;
  std::unique_lock<std::mutex> lock(mutex_);
  auto count(PartialSortFromTarget(target_id, Parameters::group_size, lock));
  for (auto iter(nodes_.begin()); iter != nodes_.begin() + count; ++iter)
    group.push_back(iter->node_id);
  return group;
}

std::vector<NodeInfo> RoutingTable::GetClosestNodes(const NodeId& target_id, uint16_t number_to_get,
                                                    bool ignore_exact_match) {
  std::unique_lock<std::mutex> lock(mutex_);
  int sorted_count(PartialSortFromTarget(target_id, number_to_get + 1, lock));
  if (sorted_count == 0)
    return std::vector<NodeInfo>();

  if (sorted_count == 1) {
    if (ignore_exact_match)
      return (nodes_.begin()->node_id == target_id)
                  ? std::vector<NodeInfo>()
                  : std::vector<NodeInfo>(std::begin(nodes_), std::end(nodes_));
    else
      return std::vector<NodeInfo>(std::begin(nodes_), std::end(nodes_));
  }

  uint16_t index(ignore_exact_match && nodes_.begin()->node_id == target_id);
  return std::vector<NodeInfo>(std::begin(nodes_) + index, std::begin(nodes_) + sorted_count);
}

std::pair<bool, std::vector<NodeInfo>::iterator> RoutingTable::Find(
    const NodeId& node_id, std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  auto itr(std::find_if(nodes_.begin(), nodes_.end(), [&node_id](const NodeInfo & node_info) {
    return node_info.node_id == node_id;
  }));
  return std::make_pair(itr != nodes_.end(), itr);
}

std::pair<bool, std::vector<NodeInfo>::const_iterator> RoutingTable::Find(
    const NodeId& node_id, std::unique_lock<std::mutex>& lock) const {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  auto itr(std::find_if(nodes_.begin(), nodes_.end(), [&node_id](const NodeInfo & node_info) {
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
  LOG(kVerbose) << DebugId(kNodeId_) << " Updating network status !!! " << (size * 100) / kMaxSize_;
}

size_t RoutingTable::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return nodes_.size();
}

/* remove group matrix
void RoutingTable::IpcSendGroupMatrix() const {
  if (ipc_message_queue_) {
    network_viewer::MatrixRecord matrix_record(kNodeId_);
    std::vector<NodeInfo> matrix, close;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      matrix = group_matrix_.GetUniqueNodes();
      close = group_matrix_.GetConnectedPeers();
    }
    std::string printout("\tMatrix sent by: " + DebugId(kNodeId_) + "\n");
    for (const auto& matrix_element : matrix) {
      matrix_record.AddElement(matrix_element.node_id, network_viewer::ChildType::kMatrix);
      printout += "\t\t" + DebugId(matrix_element.node_id) + " - kMatrix\n";
    }

    std::sort(std::begin(close), std::end(close),
              [this](const NodeInfo & lhs, const NodeInfo & rhs) {
      return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, kNodeId_);
    });

    size_t index(0);
    size_t limit(std::min(static_cast<size_t>(Parameters::group_size), close.size()));
    for (; index < limit; ++index) {
      matrix_record.AddElement(close[index].node_id, network_viewer::ChildType::kGroup);
      printout += "\t\t" + DebugId(close[index].node_id) + " - kGroup\n";
    }
    for (; index < close.size(); ++index) {
      matrix_record.AddElement(close[index].node_id, network_viewer::ChildType::kClosest);
      printout += "\t\t" + DebugId(close[index].node_id) + " - kClosest\n";
    }
    LOG(kInfo) << printout << '\n';
    std::string serialised_matrix(matrix_record.Serialise());
    ipc_message_queue_->try_send(serialised_matrix.c_str(), serialised_matrix.size(), 0);
  }
}
*/

std::string RoutingTable::PrintRoutingTable() {
  std::vector<NodeInfo> rt;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::sort(nodes_.begin(), nodes_.end(), [&](const NodeInfo & lhs, const NodeInfo & rhs) {
      return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, kNodeId_);
    });
    rt = nodes_;
  }
  std::string s = "\n\n[" + DebugId(kNodeId_) +
                  "] This node's own routing table and peer connections:\n" +
                  "Routing table size: " + std::to_string(nodes_.size()) + "\n";
  for (const auto& node : rt) {
    s += std::string("\tPeer ") + "[" + DebugId(node.node_id) + "]" + "-->";
    s += DebugId(node.connection_id) + " && xored ";
    s += DebugId(kNodeId_ ^ node.node_id) + " bucket ";
    s += std::to_string(node.bucket) + "\n";
  }
  s += "\n\n";
  return s;
}

}  // namespace routing

}  // namespace maidsafe
