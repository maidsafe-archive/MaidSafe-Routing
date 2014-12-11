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
#include <sstream>

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/tools/network_viewer.h"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/close_nodes_change.h"

namespace maidsafe {

namespace routing {

RoutingTable::RoutingTable(bool client_mode, const NodeId& node_id, const asymm::Keys& keys)
    : kClientMode_(client_mode),
      kNodeId_(node_id),
      kConnectionId_(kClientMode_ ? NodeId(RandomString(NodeId::kSize)) : kNodeId_),
      kKeys_(keys),
      kMaxSize_(kClientMode_ ? Parameters::max_routing_table_size_for_client
                             : Parameters::max_routing_table_size),
      kThresholdSize_(kClientMode_ ? Parameters::max_routing_table_size_for_client
                                   : Parameters::routing_table_size_threshold),
      mutex_(),
      routing_table_change_functor_(),
      nodes_(),
      ipc_message_queue_() {
#ifdef TESTING
  try {
    ipc_message_queue_.reset(new boost::interprocess::message_queue(
        boost::interprocess::open_only, network_viewer::kMessageQueueName.c_str()));
    if (static_cast<unsigned int>(ipc_message_queue_->get_max_msg_size()) <
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

void RoutingTable::InitialiseFunctors(RoutingTableChangeFunctor routing_table_change_functor) {
  assert(routing_table_change_functor);
  routing_table_change_functor_ = routing_table_change_functor;
}

bool RoutingTable::AddNode(const NodeInfo& peer) {
  return AddOrCheckNode(peer, true);
}

bool RoutingTable::CheckNode(const NodeInfo& peer) { return AddOrCheckNode(peer, false); }

bool RoutingTable::AddOrCheckNode(NodeInfo peer, bool remove) {
  if (!peer.id.IsValid() || peer.id == kNodeId_) {
    LOG(kError) << "Attempt to add an invalid node " << peer.id;
    return false;
  }
  if (remove && !asymm::ValidateKey(peer.public_key)) {
    return false;
  }

  bool return_value(false);
  NodeInfo removed_node;
  unsigned int routing_table_size(0);
  std::vector<NodeId> old_close_nodes, new_close_nodes;
  std::shared_ptr<CloseNodesChange> close_nodes_change;

  if (remove)
    SetBucketIndex(peer);
  {
    std::unique_lock<std::mutex> lock(mutex_);
    auto found(Find(peer.id, lock));
    if (found.first) {
      return false;
    }

    auto close_nodes_size(PartialSortFromTarget(kNodeId_, Parameters::max_routing_table_size,
                                                lock));

    if (MakeSpaceForNodeToBeAdded(peer, remove, removed_node, lock)) {
      if (remove) {
        assert(peer.bucket != NodeInfo::kInvalidBucket);
        if (!client_mode() &&
           ((nodes_.size() < Parameters::closest_nodes_size)  ||
            NodeId::CloserToTarget(
                peer.id, nodes_.at(Parameters::closest_nodes_size - 1).id, kNodeId()))) {
          bool full_close_nodes(nodes_.size() >= Parameters::closest_nodes_size);
          close_nodes_size = (full_close_nodes) ? (close_nodes_size - 1) : close_nodes_size;
          std::for_each (std::begin(nodes_), std::begin(nodes_) + close_nodes_size,
                         [&](const NodeInfo& node_info) {
                           old_close_nodes.push_back(node_info.id);
                           new_close_nodes.push_back(node_info.id);
                         });
            new_close_nodes.push_back(peer.id);
            if (full_close_nodes)
              old_close_nodes.push_back(nodes_.at(Parameters::closest_nodes_size - 1).id);
          close_nodes_change.reset(new CloseNodesChange(kNodeId(), old_close_nodes,
                                                        new_close_nodes));
        }
        nodes_.push_back(peer);
      }
      return_value = true;
    }
    routing_table_size = static_cast<unsigned int>(nodes_.size());
  }

  if (return_value && remove) {  // Firing functors on Add only
    if (routing_table_change_functor_) {
      routing_table_change_functor_(
          RoutingTableChange(peer, RoutingTableChange::Remove(removed_node, false), true,
                             close_nodes_change, NetworkStatus(routing_table_size)));
    }
  }
  return return_value;
}

NodeInfo RoutingTable::DropNode(const NodeId& node_to_drop, bool routing_only) {
  NodeInfo dropped_node;
  unsigned int routing_table_size(0);
  std::vector<NodeId> old_close_nodes, new_close_nodes;
  std::shared_ptr<CloseNodesChange> close_nodes_change;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    auto close_nodes_size(PartialSortFromTarget(kNodeId_, Parameters::closest_nodes_size + 1,
                                                lock));
    auto found(Find(node_to_drop, lock));
    if (found.first) {
      if (!client_mode() &&
          ((nodes_.size() < Parameters::closest_nodes_size) ||
           !NodeId::CloserToTarget(nodes_.at(Parameters::closest_nodes_size - 1).id,
                                   node_to_drop, kNodeId()))) {
        std::for_each (std::begin(nodes_),
                       std::begin(nodes_) + std::min(close_nodes_size,
                                                     Parameters::closest_nodes_size),
                       [&](const NodeInfo& node_info) {
                         old_close_nodes.push_back(node_info.id);
                         if (node_info.id != node_to_drop)
                           new_close_nodes.push_back(node_info.id);
                       });
        if (close_nodes_size == Parameters::closest_nodes_size + 1)
          new_close_nodes.push_back(nodes_.at(Parameters::closest_nodes_size).id);
        close_nodes_change.reset(new CloseNodesChange(kNodeId(), old_close_nodes, new_close_nodes));
      }
      dropped_node = *found.second;
      nodes_.erase(found.second);
      routing_table_size = static_cast<unsigned int>(nodes_.size());
    }
  }

  if (dropped_node.id.IsValid()) {
    if (routing_table_change_functor_) {
      routing_table_change_functor_(
          RoutingTableChange(NodeInfo(), RoutingTableChange::Remove(dropped_node, routing_only),
                             false, close_nodes_change, NetworkStatus(routing_table_size)));
    }
  }
  return dropped_node;
}

NodeId RoutingTable::RandomConnectedNode() {
  std::unique_lock<std::mutex> lock(mutex_);
// Commenting out assert as peer starts treating this node as joined as soon as it adds
// it into its routing table.
//  assert(nodes_.size() > Parameters::closest_nodes_size &&
//         "Shouldn't call RandomConnectedNode when routing table size is <= closest_nodes_size");
//   assert(nodes_.empty());
  if (nodes_.empty())
    return NodeId();

  PartialSortFromTarget(kNodeId_, static_cast<unsigned int>(nodes_.size()), lock);
  size_t index(RandomUint32() % (nodes_.size()));
  return nodes_.at(index).id;
}

bool RoutingTable::GetNodeInfo(const NodeId& node_id, NodeInfo& peer) const {
  std::unique_lock<std::mutex> lock(mutex_);
  auto found(Find(node_id, lock));
  if (found.first)
    peer = *found.second;
  return found.first;
}

bool RoutingTable::IsThisNodeInRange(const NodeId& target_id, const unsigned int range) {
  // sort by target will always put the node bearing the same target_id (such as pmid_pub_key)
  // as the closest if that node is in the routing table
  std::unique_lock<std::mutex> lock(mutex_);
  if (nodes_.size() < range)
    return true;

  auto count(PartialSortFromTarget(target_id, range + 1, lock));
  bool skip_front(target_id == nodes_[0].id);
  if (skip_front && (count == range))
    return true;
  return NodeId::CloserToTarget(kNodeId_,
                                nodes_[count - 1 - (skip_front ? 0 : 1)].id,
                                target_id);
}

bool RoutingTable::IsThisNodeClosestTo(const NodeId& target_id, bool ignore_exact_match) {
  if (target_id == kNodeId())
    return false;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    if (nodes_.empty())
      return false;
  }

  if (!target_id.IsValid()) {
    LOG(kError) << "Invalid target_id passed.";
    return false;
  }

  NodeInfo closest_node(GetClosestNode(target_id, ignore_exact_match));
  return (closest_node.bucket == NodeInfo::kInvalidBucket) ||
         NodeId::CloserToTarget(kNodeId_, closest_node.id, target_id);
}

bool RoutingTable::Contains(const NodeId& node_id) const {
  std::unique_lock<std::mutex> lock(mutex_);
  return Find(node_id, lock).first;
}

bool RoutingTable::ConfirmGroupMembers(const NodeId& node1, const NodeId& node2) {
  NodeId difference =
      kNodeId_ ^ GetNthClosestNode(
                     kNodeId(),
                     std::min(static_cast<unsigned>(nodes_.size()),
                              static_cast<unsigned>(Parameters::closest_nodes_size))).id;
  return (node1 ^ node2) < difference;
}

// bucket 0 is us, 511 is furthest bucket (should fill first)
void RoutingTable::SetBucketIndex(NodeInfo& node_info) const {
  std::string holder_raw_id(kNodeId_.string());
  std::string node_raw_id(node_info.id.string());
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
  //    return false;
  //  }

  return true;
}

bool RoutingTable::MakeSpaceForNodeToBeAdded(const NodeInfo& node, bool remove,
                                             NodeInfo& removed_node,
                                             std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());

  std::map<uint32_t, unsigned int> bucket_rank_map;

  if (remove && !CheckPublicKeyIsUnique(node, lock))
    return false;

  if (nodes_.size() < kMaxSize_)
    return true;

  if (client_mode()) {
    assert(nodes_.size() == kMaxSize_);
    if (NodeId::CloserToTarget(node.id, nodes_.at(kMaxSize_ - 1).id, kNodeId())) {
      removed_node = *nodes_.rbegin();
      nodes_.pop_back();
      return true;
    } else {
      return false;
    }
  }

  unsigned int max_bucket(0), max_bucket_count(1);
  std::for_each(std::begin(nodes_) + Parameters::unidirectional_interest_range, std::end(nodes_),
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

  // If no duplicate bucket exists, prioirity is given to closer nodes.
  if ((max_bucket_count == 1) && (nodes_.back().bucket < node.bucket))
    return false;

  if (NodeId::CloserToTarget(nodes_.at(Parameters::unidirectional_interest_range).id, node.id,
                             kNodeId()))
    return false;

  for (auto it(nodes_.rbegin()); it != nodes_.rend(); ++it)
    if (static_cast<unsigned int>(it->bucket) == max_bucket) {
      if ((it->bucket != node.bucket) || NodeId::CloserToTarget(node.id, it->id, kNodeId())) {
        if (remove) {
          removed_node = *it;
          nodes_.erase(--(it.base()));
        }
        return true;
      }
    }
  return false;
}

unsigned int RoutingTable::PartialSortFromTarget(const NodeId& target, unsigned int number,
                                                 std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  unsigned int count = std::min(number, static_cast<unsigned int>(nodes_.size()));
  std::partial_sort(nodes_.begin(), nodes_.begin() + count, nodes_.end(),
                    [target](const NodeInfo& lhs, const NodeInfo& rhs) {
    return NodeId::CloserToTarget(lhs.id, rhs.id, target);
  });
  return count;
}

void RoutingTable::NthElementSortFromTarget(const NodeId& target, unsigned int nth_element,
                                            std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  assert((nodes_.size() >= nth_element) &&
         "This should only be called when n is at max the size of RT");
#if defined(__clang__) || defined(MAIDSAFE_WIN32) || \
    (__GNUC__ > 4 || \
    (__GNUC__ == 4 && (__GNUC_MINOR__ > 8 || (__GNUC_MINOR__ ==  8  && __GNUC_PATCHLEVEL__ > 2))))
  std::nth_element(std::begin(nodes_), std::begin(nodes_) + nth_element, std::end(nodes_),
                   [target](const NodeInfo& lhs, const NodeInfo& rhs) {
                     return NodeId::CloserToTarget(lhs.id, rhs.id, target);
                   });
#else
// BEFORE_RELEASE use std::nth_element() for all platform when min required Gcc version is 4.8.3
// http://gcc.gnu.org/bugzilla/show_bug.cgi?id=58800 Bug fixed in gcc 4.8.3
  PartialSortFromTarget(target, nth_element + 1, lock);
#endif
}

NodeInfo RoutingTable::GetClosestNode(const NodeId& target_id, bool ignore_exact_match,
                                      const std::vector<std::string>& exclude) {
  auto closest_nodes(GetClosestNodes(target_id, Parameters::closest_nodes_size,
                                     ignore_exact_match));
  for (const auto& node_info : closest_nodes) {
    if (std::find(exclude.begin(), exclude.end(), node_info.id.string()) == exclude.end())
      return node_info;
  }
  return NodeInfo();
}

std::vector<NodeInfo> RoutingTable::GetClosestNodes(
    const NodeId& target_id, unsigned int number_to_get, bool ignore_exact_match) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (number_to_get == 0)
    return std::vector<NodeInfo>();

  int sorted_count(PartialSortFromTarget(target_id, number_to_get + 1, lock));
  if (sorted_count == 0)
    return std::vector<NodeInfo>();

  if (sorted_count == 1) {
    if (ignore_exact_match)
      return (nodes_.begin()->id == target_id)
                  ? std::vector<NodeInfo>()
                  : std::vector<NodeInfo>(std::begin(nodes_), std::end(nodes_));
    else
      return std::vector<NodeInfo>(std::begin(nodes_), std::end(nodes_));
  }

  unsigned int index(ignore_exact_match && nodes_.begin()->id == target_id);
  return std::vector<NodeInfo>(std::begin(nodes_) + index,
                               std::begin(nodes_) +
                                   std::min(nodes_.size(),
                                            static_cast<size_t>(number_to_get + index)));
}

NodeInfo RoutingTable::GetNthClosestNode(const NodeId& target_id, unsigned int index) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (nodes_.size() < index) {
    NodeInfo node_info;
    node_info.id = NodeInNthBucket(kNodeId(), static_cast<int>(index));
    return node_info;
  }
  NthElementSortFromTarget(target_id, index - 1, lock);
  return nodes_.at(index - 1);
}

std::pair<bool, std::vector<NodeInfo>::iterator> RoutingTable::Find(
    const NodeId& node_id, std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  auto itr(std::find_if(nodes_.begin(), nodes_.end(), [&node_id](const NodeInfo & node_info) {
    return node_info.id == node_id;
  }));
  return std::make_pair(itr != nodes_.end(), itr);
}

std::pair<bool, std::vector<NodeInfo>::const_iterator> RoutingTable::Find(
    const NodeId& node_id, std::unique_lock<std::mutex>& lock) const {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  auto itr(std::find_if(nodes_.begin(), nodes_.end(), [&node_id](const NodeInfo & node_info) {
    return node_info.id == node_id;
  }));
  return std::make_pair(itr != nodes_.end(), itr);
}

unsigned int RoutingTable::NetworkStatus(unsigned int size) const {
  return static_cast<unsigned int>((size) * 100 / kMaxSize_);
}

size_t RoutingTable::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return nodes_.size();
}

// to be moved to utils
//  void RoutingTable::IpcSendCloseNodes() {
//  if (ipc_message_queue_) {
//    network_viewer::MatrixRecord close_nodes_record(kNodeId_);
//    std::vector<NodeInfo> close;
//    {
//      std::unique_lock<std::mutex> lock(mutex_);
//      auto count(PartialSortFromTarget(kNodeId(), Parameters::closest_nodes_size, lock));
//      std::copy(std::begin(nodes_), std::begin(nodes_) + count, std::back_inserter(close));
//    }

//    size_t index(0);
//    std::string printout("\tClose nodes sent by: " + DebugId(kNodeId_) + "\n");
//    size_t limit(std::min(static_cast<size_t>(Parameters::group_size), close.size()));
//    for (; index < limit; ++index) {
//      matrix_record.AddElement(close[index].id, network_viewer::ChildType::kGroup);
//      printout += "\t\t" + DebugId(close[index].id) + " - kGroup\n";
//    }
//    for (; index < close.size(); ++index) {
//      matrix_record.AddElement(close[index].id, network_viewer::ChildType::kClosest);
//      printout += "\t\t" + DebugId(close[index].id) + " - kClosest\n";
//    }
//    std::string serialised_matrix(matrix_record.Serialise());
//    ipc_message_queue_->try_send(serialised_matrix.c_str(), serialised_matrix.size(), 0);
//  }
//  }

std::string RoutingTable::Print() {
  std::vector<NodeInfo> rt;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::sort(nodes_.begin(), nodes_.end(), [&](const NodeInfo& lhs, const NodeInfo& rhs) {
      return NodeId::CloserToTarget(lhs.id, rhs.id, kNodeId_);
    });
    rt = nodes_;
  }
  std::stringstream stream;
  stream << "\n\n[" << kNodeId_ << "] This node's own routing table and peer connections:"
         << "\nRouting table size: " << nodes_.size();
  for (const auto& node : rt) {
    stream << "\n\tPeer [" << node.id << "]--> " << node.connection_id << " && xored "
           << NodeId(kNodeId_ ^ node.id) << " bucket " << node.bucket;
  }
  stream << "\n\n";
  return stream.str();
}

}  // namespace routing

}  // namespace maidsafe
