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

#include "maidsafe/routing/network_statistics.h"

#include <string>
#include <algorithm>

#include "maidsafe/routing/parameters.h"

namespace maidsafe {

namespace routing {

NetworkStatistics::NetworkStatistics(NodeId node_id)
    : mutex_(), kNodeId_(std::move(node_id)), distance_(), network_distance_data_() {}

void NetworkStatistics::UpdateLocalAverageDistance(const std::vector<NodeId>& close_nodes) {
  std::vector<NodeId> unique_nodes(close_nodes);
  if (unique_nodes.size() < Parameters::group_size)
    return;
#ifndef __GNUC__
  std::nth_element(unique_nodes.begin(), unique_nodes.begin() + Parameters::group_size,
                   unique_nodes.end(), [&](const NodeId & lhs, const NodeId & rhs) {
    return NodeId::CloserToTarget(lhs, rhs, kNodeId_);
  });
#else
  // BEFORE_RELEASE use std::nth_element() for all platform when min required Gcc version is 4.8.3
  // http://gcc.gnu.org/bugzilla/show_bug.cgi?id=58800 Bug fixed in gcc 4.8.3
  std::partial_sort(unique_nodes.begin(), unique_nodes.begin() + Parameters::group_size,
                    unique_nodes.end(), [&](const NodeId & lhs, const NodeId & rhs) {
    return NodeId::CloserToTarget(lhs, rhs, kNodeId_);
  });
#endif
  NodeId furthest_group_node(unique_nodes.at(
      std::min(Parameters::group_size - 1, static_cast<unsigned int>(unique_nodes.size()))));
  {
    std::lock_guard<std::mutex> lock(mutex_);
    distance_ = furthest_group_node ^ kNodeId_;
  }
}

void NetworkStatistics::UpdateNetworkAverageDistance(const NodeId& distance) {
  if (distance == NodeId())
    return;
  crypto::BigInt distance_integer(
      (distance.ToStringEncoded(NodeId::EncodingType::kHex) + 'h').c_str());
  {
    std::lock_guard<std::mutex> lock(mutex_);
    network_distance_data_.total_distance += distance_integer;
    auto average(network_distance_data_.total_distance /
                 ++network_distance_data_.contributors_count);
    std::string average_str(NodeId::kSize, '\0');
    for (auto index(static_cast<int>(NodeId::kSize) - 1); index >= 0; --index)
      average_str[NodeId::kSize - 1 - index] = average.GetByte(index);
    network_distance_data_.average_distance = NodeId(average_str);
  }
}

// FIXME(Prakash) handle the case of sender_id == info_id
bool NetworkStatistics::EstimateInGroup(const NodeId& sender_id, const NodeId& info_id) {
  NodeId local_distance;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    local_distance = distance_;
  }
  return crypto::BigInt(
             ((info_id ^ sender_id).ToStringEncoded(NodeId::EncodingType::kHex) + 'h').c_str()) <=
         crypto::BigInt(
             (local_distance.ToStringEncoded(NodeId::EncodingType::kHex) + 'h').c_str()) *
             Parameters::accepted_distance_tolerance;
}

NodeId NetworkStatistics::GetDistance() { return distance_; }

}  // namespace routing

}  // namespace maidsafe
