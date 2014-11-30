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

const size_t routing_table::bucket_size = 1;
const size_t routing_table::parallelism = 4;
const size_t routing_table::routing_table_size = 64;

routing_table::routing_table(NodeId our_id) : our_id_(std::move(our_id)), mutex_(), nodes_() {}

std::pair<bool, boost::optional<node_info>> routing_table::add_node(node_info their_info) {
  if (!their_info.id.IsValid() || their_info.id == our_id_ ||
      !asymm::ValidateKey(their_info.public_key)) {
    return {false, boost::optional<node_info>()};
  }

  std::lock_guard<std::mutex> lock(mutex_);
  // check not duplicate
  if (std::any_of(nodes_.begin(), nodes_.end(), [&their_info](const node_info& node_info) {
        return node_info.id == their_info.id;
      })) {
    return {false, boost::optional<node_info>()};
  }
  // routing table small, just grab this node
  if (nodes_.size() < routing_table_size) {
    nodes_.push_back(std::move(their_info));
    sort();
    return {true, boost::optional<node_info>()};
  }

  // new close group member
  if (NodeId::CloserToTarget(their_info.id, nodes_.at(group_size).id, our_id())) {
    // first push the new node in (its close) and then get antoher sacrificial node if we can
    // this will make RT grow but only after several tens of millions of nodes
    nodes_.push_back(std::move(their_info));
    sort();
    auto remove_candidate(find_candidate_for_removal());
    auto sacrificial_candidate(remove_candidate != std::end(nodes_));
    if (sacrificial_candidate) {
      nodes_.erase(remove_candidate);
      return {true, boost::optional<node_info>(*remove_candidate)};
    }
    return {true, boost::optional<node_info>()};
  }

  // is there a node we can remove
  auto remove_node(find_candidate_for_removal());
  auto sacrificial_node(remove_node != std::end(nodes_));
  node_info remove_id;

  if (sacrificial_node)
    remove_id = *remove_node;
  if (sacrificial_node && bucket_index(their_info.id) > bucket_index(remove_node->id)) {
    nodes_.erase(remove_node);
    nodes_.push_back(std::move(their_info));
    sort();
  }
  return {false, boost::optional<node_info>()};
}

bool routing_table::check_node(const NodeId& their_id) const {
  if (!their_id.IsValid() || their_id == our_id_)
    return false;

  std::lock_guard<std::mutex> lock(mutex_);
  if (nodes_.size() < routing_table_size)
    return true;
  // check for duplicates
  if (std::any_of(nodes_.begin(), nodes_.end(),
                  [&their_id](const node_info& node_info) { return node_info.id == their_id; }))
    return false;
  // close node
  if (NodeId::CloserToTarget(their_id, nodes_.at(group_size).id, our_id()))
    return true;
  // this node is a better fit than we currently have in the routing table
  auto remove_node(find_candidate_for_removal());
  return (remove_node != std::end(nodes_) &&
          bucket_index(their_id) > bucket_index(remove_node->id));
}

void routing_table::drop_node(const NodeId& node_to_drop) {
  std::lock_guard<std::mutex> lock(mutex_);
  nodes_.erase(
      remove_if(std::begin(nodes_), std::end(nodes_),
                [&node_to_drop](const node_info& node) { return node.id == node_to_drop; }),
      std::end(nodes_));
}

std::vector<node_info> routing_table::target_nodes(const NodeId& their_id) const {
  NodeId test_node(our_id_);
  std::vector<node_info> closer_to_target;
  size_t count(0), index(0);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& node : nodes_) {
      if (NodeId::CloserToTarget(node.id, test_node, their_id)) {
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
            [this, &their_id](const node_info& lhs, const node_info& rhs) {
    return NodeId::CloserToTarget(lhs.id, rhs.id, our_id_);
  });
  closer_to_target.erase(
      std::begin(closer_to_target) + std::min(parallelism, closer_to_target.size()),
      std::end(closer_to_target));
  return closer_to_target;
}

std::vector<node_info> routing_table::our_close_group() const {
  std::vector<node_info> result;
  result.reserve(group_size);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto size = std::min(group_size, nodes_.size());
    std::copy(std::begin(nodes_), std::begin(nodes_) + size, std::back_inserter(result));
  }
  return result;
}

size_t routing_table::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return nodes_.size();
}

// bucket 511 is us, 0 is furthest bucket (should fill first)
int32_t routing_table::bucket_index(const NodeId& node_id) const {
  assert(node_id != our_id_);
  return our_id_.CommonLeadingBits(node_id);
}

void routing_table::sort() {
  std::sort(nodes_.begin(), nodes_.end(), [&](const node_info& lhs, const node_info& rhs) {
    return NodeId::CloserToTarget(lhs.id, rhs.id, our_id_);
  });
}

std::vector<node_info>::const_iterator routing_table::find_candidate_for_removal() const {
  // this is only ever called on full routing table
  size_t number_in_bucket(0);
  int bucket(NodeId::kSize);
  auto found = std::find_if(nodes_.rbegin(), nodes_.rbegin() + group_size,
                            [&number_in_bucket, &bucket, this](const node_info& node) {
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

unsigned int routing_table::network_status(size_t size) const {
  return static_cast<unsigned int>(size * 100 / routing_table_size);
}

}  // namespace routing

}  // namespace maidsafe
