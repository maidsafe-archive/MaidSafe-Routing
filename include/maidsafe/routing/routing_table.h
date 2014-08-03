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

#ifndef MAIDSAFE_ROUTING_ROUTING_TABLE_H_
#define MAIDSAFE_ROUTING_ROUTING_TABLE_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "boost/asio/ip/udp.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/interprocess/ipc/message_queue.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/rsa.h"

#include "maidsafe/passport/types.h"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/types.h"


namespace maidsafe {

namespace routing {

namespace test {
class GenericNode;
template <typename NodeType>
class RoutingTableTest;
class RoutingTableTest_BEH_CheckMockSendGroupChangeRpcs_Test;
class NetworkStatisticsTest_BEH_IsIdInGroupRange_Test;
class RoutingTableTest_FUNC_IsNodeIdInGroupRange_Test;
}

struct NodeInfo;

struct RoutingTableChange {
  struct Remove {
    Remove() : node(), routing_only_removal(true) {}
    Remove(NodeInfo& node_in, bool routing_only_removal_in)
        : node(node_in), routing_only_removal(routing_only_removal_in) {}
    NodeInfo node;
    bool routing_only_removal;
  };
  RoutingTableChange()
      : added_node(), removed(), insertion(false), close_nodes_change(), health(0) {}
  RoutingTableChange(const NodeInfo& added_node_in, const Remove& removed_in, bool insertion_in,
                     std::shared_ptr<CloseNodesChange> close_nodes_change_in, uint16_t health_in)
      : added_node(added_node_in),
        removed(removed_in),
        insertion(insertion_in),
        close_nodes_change(close_nodes_change_in),
        health(health_in) {}
  NodeInfo added_node;
  Remove removed;
  bool insertion;
  std::shared_ptr<CloseNodesChange> close_nodes_change;
  uint16_t health;
};

typedef std::function<void(const RoutingTableChange& /*routing_table_change*/)>
    RoutingTableChangeFunctor;

template <typename NodeType>
class RoutingTable {
 public:
  typedef NodeType Type;
  RoutingTable(const LocalNodeId& node_id, const asymm::Keys& keys);
  virtual ~RoutingTable();
  void InitialiseFunctors(RoutingTableChangeFunctor routing_table_change_functor);
  bool AddNode(const NodeInfo& peer);
  bool CheckNode(const NodeInfo& peer);
  NodeInfo DropNode(const NodeId& node_to_drop, bool routing_only);

  bool IsThisNodeInRange(const NodeId& target_id, uint16_t range);
  bool IsThisNodeClosestTo(const NodeId& target_id, bool ignore_exact_match = false);
  bool Contains(const NodeId& node_id) const;
  bool ConfirmGroupMembers(const NodeId& node1, const NodeId& node2);

  bool GetNodeInfo(const NodeId& node_id, NodeInfo& node_info) const;
  // Returns default-constructed NodeId if routing table size is zero
  NodeInfo GetClosestNode(const NodeId& target_id, bool ignore_exact_match = false,
                          const std::vector<std::string>& exclude = std::vector<std::string>());
  std::vector<NodeInfo> GetClosestNodes(const NodeId& target_id, uint16_t number_to_get,
                                        bool ignore_exact_match = false);
  NodeInfo GetNthClosestNode(const NodeId& target_id, uint16_t index);
  NodeId RandomConnectedNode();

  size_t size() const;
  LocalNodeId kNodeId() const { return kNodeId_; }
  asymm::PrivateKey kPrivateKey() const { return kKeys_.private_key; }
  asymm::PublicKey kPublicKey() const { return kKeys_.public_key; }
  LocalConnectionId kConnectionId() const { return kConnectionId_; }

  friend class test::GenericNode;
  template <typename Type>
  friend class test::RoutingTableTest;
  friend class test::RoutingTableTest_BEH_CheckMockSendGroupChangeRpcs_Test;
  friend class test::NetworkStatisticsTest_BEH_IsIdInGroupRange_Test;
  friend class test::RoutingTableTest_FUNC_IsNodeIdInGroupRange_Test;

 private:
  RoutingTable(const RoutingTable&);
  RoutingTable& operator=(const RoutingTable&);
  bool AddOrCheckNode(NodeInfo node, bool remove);
  void SetBucketIndex(NodeInfo& node_info) const;
  bool CheckPublicKeyIsUnique(const NodeInfo& node, std::unique_lock<std::mutex>& lock) const;

  /** Attempts to find or allocate memory for an incomming connect request, returning true
   * indicates approval
   * returns true if routing table is not full, otherwise, performs the following process to
   * possibly evict an existing node:
   * - sorts the nodes according to their distance from self-node-id
   * - a candidate for eviction must have an index > Parameters::unidirectional_interest_range
   * - count the number of nodes in each bucket for nodes with
   *    index > Parameters::unidirectional_interest_range
   * - choose the furthest node among the nodes with maximum bucket index
   * - in case more than one bucket have similar maximum bucket size, the furthest node in higher
   *    bucket will be evicted
   * - remove the selected node and return true **/
  bool MakeSpaceForNodeToBeAdded(const NodeInfo& node, bool remove, NodeInfo& removed_node,
                                 std::unique_lock<std::mutex>& lock);

  uint16_t PartialSortFromTarget(const NodeId& target, uint16_t number,
                                 std::unique_lock<std::mutex>& lock);
  void NthElementSortFromTarget(const NodeId& target, uint16_t nth_element,
                                std::unique_lock<std::mutex>& lock);
  std::pair<bool, std::vector<NodeInfo>::iterator> Find(const NodeId& node_id,
                                                        std::unique_lock<std::mutex>& lock);
  std::pair<bool, std::vector<NodeInfo>::const_iterator> Find(
      const NodeId& node_id, std::unique_lock<std::mutex>& lock) const;

  unsigned int NetworkStatus(unsigned int size) const;

  void IpcSendCloseNodes();
  std::string PrintRoutingTable();

  const LocalNodeId kNodeId_;
  const LocalConnectionId kConnectionId_;
  const asymm::Keys kKeys_;
  mutable std::mutex mutex_;
  RoutingTableChangeFunctor routing_table_change_functor_;
  std::vector<NodeInfo> nodes_;
};

template <typename NodeType>
RoutingTable<NodeType>::RoutingTable(const LocalNodeId& node_id, const asymm::Keys& keys)
    : kNodeId_(node_id),
      kConnectionId_(kNodeId_),
      kKeys_(keys),
      mutex_(),
      routing_table_change_functor_(),
      nodes_() {}

template <>
RoutingTable<ClientNode>::RoutingTable(const LocalNodeId& node_id, const asymm::Keys& keys);


template <typename NodeType>
RoutingTable<NodeType>::~RoutingTable() {}

template <typename NodeType>
void RoutingTable<NodeType>::InitialiseFunctors(
    RoutingTableChangeFunctor routing_table_change_functor) {
  assert(routing_table_change_functor);
  routing_table_change_functor_ = routing_table_change_functor;
}

template <typename NodeType>
bool RoutingTable<NodeType>::AddNode(const NodeInfo& peer) {
  return AddOrCheckNode(peer, true);
}

template <typename NodeType>
bool RoutingTable<NodeType>::CheckNode(const NodeInfo& peer) {
  return AddOrCheckNode(peer, false);
}

template <typename NodeType>
bool RoutingTable<NodeType>::AddOrCheckNode(NodeInfo peer, bool remove) {
  if (peer.id.IsZero() || peer.id == kNodeId_) {
    LOG(kError) << "Attempt to add an invalid node " << peer.id;
    return false;
  }
  if (remove && !asymm::ValidateKey(peer.public_key)) {
    LOG(kInfo) << "Invalid public key for node " << DebugId(peer.id);
    return false;
  }

  bool return_value(false);
  NodeInfo removed_node;
  uint16_t routing_table_size(0);
  std::vector<NodeId> old_close_nodes, new_close_nodes;
  std::shared_ptr<CloseNodesChange> close_nodes_change;

  if (remove)
    SetBucketIndex(peer);
  {
    std::unique_lock<std::mutex> lock(mutex_);
    auto found(Find(peer.id, lock));
    if (found.first) {
      LOG(kVerbose) << "Node " << peer.id << " already in routing table.";
      return false;
    }

    auto close_nodes_size(
        PartialSortFromTarget(kNodeId_, Parameters::max_routing_table_size, lock));

    if (MakeSpaceForNodeToBeAdded(peer, remove, removed_node, lock)) {
      if (remove) {
        assert(peer.bucket != NodeInfo::kInvalidBucket);
        if (((nodes_.size() < Parameters::closest_nodes_size) ||
             NodeId::CloserToTarget(peer.id, nodes_.at(Parameters::closest_nodes_size - 1).id,
                                    kNodeId()))) {
          bool full_close_nodes(nodes_.size() >= Parameters::closest_nodes_size);
          close_nodes_size = (full_close_nodes) ? (close_nodes_size - 1) : close_nodes_size;
          std::for_each(std::begin(nodes_), std::begin(nodes_) + close_nodes_size,
                        [&](const NodeInfo& node_info) {
            old_close_nodes.push_back(node_info.id);
            new_close_nodes.push_back(node_info.id);
          });
          new_close_nodes.push_back(peer.id);
          if (full_close_nodes)
            old_close_nodes.push_back(nodes_.at(Parameters::closest_nodes_size - 1).id);
          close_nodes_change.reset(
              new CloseNodesChange(kNodeId(), old_close_nodes, new_close_nodes));
        }
        nodes_.push_back(peer);
      }
      return_value = true;
    }
    routing_table_size = static_cast<uint16_t>(nodes_.size());
  }

  if (return_value && remove) {  // Firing functors on Add only
    if (routing_table_change_functor_) {
      routing_table_change_functor_(
          RoutingTableChange(peer, RoutingTableChange::Remove(removed_node, false), true,
                             close_nodes_change, NetworkStatus(routing_table_size)));
    }

    if (peer.nat_type == rudp::NatType::kOther) {  // Usable as bootstrap endpoint
                                                   // if (new_bootstrap_endpoint_)
                                                   // new_bootstrap_endpoint_(peer.endpoint);
    }
    LOG(kInfo) << PrintRoutingTable();
  }
  return return_value;
}

template <>
bool RoutingTable<ClientNode>::AddOrCheckNode(NodeInfo peer, bool remove);

template <typename NodeType>
NodeInfo RoutingTable<NodeType>::DropNode(const NodeId& node_to_drop, bool routing_only) {
  NodeInfo dropped_node;
  uint16_t routing_table_size(0);
  std::vector<NodeId> old_close_nodes, new_close_nodes;
  std::shared_ptr<CloseNodesChange> close_nodes_change;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    auto close_nodes_size(
        PartialSortFromTarget(kNodeId_, Parameters::closest_nodes_size + 1, lock));
    auto found(Find(node_to_drop, lock));
    if (found.first) {
      if (((nodes_.size() < Parameters::closest_nodes_size) ||
           !NodeId::CloserToTarget(nodes_.at(Parameters::closest_nodes_size - 1).id, node_to_drop,
                                   kNodeId()))) {
        std::for_each(std::begin(nodes_),
                      std::begin(nodes_) +
                          std::min(close_nodes_size, Parameters::closest_nodes_size),
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
      routing_table_size = static_cast<uint16_t>(nodes_.size());
    }
  }

  if (!dropped_node.id.IsZero()) {
    if (routing_table_change_functor_) {
      routing_table_change_functor_(
          RoutingTableChange(NodeInfo(), RoutingTableChange::Remove(dropped_node, routing_only),
                             false, close_nodes_change, NetworkStatus(routing_table_size)));
    }
  }
  LOG(kInfo) << PrintRoutingTable();
  return dropped_node;
}

template <>
NodeInfo RoutingTable<ClientNode>::DropNode(const NodeId& node_to_drop, bool routing_only);

template <typename NodeType>
NodeId RoutingTable<NodeType>::RandomConnectedNode() {
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
  return nodes_.at(index).id;
}

template <typename NodeType>
bool RoutingTable<NodeType>::GetNodeInfo(const NodeId& node_id, NodeInfo& peer) const {
  std::unique_lock<std::mutex> lock(mutex_);
  auto found(Find(node_id, lock));
  if (found.first)
    peer = *found.second;
  return found.first;
}

template <typename NodeType>
bool RoutingTable<NodeType>::IsThisNodeInRange(const NodeId& target_id, const uint16_t range) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (nodes_.size() < range)
    return true;
  NthElementSortFromTarget(kNodeId_, range, lock);
  return NodeId::CloserToTarget(target_id, nodes_[range - 1].id, kNodeId_);
}

template <typename NodeType>
bool RoutingTable<NodeType>::IsThisNodeClosestTo(const NodeId& target_id, bool ignore_exact_match) {
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
         NodeId::CloserToTarget(kNodeId_, closest_node.id, target_id);
}

template <typename NodeType>
bool RoutingTable<NodeType>::Contains(const NodeId& node_id) const {
  std::unique_lock<std::mutex> lock(mutex_);
  return Find(node_id, lock).first;
}

template <typename NodeType>
bool RoutingTable<NodeType>::ConfirmGroupMembers(const NodeId& node1, const NodeId& node2) {
  NodeId difference =
      kNodeId_ ^ GetNthClosestNode(kNodeId(), std::min(static_cast<uint16_t>(nodes_.size()),
                                                       Parameters::closest_nodes_size)).id;
  return (node1 ^ node2) < difference;
}

// bucket 0 is us, 511 is furthest bucket (should fill first)
template <typename NodeType>
void RoutingTable<NodeType>::SetBucketIndex(NodeInfo& node_info) const {
  std::string holder_raw_id(kNodeId_->string());
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

template <typename NodeType>
bool RoutingTable<NodeType>::CheckPublicKeyIsUnique(const NodeInfo& node,
                                                    std::unique_lock<std::mutex>& lock) const {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  // If we already have a duplicate public key return false
  if (std::find_if(nodes_.begin(), nodes_.end(), [node](const NodeInfo& node_info) {
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

template <typename NodeType>
bool RoutingTable<NodeType>::MakeSpaceForNodeToBeAdded(const NodeInfo& node, bool remove,
                                                       NodeInfo& removed_node,
                                                       std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());

  std::map<uint32_t, uint16_t> bucket_rank_map;

  if (remove && !CheckPublicKeyIsUnique(node, lock))
    return false;

  if (nodes_.size() < Params<NodeType>::max_routing_table_size)
    return true;

  int32_t max_bucket(0), max_bucket_count(1);
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

  LOG(kVerbose) << "[" << kNodeId_ << "] max_bucket " << max_bucket << " count "
                << max_bucket_count;

  // If no duplicate bucket exists, prioirity is given to closer nodes.
  if ((max_bucket_count == 1) && (nodes_.back().bucket < node.bucket))
    return false;

  for (auto it(nodes_.rbegin()); it != nodes_.rend(); ++it)
    if (it->bucket == max_bucket) {
      if (remove) {
        removed_node = *it;
        nodes_.erase(--(it.base()));
        LOG(kVerbose) << kNodeId_ << " Proposed removable " << removed_node.id;
      }
      return true;
    }
  return false;
}

template <>
bool RoutingTable<ClientNode>::MakeSpaceForNodeToBeAdded(const NodeInfo& node, bool remove,
                                                         NodeInfo& removed_node,
                                                         std::unique_lock<std::mutex>& lock);


template <typename NodeType>
uint16_t RoutingTable<NodeType>::PartialSortFromTarget(const NodeId& target, uint16_t number,
                                                       std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  uint16_t count = std::min(number, static_cast<uint16_t>(nodes_.size()));
  std::partial_sort(nodes_.begin(), nodes_.begin() + count, nodes_.end(),
                    [target](const NodeInfo& lhs, const NodeInfo& rhs) {
    return NodeId::CloserToTarget(lhs.id, rhs.id, target);
  });
  return count;
}

template <typename NodeType>
void RoutingTable<NodeType>::NthElementSortFromTarget(const NodeId& target, uint16_t nth_element,
                                                      std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  assert((nodes_.size() >= nth_element) &&
         "This should only be called when n is at max the size of RT");
#ifndef __GNUC__
  std::nth_element(nodes_.begin(), nodes_.begin() + nth_element - 1, nodes_.end(),
                   [target](const NodeInfo& lhs, const NodeInfo& rhs) {
    return NodeId::CloserToTarget(lhs.id, rhs.id, target);
  });
#else
  // BEFORE_RELEASE use std::nth_element() for all platform when min required Gcc version is 4.8.3
  // http://gcc.gnu.org/bugzilla/show_bug.cgi?id=58800 Bug fixed in gcc 4.8.3
  std::partial_sort(nodes_.begin(), nodes_.begin() + nth_element - 1, nodes_.end(),
                    [target](const NodeInfo& lhs, const NodeInfo& rhs) {
    return NodeId::CloserToTarget(lhs.id, rhs.id, target);
  });
#endif
}

template <typename NodeType>
NodeInfo RoutingTable<NodeType>::GetClosestNode(const NodeId& target_id, bool ignore_exact_match,
                                                const std::vector<std::string>& exclude) {
  auto closest_nodes(
      GetClosestNodes(target_id, Parameters::closest_nodes_size, ignore_exact_match));
  for (const auto& node_info : closest_nodes) {
    if (std::find(exclude.begin(), exclude.end(), node_info.id.string()) == exclude.end())
      return node_info;
  }
  return NodeInfo();
}

template <typename NodeType>
std::vector<NodeInfo> RoutingTable<NodeType>::GetClosestNodes(const NodeId& target_id,
                                                              uint16_t number_to_get,
                                                              bool ignore_exact_match) {
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

  uint16_t index(ignore_exact_match && nodes_.begin()->id == target_id);
  return std::vector<NodeInfo>(
      std::begin(nodes_) + index,
      std::begin(nodes_) + std::min(nodes_.size(), static_cast<size_t>(number_to_get + index)));
}

template <typename NodeType>
NodeInfo RoutingTable<NodeType>::GetNthClosestNode(const NodeId& target_id, uint16_t index) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (nodes_.size() < index) {
    NodeInfo node_info;
    node_info.id = NodeInNthBucket(kNodeId(), static_cast<int>(index));
    return node_info;
  }
  NthElementSortFromTarget(target_id, index, lock);
  return nodes_.at(index - 1);
}

template <typename NodeType>
std::pair<bool, std::vector<NodeInfo>::iterator> RoutingTable<NodeType>::Find(
    const NodeId& node_id, std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  auto itr(std::find_if(nodes_.begin(), nodes_.end(),
                        [&node_id](const NodeInfo& node_info) { return node_info.id == node_id; }));
  return std::make_pair(itr != nodes_.end(), itr);
}

template <typename NodeType>
std::pair<bool, std::vector<NodeInfo>::const_iterator> RoutingTable<NodeType>::Find(
    const NodeId& node_id, std::unique_lock<std::mutex>& lock) const {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  auto itr(std::find_if(nodes_.begin(), nodes_.end(),
                        [&node_id](const NodeInfo& node_info) { return node_info.id == node_id; }));
  return std::make_pair(itr != nodes_.end(), itr);
}

template <typename NodeType>
unsigned int RoutingTable<NodeType>::NetworkStatus(unsigned int size) const {
  return size * 100 / Params<NodeType>::max_routing_table_size;
}

template <typename NodeType>
size_t RoutingTable<NodeType>::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return nodes_.size();
}

template <typename NodeType>
std::string RoutingTable<NodeType>::PrintRoutingTable() {
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

#endif  // MAIDSAFE_ROUTING_ROUTING_TABLE_H_
