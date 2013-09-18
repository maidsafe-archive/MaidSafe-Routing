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
