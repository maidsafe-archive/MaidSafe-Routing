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

RoutingTable::RoutingTable(Address our_id)
    : our_id_(std::move(our_id)), comparison_(our_id_), mutex_(), nodes_() {
  assert(our_id_.IsValid());
}

std::pair<bool, boost::optional<NodeInfo>> RoutingTable::AddNode(NodeInfo their_info) {
  if (!their_info.id.IsValid() || their_info.id == our_id_ ||
      !asymm::ValidateKey(their_info.public_key)) {
    return {false, boost::optional<NodeInfo>()};
  }

  std::lock_guard<std::mutex> lock(mutex_);

  // check not duplicate
  if (HaveNode(their_info))
    return {false, boost::optional<NodeInfo>()};

  // routing table small, just grab this node
  if (nodes_.size() < OptimalSize()) {
    PushBackThenSort(std::move(their_info));
    return {true, boost::optional<NodeInfo>()};
  }

  // new close group member
  auto result = std::make_pair(true, boost::optional<NodeInfo>());
  if (Address::CloserToTarget(their_info.id, nodes_.at(kGroupSize).id, our_id_)) {
    // first push the new node in (it's close) and then get another sacrificial node if we can
    // this will make RT grow but only after several tens of millions of nodes
    PushBackThenSort(std::move(their_info));
    auto removal_candidate(FindCandidateForRemoval());
    if (removal_candidate != std::end(nodes_)) {
      result.second = *removal_candidate;
      nodes_.erase(removal_candidate);
    }
    return result;
  }

  // is there a node we can remove
  auto removal_candidate(FindCandidateForRemoval());
  if (NewNodeIsBetterThanExisting(their_info.id, removal_candidate)) {
    result.second = *removal_candidate;
    nodes_.erase(removal_candidate);
    PushBackThenSort(std::move(their_info));
  } else {
    result.first = false;
  }
  return result;
}

bool RoutingTable::CheckNode(const Address& their_id) const {
  if (!their_id.IsValid() || their_id == our_id_)
    return false;

  // check for duplicates
  static NodeInfo their_info;
  their_info.id = their_id;
  if (HaveNode(their_info))
    return false;

  std::lock_guard<std::mutex> lock(mutex_);
  if (nodes_.size() < OptimalSize())
    return true;

  // close node
  if (Address::CloserToTarget(their_id, nodes_.at(kGroupSize).id, our_id_))
    return true;

  return NewNodeIsBetterThanExisting(their_id, FindCandidateForRemoval());
}

void RoutingTable::DropNode(const Address& node_to_drop) {
  std::lock_guard<std::mutex> lock(mutex_);
  nodes_.erase(remove_if(std::begin(nodes_), std::end(nodes_),
                         [&node_to_drop](const NodeInfo& node) { return node.id == node_to_drop; }),
               std::end(nodes_));
}

std::vector<NodeInfo> RoutingTable::TargetNodes(const Address& their_id) const {
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

  if (index < kGroupSize) {
    return OurCloseGroup();
  }

  std::sort(std::begin(closer_to_target), std::end(closer_to_target),
            [this, &their_id](const NodeInfo& lhs, const NodeInfo& rhs) {
    return Address::CloserToTarget(lhs.id, rhs.id, our_id_);
  });
  closer_to_target.erase(std::begin(closer_to_target) +
                             std::min(Parallelism(), closer_to_target.size()),
                         std::end(closer_to_target));
  return closer_to_target;
}

std::vector<NodeInfo> RoutingTable::OurCloseGroup() const {
  std::vector<NodeInfo> result;
  result.reserve(kGroupSize);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto size = std::min(kGroupSize, nodes_.size());
    std::copy(std::begin(nodes_), std::begin(nodes_) + size, std::back_inserter(result));
  }
  return result;
}

size_t RoutingTable::Size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return nodes_.size();
}

// bucket 511 is us, 0 is furthest bucket (should fill first)
int32_t RoutingTable::BucketIndex(const Address& address) const {
  assert(address != our_id_);
  return our_id_.CommonLeadingBits(address);
}

bool RoutingTable::HaveNode(const NodeInfo& their_info) const {
  assert(std::is_sorted(std::begin(nodes_), std::end(nodes_), comparison_));
  return std::binary_search(std::begin(nodes_), std::end(nodes_), their_info, comparison_);
}

bool RoutingTable::NewNodeIsBetterThanExisting(
    const Address& their_id, std::vector<NodeInfo>::const_iterator removal_candidate) const {
  return removal_candidate != std::end(nodes_) &&
         BucketIndex(their_id) > BucketIndex(removal_candidate->id);
}

void RoutingTable::PushBackThenSort(NodeInfo&& their_info) {
  nodes_.push_back(std::move(their_info));
  std::sort(std::begin(nodes_), std::end(nodes_), comparison_);
}

std::vector<NodeInfo>::const_iterator RoutingTable::FindCandidateForRemoval() const {
  assert(nodes_.size() >= OptimalSize());
  size_t number_in_bucket(0);
  int bucket(0);
  auto furthest_group_member = nodes_.rbegin() + nodes_.size() - kGroupSize;
  auto found = std::find_if(nodes_.rbegin(), furthest_group_member,
                            [&number_in_bucket, &bucket, this](const NodeInfo& node) {
    if (BucketIndex(node.id) != bucket) {
      bucket = BucketIndex(node.id);
      number_in_bucket = 0;
    }
    ++number_in_bucket;
    return number_in_bucket > BucketSize();
  });

  return found == furthest_group_member ? std::end(nodes_) : found.base();
}

unsigned int RoutingTable::NetworkStatus(size_t size) const {
  return static_cast<unsigned int>(size * 100 / OptimalSize());
}

}  // namespace routing

}  // namespace maidsafe
