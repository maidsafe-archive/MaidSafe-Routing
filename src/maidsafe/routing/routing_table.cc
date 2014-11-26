/*  Copyright 2014 MaidSafe.net limited

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

namespace maidsafe {

namespace routing {

#if !defined(_MSC_VER) || _MSC_VER >= 1900
const size_t routing_table::bucket_size;
const size_t routing_table::parallelism;
const size_t routing_table::default_size;
#endif

routing_table::routing_table(NodeId our_id) : our_id_(std::move(our_id)), mutex_(), nodes_() {
  assert(our_id_.IsValid());
}

std::pair<bool, boost::optional<node_info>> routing_table::add_node(node_info their_info) {
  if (!their_info.id.IsValid() || their_info.id == our_id_ ||
      !asymm::ValidateKey(their_info.public_key)) {
    return {false, boost::optional<node_info>()};
  }

  std::lock_guard<std::mutex> lock(mutex_);

  // check not duplicate
  if (have_node(their_info))
    return {false, boost::optional<node_info>()};

  // routing table small, just grab this node
  if (nodes_.size() < default_size) {
    push_back_then_sort(std::move(their_info));
    return {true, boost::optional<node_info>()};
  }

  // new close group member
  auto result =
      std::make_pair<bool, boost::optional<node_info>>(true, boost::optional<node_info>());
  if (NodeId::CloserToTarget(their_info.id, nodes_.at(group_size).id, our_id_)) {
    // first push the new node in (it's close) and then get another sacrificial node if we can
    // this will make RT grow but only after several tens of millions of nodes
    push_back_then_sort(std::move(their_info));
    auto removal_candidate(find_candidate_for_removal());
    if (removal_candidate != std::end(nodes_)) {
      result.second = *removal_candidate;
      nodes_.erase(removal_candidate);
    }
    return result;
  }

  // is there a node we can remove
  auto removal_candidate(find_candidate_for_removal());
  if (new_node_is_better_than_existing(their_info.id, removal_candidate)) {
    result.second = *removal_candidate;
    nodes_.erase(removal_candidate);
    push_back_then_sort(std::move(their_info));
  } else {
    result.first = false;
  }
  return result;
}

bool routing_table::check_node(const NodeId& their_id) const {
  if (!their_id.IsValid() || their_id == our_id_)
    return false;

  std::lock_guard<std::mutex> lock(mutex_);
  if (nodes_.size() < default_size)
    return true;

  // check for duplicates
  static node_info their_info;
  their_info.id = their_id;
  if (have_node(their_info))
    return false;

  // close node
  if (NodeId::CloserToTarget(their_id, nodes_.at(group_size).id, our_id_))
    return true;

  return new_node_is_better_than_existing(their_id, find_candidate_for_removal());
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
  closer_to_target.erase(std::begin(closer_to_target) +
                             std::min(parallelism, closer_to_target.size()),
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

bool routing_table::have_node(const node_info& their_info) const {
  auto comparison = [&](const node_info& lhs, const node_info& rhs) {
    return NodeId::CloserToTarget(lhs.id, rhs.id, our_id_);
  };
  assert(std::is_sorted(std::begin(nodes_), std::end(nodes_), comparison));
  return std::binary_search(std::begin(nodes_), std::end(nodes_), their_info, comparison);
}

bool routing_table::new_node_is_better_than_existing(
    const NodeId& their_id, std::vector<node_info>::const_iterator removal_candidate) const {
  return removal_candidate != std::end(nodes_) &&
         bucket_index(their_id) > bucket_index(removal_candidate->id);
}

void routing_table::push_back_then_sort(node_info&& their_info) {
  nodes_.push_back(std::move(their_info));
  std::sort(std::begin(nodes_), std::end(nodes_), [&](const node_info& lhs, const node_info& rhs) {
    return NodeId::CloserToTarget(lhs.id, rhs.id, our_id_);
  });
}

std::vector<node_info>::const_iterator routing_table::find_candidate_for_removal() const {
  assert(nodes_.size() >= default_size);
  size_t number_in_bucket(0);
  int bucket(0);
  // TODO(Fraser#5#): 2014-11-26 - Should 'default_size' be 'nodes_.size()'?
  auto furthest_group_member = nodes_.rbegin() + default_size - group_size;
  auto found = std::find_if(nodes_.rbegin(), furthest_group_member,
                            [&number_in_bucket, &bucket, this](const node_info& node) {
    if (bucket_index(node.id) != bucket) {
      bucket = bucket_index(node.id);
      number_in_bucket = 0;
    }
    ++number_in_bucket;
    return number_in_bucket > bucket_size;
  });

  return found == furthest_group_member ? std::end(nodes_) : found.base();
}

unsigned int routing_table::network_status(size_t size) const {
  return static_cast<unsigned int>(size * 100 / default_size);
}

}  // namespace routing

}  // namespace maidsafe
