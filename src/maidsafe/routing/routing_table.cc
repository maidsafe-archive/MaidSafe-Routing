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

#include "maidsafe/common/utils.h"

#include "maidsafe/routing/node_info.h"

namespace maidsafe {

namespace routing {

const size_t RoutingTable::bucket_size = 1;
const size_t RoutingTable::parallelism = 4;
const size_t RoutingTable::RoutingTable_size = 64;

RoutingTable::RoutingTable(Address our_id) : our_id_(std::move(our_id)), mutex_(), nodes_() {}

std::pair<bool, boost::optional<NodeInfo>> RoutingTable::add_node(NodeInfo their_info) {
  if (!their_info.id.IsValid() || their_info.id == our_id_ ||
      !asymm::ValidateKey(their_info.public_key)) {
    return {false, boost::optional<NodeInfo>()};
  }

  std::lock_guard<std::mutex> lock(mutex_);
  // check not duplicate
  if (std::any_of(nodes_.begin(), nodes_.end(), [&their_info](const NodeInfo& NodeInfo) {
        return NodeInfo.id == their_info.id;
      })) {
    return {false, boost::optional<NodeInfo>()};
  }
  // routing table small, just grab this node
  if (nodes_.size() < RoutingTable_size) {
    nodes_.push_back(std::move(their_info));
    sort();
    return {true, boost::optional<NodeInfo>()};
  }

  // new close group member
  if (Address::CloserToTarget(their_info.id, nodes_.at(group_size).id, our_id())) {
    // first push the new node in (its close) and then get antoher sacrificial node if we can
    // this will make RT grow but only after several tens of millions of nodes
    nodes_.push_back(std::move(their_info));
    sort();
    auto remove_candidate(find_candidate_for_removal());
    auto sacrificial_candidate(remove_candidate != std::end(nodes_));
    if (sacrificial_candidate) {
      nodes_.erase(remove_candidate);
      return {true, boost::optional<NodeInfo>(*remove_candidate)};
    }
    return {true, boost::optional<NodeInfo>()};
  }

  // is there a node we can remove
  auto remove_node(find_candidate_for_removal());
  auto sacrificial_node(remove_node != std::end(nodes_));
  NodeInfo remove_id;

  if (sacrificial_node)
    remove_id = *remove_node;
  if (sacrificial_node && bucket_index(their_info.id) > bucket_index(remove_node->id)) {
    nodes_.erase(remove_node);
    nodes_.push_back(std::move(their_info));
    sort();
  }
  return {false, boost::optional<NodeInfo>()};
}

bool RoutingTable::check_node(const Address& their_id) const {
  if (!their_id.IsValid() || their_id == our_id_)
    return false;

  std::lock_guard<std::mutex> lock(mutex_);
  if (nodes_.size() < RoutingTable_size)
    return true;
  // check for duplicates
  if (std::any_of(nodes_.begin(), nodes_.end(),
                  [&their_id](const NodeInfo& NodeInfo) { return NodeInfo.id == their_id; }))
    return false;
  // close node
  if (Address::CloserToTarget(their_id, nodes_.at(group_size).id, our_id()))
    return true;
  // this node is a better fit than we currently have in the routing table
  auto remove_node(find_candidate_for_removal());
  return (remove_node != std::end(nodes_) &&
          bucket_index(their_id) > bucket_index(remove_node->id));
}

void RoutingTable::drop_node(const Address& node_to_drop) {
  std::lock_guard<std::mutex> lock(mutex_);
  nodes_.erase(
      remove_if(std::begin(nodes_), std::end(nodes_),
                [&node_to_drop](const NodeInfo& node) { return node.id == node_to_drop; }),
      std::end(nodes_));
}

std::vector<NodeInfo> RoutingTable::target_nodes(const Address& their_id) const {
  Address test_node(our_id_);
  std::vector<NodeInfo> closer_to_target;
  size_t count(0), index(0);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& node : nodes_) {
      if (Address::CloserToTarget(node.id, test_node, their_id)) {
        closer_to_target.push_back(node);
        test_node = node.id;
        index = count;
      }
      ++count;
    }
  }

  if (index < group_size) {
    return our_close_group();
  }

  std::sort(std::begin(closer_to_target), std::end(closer_to_target),
            [this, &their_id](const NodeInfo& lhs, const NodeInfo& rhs) {
    return Address::CloserToTarget(lhs.id, rhs.id, our_id_);
  });
  closer_to_target.erase(
      std::begin(closer_to_target) + std::min(parallelism, closer_to_target.size()),
      std::end(closer_to_target));
  return closer_to_target;
}

std::vector<NodeInfo> RoutingTable::our_close_group() const {
  std::vector<NodeInfo> result;
  result.reserve(group_size);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto size = std::min(group_size, nodes_.size());
    std::copy(std::begin(nodes_), std::begin(nodes_) + size, std::back_inserter(result));
  }
  return result;
}

size_t RoutingTable::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return nodes_.size();
}

// bucket 511 is us, 0 is furthest bucket (should fill first)
int32_t RoutingTable::bucket_index(const Address& Address) const {
  assert(Address != our_id_);
  return our_id_.CommonLeadingBits(Address);
}

void RoutingTable::sort() {
  std::sort(nodes_.begin(), nodes_.end(), [&](const NodeInfo& lhs, const NodeInfo& rhs) {
    return Address::CloserToTarget(lhs.id, rhs.id, our_id_);
  });
}

std::vector<NodeInfo>::const_iterator RoutingTable::find_candidate_for_removal() const {
  // this is only ever called on full routing table
  size_t number_in_bucket(0);
  int bucket(Address::kSize);
  auto found = std::find_if(nodes_.rbegin(), nodes_.rbegin() + group_size,
                            [&number_in_bucket, &bucket, this](const NodeInfo& node) {
    if (bucket_index(node.id) != bucket) {
      bucket = bucket_index(node.id);
      number_in_bucket = 0;
    }
    ++number_in_bucket;
    return (number_in_bucket > bucket_size);
  });

  if (found != nodes_.rbegin() + group_size) {
    return std::next(found.base());
  }
  return std::end(nodes_);
}

unsigned int RoutingTable::network_status(size_t size) const {
  return static_cast<unsigned int>(size * 100 / RoutingTable_size);
}

}  // namespace routing

}  // namespace maidsafe
