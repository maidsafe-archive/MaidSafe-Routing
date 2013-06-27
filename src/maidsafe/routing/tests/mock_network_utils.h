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

#ifndef MAIDSAFE_ROUTING_TESTS_MOCK_NETWORK_UTILS_H_
#define MAIDSAFE_ROUTING_TESTS_MOCK_NETWORK_UTILS_H_

#include <string>

#include "gmock/gmock.h"

#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/routing.pb.h"


namespace maidsafe {

namespace routing {

namespace test {

class MockNetworkUtils : public NetworkUtils {
 public:
  MockNetworkUtils(RoutingTable& routing_table, ClientRoutingTable& client_routing_table);
  virtual ~MockNetworkUtils();

  MOCK_METHOD1(SendToClosestNode, void(const protobuf::Message& message));
  MOCK_METHOD1(MarkConnectionAsValid, int(const NodeId& peer_id));
  MOCK_METHOD3(SendToDirect, void(const protobuf::Message& message,
                                  const NodeId& peer,
                                  const NodeId& connection));
  MOCK_METHOD3(Add, int(const NodeId& peer_id,
                        const rudp::EndpointPair& peer_endpoint_pair,
                        const std::string& validation_data));
  MOCK_METHOD4(GetAvailableEndpoint, int(const NodeId& peer_id,
                                         const rudp::EndpointPair& peer_endpoint_pair,
                                         rudp::EndpointPair& this_endpoint_pair,
                                         rudp::NatType& this_nat_type));
  void SetBootstrapConnectionId(const NodeId &node_id) {
    this->bootstrap_connection_id_ = node_id;
  }

 private:
  MockNetworkUtils &operator=(const MockNetworkUtils&);
  MockNetworkUtils(const MockNetworkUtils&);
};

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TESTS_MOCK_NETWORK_UTILS_H_
