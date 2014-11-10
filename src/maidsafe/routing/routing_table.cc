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
#include <map>
#include <sstream>
#include <iostream>

#include "maidsafe/common/utils.h"

#include "maidsafe/routing/types.h"
#include "maidsafe/routing/node_info.h"

namespace maidsafe {

namespace routing {

routing_table::routing_table(const NodeId& node_id, const asymm::Keys& keys)
    : our_id_(node_id), kKeys_(keys), mutex_(), nodes_() {}


bool routing_table::add_node(node_info peer) {
  if (!peer.id.IsValid() || peer.id == our_id_ || !asymm::ValidateKey(peer.public_key))
    return false;

  node_info removed_node;
  std::shared_ptr<CloseNodesChange> close_nodes_change;

  peer.bucket = bucket_index(peer.id);
  bool close_node(true);
  std::lock_guard<std::mutex> lock(mutex_);
  if (nodes_.size() > kGroupSize)
    close_node = (NodeId::CloserToTarget(peer.id, nodes_.at(kGroupSize).id, our_id()));

  if (std::any_of(nodes_.begin(), nodes_.end(),
                  [&peer](const node_info& node_info) { return node_info.id == peer.id; }))
    return false;
  auto remove_node(is_node_viable_for_routing_table());

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
    auto remove_node(is_node_viable_for_routing_table());
    if (remove_node != nodes_.rend())
      nodes_.erase(std::next(remove_node).base());
    nodes_.push_back(peer);
  } else {
    return false;
  }
  std::sort(nodes_.begin(), nodes_.end(), [&](const node_info& lhs, const node_info& rhs) {
    return NodeId::CloserToTarget(lhs.id, rhs.id, our_id_);
  });
  return true;
}

bool routing_table::check_node(node_info peer) {
  if (!peer.id.IsValid() || peer.id == our_id_)
    return false;

  std::lock_guard<std::mutex> lock(mutex_);

  if (nodes_.size() < kRoutingTableSize)
    return true;
  // check for duplicates
  if (std::any_of(nodes_.begin(), nodes_.end(),
                  [&peer](const node_info& node_info) { return node_info.id == peer.id; }))
    return false;
  // close node
  if (NodeId::CloserToTarget(peer.id, nodes_.at(kGroupSize).id, our_id()))
    return true;
  // this node is a better fot than we currently have in the routing table
  auto remove_node(is_node_viable_for_routing_table());
  return (remove_node != nodes_.rend() &&
          bucket_index(peer.id) > std::next(remove_node).base()->bucket);
}

bool routing_table::drop_node(const NodeId& node_to_drop) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto remove =
        find_if(std::begin(nodes_), std::end(nodes_),
                [&node_to_drop](const node_info& node) { return node.id == node_to_drop; });
    if (remove != std::end(nodes_)) {
      nodes_.erase(remove);
      return true;
    }
  }
  return false;
}

std::vector<node_info> routing_table::target_nodes(NodeId their_id) {
  NodeId test_node(our_id_);
  auto count(0);
  auto index(0);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& node : nodes_) {
      if (NodeId::CloserToTarget(node.id, test_node, their_id)) {
        test_node = node.id;
        index = count;
      }
      ++count;
    }
  }

  if (static_cast<size_t>(index) < kGroupSize) {
    return our_close_group();
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return {nodes_.at(index)};
  }
}

std::vector<node_info> routing_table::our_close_group() {
  std::vector<node_info> return_vec;
  return_vec.reserve(kGroupSize);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto size = std::min(kGroupSize, static_cast<size_t>(nodes_.size()));
    std::copy(std::begin(nodes_), std::begin(nodes_) + size, std::back_inserter(return_vec));
  }
  return return_vec;
}

size_t routing_table::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return nodes_.size();
}

// ################## Private ###################

// bucket 511 is us, 0 is furthest bucket (should fill first)
int32_t routing_table::bucket_index(const NodeId& node_id) const {
  return our_id_.CommonLeadingBits(node_id);
}

std::vector<node_info>::reverse_iterator routing_table::is_node_viable_for_routing_table() {
  size_t bucket_count(0);
  int bucket(0);
  if (nodes_.size() < kRoutingTableSize)
    return nodes_.rend();
  auto found = std::find_if(nodes_.rbegin(), nodes_.rbegin() + kGroupSize,
                            [&bucket_count, &bucket](const node_info& node) {
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

unsigned int routing_table::network_status(unsigned int size) const {
  return static_cast<unsigned int>((size)*100 / kRoutingTableSize);
}


}  // namespace routing

}  // namespace maidsafe
