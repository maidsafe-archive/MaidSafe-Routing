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

#ifndef MAIDSAFE_ROUTING_TESTS_MOCK_NETWORK_UTILS_H_
#define MAIDSAFE_ROUTING_TESTS_MOCK_NETWORK_UTILS_H_

#include "gmock/gmock.h"

#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/routing_pb.h"


namespace maidsafe {

namespace routing {

namespace test {

class MockNetworkUtils : public NetworkUtils {
 public:
  MockNetworkUtils(RoutingTable& routing_table, NonRoutingTable& non_routing_table);
  virtual ~MockNetworkUtils();

  MOCK_METHOD1(SendToClosestNode, void(const protobuf::Message& message));
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
