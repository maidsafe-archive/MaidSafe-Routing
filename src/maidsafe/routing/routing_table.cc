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
#include <iostream>

#include "maidsafe/common/utils.h"

#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

routing_table::routing_table(NodeId our_id, asymm::Keys keys)
    : our_id_(std::move(our_id)), kKeys_(std::move(keys)), mutex_(), nodes_() {}

bool routing_table::add_node(node_info their_info) {
  if (!their_info.id.IsValid() || their_info.id == our_id_ ||
      !asymm::ValidateKey(their_info.public_key)) {
    return false;
  }

  node_info removed_node;
  std::lock_guard<std::mutex> lock(mutex_);

  if (std::any_of(nodes_.begin(), nodes_.end(), [&their_info](const node_info& node_info) {
        return node_info.id == their_info.id;
      })) {
    return false;
  }
  auto remove_node(find_candidate_for_removal());

  if (nodes_.size() < default_routing_table_size) {
    nodes_.push_back(their_info);
  } else if (NodeId::CloserToTarget(their_info.id, nodes_.at(kGroupSize).id, our_id())) {
    // try to push another node out here as new node is also a close node
    // rather than removing the old close node we keep it if possible and
    // sacrifice a less importnt node
    auto remove_node(find_candidate_for_removal());
    if (remove_node != nodes_.rend())
      nodes_.erase(std::next(remove_node).base());
    nodes_.push_back(their_info);
  } else if ((remove_node != nodes_.rend()) &&
             bucket_index(their_info.id) > bucket_index(std::next(remove_node).base()->id)) {
    removed_node = *(std::next(remove_node).base());
    nodes_.erase(std::next(remove_node).base());
    nodes_.push_back(their_info);
  } else {
    return false;
  }
  std::sort(nodes_.begin(), nodes_.end(), [&](const node_info& lhs, const node_info& rhs) {
    return NodeId::CloserToTarget(lhs.id, rhs.id, our_id_);
  });
  return true;
}

bool routing_table::check_node(const node_info& their_info) const {
  if (!their_info.id.IsValid() || their_info.id == our_id_)
    return false;

  std::lock_guard<std::mutex> lock(mutex_);
  if (nodes_.size() < default_routing_table_size)
    return true;
  // check for duplicates
  if (std::any_of(nodes_.begin(), nodes_.end(), [&their_info](const node_info& node_info) {
        return node_info.id == their_info.id;
      }))
    return false;
  // close node
  if (NodeId::CloserToTarget(their_info.id, nodes_.at(group_size).id, our_id()))
    return true;
  // this node is a better fot than we currently have in the routing table
  auto remove_node(find_candidate_for_removal());
  return (remove_node != nodes_.rend() &&
          bucket_index(their_info.id) > bucket_index(std::next(remove_node).base()->id));
}

bool routing_table::drop_node(const NodeId& node_to_drop) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto remove = find_if(std::begin(nodes_), std::end(nodes_),
                        [&node_to_drop](const node_info& node) { return node.id == node_to_drop; });
  if (remove != std::end(nodes_)) {
    nodes_.erase(remove);
    return true;
  }
  return false;
}

std::vector<node_info> routing_table::target_nodes(const NodeId& their_id) const {
  NodeId test_node(our_id_);
  size_t count(0), index(0);
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

  if (index < group_size) {
    return our_close_group();
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return {nodes_.at(index)};
  }
}

std::vector<node_info> routing_table::our_close_group() const {
  std::vector<node_info> result;
  result.reserve(group_size);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto size = std::min(group_size, static_cast<size_t>(nodes_.size()));
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

std::vector<node_info>::const_reverse_iterator routing_table::find_candidate_for_removal() const {
  size_t bucket_count(0);
  int bucket(0);
  if (nodes_.size() < default_routing_table_size)
    return nodes_.rend();
  auto found = std::find_if(nodes_.rbegin(), nodes_.rbegin() + group_size,
                            [&bucket_count, &bucket, this](const node_info& node) {
    auto node_bucket(bucket_index(node.id));
    if (node_bucket != bucket) {
      bucket = node_bucket;
      bucket_count = 0;
    }
    return (++bucket_count > kBucketSize_);
  });
  if (found < nodes_.rbegin() + group_size)
    return found;
  else
    return nodes_.rend();
}

unsigned int routing_table::network_status(size_t size) const {
  return static_cast<unsigned int>(size * 100 / default_routing_table_size);
}

}  // namespace routing

}  // namespace maidsafe
