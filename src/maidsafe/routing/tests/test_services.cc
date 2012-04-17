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

#include <memory>
#include <vector>
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/service.h"
#include "maidsafe/routing/tests/test_utils.h"
#include "maidsafe/routing/log.h"


namespace maidsafe {
namespace routing {
namespace test {


TEST(Services, BEH_PingM) {
  asymm::Keys keys;
  keys.identity = RandomString(64);
  RoutingTable RT(keys);
  NodeInfo node;
  protobuf::Message message = rpcs::Ping(NodeId(keys.identity), "me");
  service::Ping(RT, message);
  EXPECT_TRUE(message.IsInitialized());
  protobuf::PingResponse ping_response;
  EXPECT_FALSE(ping_response.ParseFromString(message.data())); // not us
  message = rpcs::Ping(NodeId(keys.identity), RandomString(64));
  EXPECT_TRUE(ping_response.ParseFromString(message.data())); // is us
  // TODO(dirvine) check all elements as expected
}

TEST(Services, BEH_Connect) {

  boost::asio::ip::udp::endpoint our_endpoint;
  our_endpoint.address().from_string("192.168.1.1");
  our_endpoint.port(5000);
  asymm::Keys keys;
  keys.identity = RandomString(64);
  RoutingTable RT(keys);
  NodeInfo node;
  rudp::ManagedConnections rudp;
  protobuf::Message message = rpcs::Connect(NodeId("dav"), our_endpoint, "id");
  service::Connect(RT, rudp, message);
  EXPECT_TRUE(message.IsInitialized());
    // TODO(dirvine) check all elements as expected
}

TEST(Services, BEH_FindNodes) {
  ASSERT_TRUE(rpcs::FindNodes(NodeId("david")).IsInitialized());
}





}  // namespace test
}  // namespace routing
}  // namespace maidsafe