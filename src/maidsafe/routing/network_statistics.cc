/*******************************************************************************
 *  Copyright 2012 maidsafe.net limited                                        *
 *                                                                             *
 *  The following source code is property of maidsafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the licence   *
 *  file licence.txt found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of maidsafe.net. *
 ******************************************************************************/

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

void NetworkStatistics::UpdateLocalAverageDistance(std::vector<NodeInfo> unique_nodes) {
  if (unique_nodes.size() < Parameters::node_group_size)
    return;
  std::nth_element(unique_nodes.begin(),
                   unique_nodes.begin() + Parameters::node_group_size,
                   unique_nodes.end(),
                   [&](const NodeInfo& lhs, const NodeInfo& rhs) {
                     return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, kNodeId_);
                   });
  NodeInfo furthest_group_node(unique_nodes.at(std::min(Parameters::node_group_size - 1,
                                   static_cast<int>(unique_nodes.size()))));
  {
     std::lock_guard<std::mutex> lock(mutex_);
     distance_ = furthest_group_node.node_id ^ kNodeId_;
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
