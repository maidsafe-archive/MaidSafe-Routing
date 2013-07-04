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

#ifndef MAIDSAFE_ROUTING_NETWORK_STATISTICS_H_
#define MAIDSAFE_ROUTING_NETWORK_STATISTICS_H_

#include <vector>

#include "maidsafe/common/crypto.h"

#include "maidsafe/common/node_id.h"
#include "maidsafe/routing/node_info.h"


namespace maidsafe {

namespace routing {

namespace test {
  class NetworkStatisticsTest_BEH_AverageDistance_Test;
  class NetworkStatisticsTest_BEH_IsIdInGroupRange_Test;
}

class NetworkStatistics {
 public:
  explicit NetworkStatistics(const NodeId& node_id);
  void UpdateLocalAverageDistance(std::vector<NodeId>& unique_nodes);
  void UpdateNetworkAverageDistance(const NodeId& distance);
  bool EstimateInGroup(const NodeId& sender_id, const NodeId& info_id);
  NodeId GetDistance();

  friend class test::NetworkStatisticsTest_BEH_AverageDistance_Test;
  friend class test::NetworkStatisticsTest_BEH_IsIdInGroupRange_Test;

 private:
  NetworkStatistics(const NetworkStatistics&);
  NetworkStatistics& operator=(const NetworkStatistics&);
  struct NetworkDistanceData {
    NetworkDistanceData() : contributors_count(), total_distance(), average_distance() {}
    crypto::BigInt contributors_count, total_distance;
    NodeId average_distance;
  };
  std::mutex mutex_;
  const NodeId kNodeId_;
  NodeId distance_;
  NetworkDistanceData network_distance_data_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_NETWORK_STATISTICS_H_

