/* Copyright 2012 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

#include "maidsafe/routing/network_statistics.h"

#include <string>
#include <algorithm>

#include "maidsafe/routing/parameters.h"

namespace maidsafe {

namespace routing {

NetworkStatistics::NetworkStatistics(const NodeId& node_id)
    :  mutex_(),
       kNodeId_(node_id),
       distance_(),
       network_distance_data_() {}

void NetworkStatistics::UpdateLocalAverageDistance(std::vector<NodeId>& unique_nodes) {
  if (unique_nodes.size() < Parameters::node_group_size)
    return;
  std::nth_element(unique_nodes.begin(),
                   unique_nodes.begin() + Parameters::node_group_size,
                   unique_nodes.end(),
                   [&](const NodeId& lhs, const NodeId& rhs) {
                     return NodeId::CloserToTarget(lhs, rhs, kNodeId_);
                   });
  NodeId furthest_group_node(unique_nodes.at(std::min(Parameters::node_group_size - 1,
                                   static_cast<int>(unique_nodes.size()))));
  {
     std::lock_guard<std::mutex> lock(mutex_);
     distance_ = furthest_group_node ^ kNodeId_;
  }
}

void NetworkStatistics::UpdateNetworkAverageDistance(const NodeId& distance) {
  if (distance == NodeId())
    return;
  crypto::BigInt distance_integer((distance.ToStringEncoded(NodeId::kHex) + 'h').c_str());
  {
    std::lock_guard<std::mutex> lock(mutex_);
    network_distance_data_.total_distance += distance_integer;
    auto average(network_distance_data_.total_distance /
                 ++network_distance_data_.contributors_count);
    std::string average_str(NodeId::kSize, '\0');
    for (auto index(NodeId::kSize - 1); index >= 0; --index)
      average_str[NodeId::kSize - 1 - index] = average.GetByte(index);
    network_distance_data_.average_distance = NodeId(average_str);
  }
}

bool NetworkStatistics::EstimateInGroup(const NodeId& sender_id, const NodeId& info_id) {
  NodeId local_distance;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    local_distance = distance_;
  }
  return crypto::BigInt(((info_id ^ sender_id).ToStringEncoded(NodeId::kHex) + 'h').c_str()) <=
      crypto::BigInt((local_distance.ToStringEncoded(NodeId::kHex) + 'h').c_str()) *
      Parameters::accepted_distance_tolerance;
}

NodeId NetworkStatistics::GetDistance() {
  return distance_;
}

}  // namespace routing

}  // namespace maidsafe
