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
#include <limits>
#include <memory>

#include "maidsafe/common/utils.h"

#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

RoutingTable::RoutingTable(NodeId node_id, asymm::Keys keys)
    : kNodeId_(std::move(node_id)),
      kKeys_(std::move(keys)),
      mutex_(),
      routing_table_change_functor_(),
      nodes_() {}

void RoutingTable::InitialiseFunctors(RoutingTableChangeFunctor routing_table_change_functor) {
  assert(routing_table_change_functor);
  routing_table_change_functor_ = routing_table_change_functor;
}

bool RoutingTable::AddNode(NodeInfo their_info) {
  if (their_info.id.IsZero() || their_info.id == kNodeId_ ||
      !asymm::ValidateKey(their_info.public_key)) {
    return false;
  }

  NodeInfo removed_node;
  std::shared_ptr<CloseNodesChange> close_nodes_change;

  their_info.bucket = BucketIndex(their_info.id);
  bool close_node(true);
  std::lock_guard<std::mutex> lock(mutex_);
  if (nodes_.size() > kGroupSize)
    close_node = (NodeId::CloserToTarget(their_info.id, nodes_.at(kGroupSize).id, kNodeId()));

  if (std::any_of(nodes_.begin(), nodes_.end(), [&their_info](const NodeInfo& node_info) {
        return node_info.id == their_info.id;
      })) {
    return false;
  }
  auto remove_node(MakeSpaceForNodeToBeAdded());
  if ((remove_node != nodes_.rend()) &&
      NodeId::CloserToTarget(their_info.id, remove_node->id, kNodeId_)) {
    removed_node = *(std::next(remove_node).base());
    nodes_.erase(std::next(remove_node).base());
    nodes_.push_back(their_info);
  } else if (close_node || static_cast<size_t>(nodes_.size()) < kRoutingTableSize) {
    nodes_.push_back(their_info);
  } else {
    return false;
  }
  std::sort(nodes_.begin(), nodes_.end(), [&](const NodeInfo& lhs, const NodeInfo& rhs) {
    return NodeId::CloserToTarget(lhs.id, rhs.id, kNodeId_);
  });
  if (routing_table_change_functor_)
    routing_table_change_functor_(
        RoutingTableChange(their_info, RoutingTableChange::Remove(removed_node, false), true,
                           close_nodes_change, NetworkStatus(nodes_.size())));
  return true;
}

bool RoutingTable::CheckNode(const NodeInfo& their_info) {
  if (their_info.id.IsZero() || their_info.id == kNodeId_)
    return false;

//  their_info.bucket = BucketIndex(their_info.id);
  std::lock_guard<std::mutex> lock(mutex_);
  if (std::any_of(nodes_.begin(), nodes_.end(), [&their_info](const NodeInfo& node_info) {
        return node_info.id == their_info.id;
      })) {
    return false;
  }
  if (nodes_.size() < kRoutingTableSize)
    return true;
  bool close_node(NodeId::CloserToTarget(their_info.id, nodes_.at(kGroupSize).id, kNodeId_));
  auto remove_node(MakeSpaceForNodeToBeAdded());
  return ((remove_node != nodes_.rend() &&
           NodeId::CloserToTarget(their_info.id, remove_node->id, kNodeId_)) ||
          (nodes_.size() < kRoutingTableSize - 1) || close_node);
}

NodeInfo RoutingTable::DropNode(const NodeId& node_to_drop, bool routing_only) {
  bool removed(false);
  NodeInfo dropped_node;
  std::size_t size(0);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto remove =
        find_if(std::begin(nodes_), std::end(nodes_),
                [&node_to_drop](const NodeInfo& node) { return node.id == node_to_drop; });
    if (remove != std::end(nodes_)) {
      dropped_node = std::move(*remove);
      nodes_.erase(remove);
      removed = true;
    }
    size = nodes_.size();
  }
  if (removed) {
    if (routing_table_change_functor_) {
      std::shared_ptr<CloseNodesChange> tmp;
      routing_table_change_functor_(
          RoutingTableChange(NodeInfo(), RoutingTableChange::Remove(dropped_node, routing_only),
                             false, tmp, NetworkStatus(size)));
    }
  }
  return dropped_node;
}

std::vector<NodeInfo> RoutingTable::GetTargetNodes(const NodeId& their_id) const {
  NodeId test_node(kNodeId_);
  size_t count(0), index(0);
  std::vector<NodeInfo> result;
  result.reserve(kGroupSize);
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& node : nodes_) {
    if (NodeId::CloserToTarget(node.id, test_node, their_id)) {
      test_node = node.id;
      index = count;
    }
    ++count;
  }
  if (index < kGroupSize) {
    auto size = std::min(kGroupSize, nodes_.size());
    std::copy(std::begin(nodes_), std::begin(nodes_) + size, std::begin(result));
  } else {
    result.push_back(nodes_.at(index));
  }
  return result;
}

std::vector<NodeInfo> RoutingTable::GetGroupNodes() const {
  std::vector<NodeInfo> result;
  result.reserve(kGroupSize);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto size = std::min(kGroupSize, static_cast<size_t>(nodes_.size()));
    std::copy(std::begin(nodes_), std::begin(nodes_) + size, std::begin(result));
  }
  return result;
}

size_t RoutingTable::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return nodes_.size();
}

// bucket 0 is us, 511 is furthest bucket (should fill first)
int32_t RoutingTable::BucketIndex(const NodeId& node_id) const {
  assert(node_id != kNodeId_);
  return (NodeId::kSize * 8) - 1 - kNodeId_.CommonLeadingBits(node_id);
}

std::vector<NodeInfo>::reverse_iterator RoutingTable::MakeSpaceForNodeToBeAdded() {
  size_t bucket_count(0);
  int bucket(0);
  if (nodes_.size() < kRoutingTableSize - 1)
    return nodes_.rend();
  auto found = std::find_if(nodes_.rbegin(), nodes_.rend() + kGroupSize,
                            [&bucket_count, &bucket](const NodeInfo& node) {
    if (node.bucket != bucket) {
      bucket = node.bucket;
      bucket_count = 0;
    }
    return (++bucket_count > kBucketSize_);
  });
  if (found != nodes_.rend() + kGroupSize)
    return found;
  else
    return nodes_.rend();
}

unsigned int RoutingTable::NetworkStatus(size_t size) const {
  return static_cast<unsigned int>((size) * 100 / kRoutingTableSize);
}

}  // namespace routing

}  // namespace maidsafe
