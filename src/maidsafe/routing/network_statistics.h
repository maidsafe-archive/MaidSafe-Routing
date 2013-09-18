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

