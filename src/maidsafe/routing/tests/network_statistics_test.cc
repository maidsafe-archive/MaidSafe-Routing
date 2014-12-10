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

#include <bitset>
#include <memory>
#include <numeric>
#include <vector>

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/test.h"

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/tests/test_utils.h"
#include "maidsafe/routing/network_statistics.h"

namespace maidsafe {
namespace routing {
namespace test {

TEST(NetworkStatisticsTest, BEH_AverageDistance) {
  NodeId node_id(RandomString(NodeId::kSize));
  NodeId average(node_id);
  NetworkStatistics network_statistics(node_id);
  network_statistics.network_distance_data_.average_distance = average;
  network_statistics.UpdateNetworkAverageDistance(average);
  EXPECT_EQ(network_statistics.network_distance_data_.average_distance, average);

  node_id = NodeId();
  network_statistics.network_distance_data_.total_distance = crypto::BigInt::Zero();
  network_statistics.network_distance_data_.average_distance = NodeId();
  average = node_id;
  network_statistics.UpdateNetworkAverageDistance(node_id);
  EXPECT_EQ(network_statistics.network_distance_data_.average_distance, average);

  node_id = NodeInNthBucket(NodeId(), 511);
  network_statistics.network_distance_data_.total_distance =
      crypto::BigInt((node_id.ToStringEncoded(NodeId::EncodingType::kHex) + 'h').c_str()) *
      network_statistics.network_distance_data_.contributors_count;
  average = node_id;
  network_statistics.UpdateNetworkAverageDistance(node_id);
  EXPECT_EQ(network_statistics.network_distance_data_.average_distance, average);

  network_statistics.network_distance_data_.contributors_count = 0;
  network_statistics.network_distance_data_.total_distance = crypto::BigInt::Zero();

  std::vector<NodeId> distances_as_node_id;
  std::vector<crypto::BigInt> distances_as_bigint;
  uint32_t kCount(RandomUint32() % 1000 + 9000);
  for (uint32_t i(0); i < kCount; ++i) {
    NodeId node_id(RandomString(NodeId::kSize));
    distances_as_node_id.push_back(node_id);
    distances_as_bigint.push_back(
        crypto::BigInt((node_id.ToStringEncoded(NodeId::EncodingType::kHex) + 'h').c_str()));
  }

  crypto::BigInt total(std::accumulate(distances_as_bigint.begin(), distances_as_bigint.end(),
                                       crypto::BigInt::Zero()));

  for (const auto& node_id : distances_as_node_id)
    network_statistics.UpdateNetworkAverageDistance(node_id);

  crypto::BigInt matrix_average_as_bigint(
      (network_statistics.network_distance_data_.average_distance.ToStringEncoded(
           NodeId::EncodingType::kHex) +
       'h').c_str());

  EXPECT_EQ(total / kCount, matrix_average_as_bigint);
}

TEST(NetworkStatisticsTest, FUNC_IsIdInGroupRange) {
  NodeId node_id;
  NetworkStatistics network_statistics(node_id);
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair());
  std::vector<NodeId> nodes_id;
  NodeInfo node_info;
  NodeId my_node(routing_table.kNodeId());
  while (static_cast<unsigned int>(routing_table.size()) < Parameters::max_routing_table_size) {
    NodeInfo node(MakeNode());
    nodes_id.push_back(node.id);
    EXPECT_TRUE(routing_table.AddNode(node));
  }

  NodeId info_id(RandomString(NodeId::kSize));
  std::partial_sort(nodes_id.begin(), nodes_id.begin() + Parameters::group_size + 1, nodes_id.end(),
                    [&](const NodeId& lhs,
                        const NodeId& rhs) { return NodeId::CloserToTarget(lhs, rhs, info_id); });
  unsigned int index(0);
  while (index < Parameters::max_routing_table_size) {
    if ((nodes_id.at(index) ^ info_id) <= (network_statistics.distance_))
      EXPECT_TRUE(network_statistics.EstimateInGroup(nodes_id.at(index++), info_id));
    else
      EXPECT_FALSE(network_statistics.EstimateInGroup(nodes_id.at(index++), info_id));
  }
}

}  // namespace test
}  // namespace routing
}  // namespace maidsafe
