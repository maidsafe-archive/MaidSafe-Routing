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

#include "maidsafe/common/error.h"
#include "maidsafe/common/utils.h"

namespace maidsafe {

namespace routing {

namespace {

void Validate(const Address& id) {
  assert(id.IsValid());
  if (!id.IsValid())
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::invalid_node_id));
}

}  // unnamed namespace

RoutingTable::RoutingTable(Address our_id)
    : our_id_(std::move(our_id)), comparison_(our_id_), mutex_(), nodes_() {
  assert(our_id_.IsValid());
}

std::pair<bool, boost::optional<NodeInfo>> RoutingTable::AddNode(NodeInfo their_info) {
  Validate(their_info.id);
  if (their_info.id == our_id_ || !their_info.dht_fob)
    return {false, boost::none};

  std::lock_guard<std::mutex> lock(mutex_);

  // check not duplicate
  if (HaveNode(their_info))
    return {false, boost::none};

  // routing table small, just grab this node
  if (nodes_.size() < OptimalSize()) {
    PushBackThenSort(their_info);
    return {true, their_info};
  }

  // new close group member
  if (Address::CloserToTarget(their_info.id, nodes_.at(GroupSize).id, our_id_)) {
    // first push the new node in (it's close) and then get another sacrificial node if we can
    // this will make RT grow but only after several tens of millions of nodes
    PushBackThenSort(std::move(their_info));
    auto removal_candidate(FindCandidateForRemoval());
    if (removal_candidate != std::end(nodes_)) {
      auto iter = nodes_.begin();
      std::advance(iter, std::distance<decltype(removal_candidate)>(iter, removal_candidate));
      auto candidate = *removal_candidate;
      nodes_.erase(iter);
      return {true, candidate};
    }
  }

  // is there a node we can remove
  auto removal_candidate(FindCandidateForRemoval());
  if (NewNodeIsBetterThanExisting(their_info.id, removal_candidate)) {
    auto iter = nodes_.begin();
    std::advance(iter, std::distance<decltype(removal_candidate)>(iter, removal_candidate));
    nodes_.erase(iter);
    PushBackThenSort(std::move(their_info));
    return {true, *removal_candidate};
  }
  return {false, boost::none};
}

bool RoutingTable::CheckNode(const Address& their_id) const {
  Validate(their_id);
  if (their_id == our_id_)
    return false;

  // check for duplicates
  if (HaveNode(NodeInfo(their_id)))
    return false;

  std::lock_guard<std::mutex> lock(mutex_);
  if (nodes_.size() < OptimalSize())
    return true;

  // close node
  if (Address::CloserToTarget(their_id, nodes_.at(GroupSize).id, our_id_))
    return true;

  return NewNodeIsBetterThanExisting(their_id, FindCandidateForRemoval());
}

void RoutingTable::DropNode(const Address& node_to_drop) {
  Validate(node_to_drop);
  std::lock_guard<std::mutex> lock(mutex_);
  nodes_.erase(remove_if(std::begin(nodes_), std::end(nodes_),
                         [&node_to_drop](const NodeInfo& node) { return node.id == node_to_drop; }),
               std::end(nodes_));
}

std::vector<NodeInfo> RoutingTable::TargetNodes(const Address& target) const {
  Validate(target);

  using NodesItr = std::vector<NodeInfo>::const_iterator;
  std::vector<NodesItr> our_close_group, closest_to_target;
  size_t iterations(0);
  std::vector<NodeInfo> result;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto parallelism = std::min(Parallelism(), nodes_.size());
    for (auto itr = std::begin(nodes_); itr != std::end(nodes_); ++itr, ++iterations) {
      // close group is first 'GroupSize' contacts
      if (iterations < GroupSize)
        our_close_group.push_back(itr);
      // add 'itr' to collection of all iterators for later partial sorting by closeness to target
      closest_to_target.push_back(itr);
    }
    if (closest_to_target.empty())
      return result;

    // partially sort 'parallelism' contacts by closeness to target
    std::partial_sort(std::begin(closest_to_target), std::begin(closest_to_target) + parallelism,
                      std::end(closest_to_target), Comparison(target));

    // if the closest to target is within our close group, just return the close group
    if (std::any_of(std::begin(our_close_group), std::end(our_close_group),
                    [&](NodesItr group_itr) { return group_itr == closest_to_target.front(); })) {
      for (auto group_itr : our_close_group)
        result.push_back(*group_itr);
    } else {  // return the 'parallelism' closest-to-target contacts
      for (auto closest_itr = std::begin(closest_to_target);
           closest_itr != std::begin(closest_to_target) + parallelism; ++closest_itr) {
        result.push_back(*(*closest_itr));
      }
    }
  }
  return result;
}

std::vector<NodeInfo> RoutingTable::OurCloseGroup() const {
  std::vector<NodeInfo> result;
  result.reserve(GroupSize);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto size = std::min(GroupSize, nodes_.size());
    std::copy(std::begin(nodes_), std::begin(nodes_) + size, std::back_inserter(result));
  }
  return result;
}

boost::optional<asymm::PublicKey> RoutingTable::GetPublicKey(const Address& their_id) const {
  Validate(their_id);
  NodeInfo their_info(their_id);
  std::lock_guard<std::mutex> lock(mutex_);
  assert(std::is_sorted(std::begin(nodes_), std::end(nodes_), comparison_));
  auto itrs = std::equal_range(std::begin(nodes_), std::end(nodes_), their_info, comparison_);
  if (itrs.first == itrs.second)
    return boost::none;
  assert(std::distance(itrs.first, itrs.second) == 1);
  return itrs.first->dht_fob->public_key();
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

void RoutingTable::PushBackThenSort(NodeInfo their_info) {
  nodes_.push_back(std::move(their_info));
  std::sort(std::begin(nodes_), std::end(nodes_), comparison_);
}

std::vector<NodeInfo>::const_iterator RoutingTable::FindCandidateForRemoval() const {
  assert(nodes_.size() >= OptimalSize());
  size_t number_in_bucket(0);
  int bucket(0);
  auto furthest_group_member = nodes_.rbegin() + nodes_.size() - GroupSize;
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

}  // namespace routing

}  // namespace maidsafe
