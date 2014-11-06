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

#include "maidsafe/routing/types.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/close_nodes_change.h"

namespace maidsafe {

namespace routing {

RoutingTable::RoutingTable(const NodeId& node_id, const asymm::Keys& keys)
    : kNodeId_(node_id), kKeys_(keys), mutex_(), routing_table_change_functor_(), nodes_() {}

void RoutingTable::InitialiseFunctors(RoutingTableChangeFunctor routing_table_change_functor) {
  assert(routing_table_change_functor);
  routing_table_change_functor_ = routing_table_change_functor;
}

bool RoutingTable::AddNode(const NodeInfo& peer) { return AddOrCheckNode(peer, true); }

bool RoutingTable::CheckNode(const NodeInfo& peer) { return AddOrCheckNode(peer, false); }

bool RoutingTable::AddOrCheckNode(NodeInfo peer, bool remove) {
  if (peer.id.IsZero() || peer.id == kNodeId_) {
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

    if (MakeSpaceForNodeToBeAdded(peer, remove, removed_node, lock)) {
      if (remove) {
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
    auto found(Find(node_to_drop, lock));
    if (found.first) {
      dropped_node = *found.second;
      nodes_.erase(found.second);
      routing_table_size = static_cast<unsigned int>(nodes_.size());
    }
  }

  if (!dropped_node.id.IsZero()) {
    if (routing_table_change_functor_) {
      routing_table_change_functor_(
          RoutingTableChange(NodeInfo(), RoutingTableChange::Remove(dropped_node, routing_only),
                             false, close_nodes_change, NetworkStatus(routing_table_size)));
    }
  }
  return dropped_node;
}

std::vector<NodeInfo> RoutingTable::GetTargetNodes(NodeId their_id) {
  NodeId test_node(kNodeId_);
  auto count(0);
  auto index(0);
  std::vector<NodeInfo> return_vec;
  return_vec.reserve(kGroupSize);
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& node : nodes_)
    if (NodeId::CloserToTarget(node.id, test_node, their_id)) {
      test_node = node.id;
      index = count;
    }
  if (static_cast<size_t>(index) < kGroupSize) {
    auto size = std::min(kGroupSize, static_cast<size_t>(nodes_.size()));
    std::copy(std::begin(nodes_), std::begin(nodes_) + size, std::begin(return_vec));
  } else {
    return_vec.push_back(nodes_.at(index));
  }
  return return_vec;
}

std::vector<NodeInfo> RoutingTable::GetGroupNodes() {
  std::vector<NodeInfo> return_vec;
  return_vec.reserve(kGroupSize);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto size = std::min(kGroupSize, static_cast<size_t>(nodes_.size()));
    std::copy(std::begin(nodes_), std::begin(nodes_) + size, std::begin(return_vec));
  }
  return return_vec;
}

size_t RoutingTable::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return nodes_.size();
}

// ################## Private ###################

// bucket 0 is us, 511 is furthest bucket (should fill first)
void RoutingTable::SetBucketIndex(NodeInfo& node_info) const {
  node_info.bucket = NodeId::kSize - 1 - kNodeId_.CommonLeadingBits(node_info.id);
}

bool RoutingTable::MakeSpaceForNodeToBeAdded(const NodeInfo& node, bool remove,
                                             NodeInfo& removed_node,
                                             std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());

  std::map<uint32_t, unsigned int> bucket_rank_map;

  if (nodes_.size() < kRoutingTableSize)
    return true;


  unsigned int max_bucket(0), max_bucket_count(1);
  std::for_each(std::begin(nodes_) + kGroupSize, std::end(nodes_),
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

  if (NodeId::CloserToTarget(nodes_.at(kGroupSize).id, node.id, kNodeId()))
    return false;

  for (auto it(nodes_.rbegin()); it != nodes_.rend(); ++it)
    if (static_cast<unsigned int>(it->bucket) == max_bucket) {
      if ((it->bucket != node.bucket) || NodeId::CloserToTarget(node.id, it->id, kNodeId())) {
        if (remove) {
          removed_node = *it;
          nodes_.erase(--(it.base()));
        }
        std::sort(nodes_.begin(), nodes_.end(), [&](const NodeInfo& lhs, const NodeInfo& rhs) {
          return NodeId::CloserToTarget(lhs.id, rhs.id, kNodeId_);
        });
        return true;
      }
    }
  return false;
}

std::pair<bool, std::vector<NodeInfo>::iterator> RoutingTable::Find(
    const NodeId& node_id, std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  auto itr(std::find_if(nodes_.begin(), nodes_.end(),
                        [&node_id](const NodeInfo& node_info) { return node_info.id == node_id; }));
  return std::make_pair(itr != nodes_.end(), itr);
}

std::pair<bool, std::vector<NodeInfo>::const_iterator> RoutingTable::Find(
    const NodeId& node_id, std::unique_lock<std::mutex>& lock) const {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  auto itr(std::find_if(nodes_.begin(), nodes_.end(),
                        [&node_id](const NodeInfo& node_info) { return node_info.id == node_id; }));
  return std::make_pair(itr != nodes_.end(), itr);
}

unsigned int RoutingTable::NetworkStatus(unsigned int size) const {
  return static_cast<unsigned int>((size)*100 / kRoutingTableSize);
}


std::string RoutingTable::PrintRoutingTable() {
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
