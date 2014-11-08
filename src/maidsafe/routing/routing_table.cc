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

bool RoutingTable::AddNode(NodeInfo peer) {
  if (!peer.id.IsValid() || peer.id == kNodeId_ || !asymm::ValidateKey(peer.public_key))
    return false;

  NodeInfo removed_node;
  std::shared_ptr<CloseNodesChange> close_nodes_change;

  peer.bucket = BucketIndex(peer.id);
  bool close_node(true);
  std::lock_guard<std::mutex> lock(mutex_);
  if (nodes_.size() > kGroupSize)
    close_node = (NodeId::CloserToTarget(peer.id, nodes_.at(kGroupSize).id, kNodeId()));

  if (std::any_of(nodes_.begin(), nodes_.end(),
                  [&peer](const NodeInfo& node_info) { return node_info.id == peer.id; }))
    return false;
  auto remove_node(MakeSpaceForNodeToBeAdded());

  if (static_cast<size_t>(nodes_.size()) < kRoutingTableSize) {
    nodes_.push_back(peer);
  } else if ((remove_node != nodes_.rend()) &&
             peer.bucket > (std::next(remove_node).base())->bucket) {
    removed_node = *(std::next(remove_node).base());
    nodes_.erase(std::next(remove_node).base());
    nodes_.push_back(peer);
  } else if (close_node) {
    // try to push another node out here as new node is also a close node
    // rather than removing the old close node we keep it if possible and
    // sacrifice a less importnt node
    auto remove_node(MakeSpaceForNodeToBeAdded());
    if (remove_node != nodes_.rend())
      nodes_.erase(std::next(remove_node).base());
    nodes_.push_back(peer);
  } else {
    return false;
  }
  std::sort(nodes_.begin(), nodes_.end(), [&](const NodeInfo& lhs, const NodeInfo& rhs) {
    return NodeId::CloserToTarget(lhs.id, rhs.id, kNodeId_);
  });
  if (routing_table_change_functor_)
    routing_table_change_functor_(
        RoutingTableChange(peer, RoutingTableChange::Remove(removed_node, false), true,
                           close_nodes_change, NetworkStatus(nodes_.size())));
  return true;
}

bool RoutingTable::CheckNode(NodeInfo peer) {
  if (!peer.id.IsValid() || peer.id == kNodeId_)
    return false;

  std::lock_guard<std::mutex> lock(mutex_);

  if (nodes_.size() < kRoutingTableSize)
    return true;
  // check for duplicates
  if (std::any_of(nodes_.begin(), nodes_.end(),
                  [&peer](const NodeInfo& node_info) { return node_info.id == peer.id; }))
    return false;
  // close node
  if (NodeId::CloserToTarget(peer.id, nodes_.at(kGroupSize).id, kNodeId()))
    return true;
  // this node is a better fot than we currently have in the routing table
  auto remove_node(MakeSpaceForNodeToBeAdded());
  return (remove_node != nodes_.rend() &&
          BucketIndex(peer.id) > std::next(remove_node).base()->bucket);
}

NodeInfo RoutingTable::DropNode(const NodeId& node_to_drop, bool routing_only) {
  bool removed(false);
  NodeInfo dropped_node;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto remove =
        find_if(std::begin(nodes_), std::end(nodes_),
                [&node_to_drop](const NodeInfo& node) { return node.id == node_to_drop; });
    if (remove != std::end(nodes_)) {
      dropped_node = *remove;
      nodes_.erase(remove);
      removed = true;
    }
  }
  if (removed) {
    if (routing_table_change_functor_) {
      std::shared_ptr<CloseNodesChange> tmp;
      routing_table_change_functor_(
          RoutingTableChange(NodeInfo(), RoutingTableChange::Remove(dropped_node, routing_only),
                             false, tmp, NetworkStatus(size())));
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

// bucket 511 is us, 0 is furthest bucket (should fill first)
int32_t RoutingTable::BucketIndex(const NodeId& node_id) const {
  return kNodeId_.CommonLeadingBits(node_id);
}

std::vector<NodeInfo>::reverse_iterator RoutingTable::MakeSpaceForNodeToBeAdded() {
  size_t bucket_count(0);
  int bucket(0);
  if (nodes_.size() < kRoutingTableSize)
    return nodes_.rend();
  auto found = std::find_if(nodes_.rbegin(), nodes_.rbegin() + kGroupSize,
                            [&bucket_count, &bucket](const NodeInfo& node) {
    if (node.bucket != bucket) {
      bucket = node.bucket;
      bucket_count = 0;
    }
    return (++bucket_count > kBucketSize);
  });
  if (found < nodes_.rbegin() + kGroupSize)
    return found;
  else
    return nodes_.rend();
}

unsigned int RoutingTable::NetworkStatus(unsigned int size) const {
  return static_cast<unsigned int>((size)*100 / kRoutingTableSize);
}


}  // namespace routing

}  // namespace maidsafe
