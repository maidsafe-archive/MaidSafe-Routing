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

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/test.h"

#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/tests/mock_network_utils.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/timer.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST(MessageHandlerTest, BEH_HandleInvalidMessage) {
  maidsafe::AsioService asio_service(5);
  asymm::Keys keys;
  keys.identity = RandomString(64);
  NonRoutingTable ntable(keys);
  RoutingTable table(keys, false);
  Timer timer(asio_service);
  MockNetworkUtils utils(table, ntable, timer);
  MessageHandler message_handler(asio_service, table, ntable, utils, timer);
  protobuf::Message message;
  message.set_hops_to_live(1);

  // MessageHandler should not try to use any network operations during this test.
  EXPECT_CALL(utils, SendToClosestNode(testing::_)).Times(0);
  EXPECT_CALL(utils, SendToDirect(testing::_, testing::_)).Times(0);
  message_handler.HandleMessage(message);  // Handle uninitialised message
  message.set_routing_message(true);
  message.set_direct(true);
  message.set_request(true);
  message.set_client_node(true);
  message_handler.HandleMessage(message);  // Handle minimally initialised message
}

TEST(MessageHandlerTest, BEH_HandleRelay) {
  maidsafe::AsioService asio_service(5);
  asymm::Keys keys;
  keys.identity = RandomString(64);
  NonRoutingTable ntable(keys);
  RoutingTable table(keys, false);
  Timer timer(asio_service);
  MockNetworkUtils utils(table, ntable, timer);
  MessageHandler message_handler(asio_service, table, ntable, utils, timer);
  protobuf::Message message;
  message.set_hops_to_live(1);
  message.set_routing_message(true);
  message.set_direct(true);
  message.set_request(true);
  message.set_client_node(true);
  {  // Handle direct relay request to self
    EXPECT_CALL(utils, SendToClosestNode(testing::_)).Times(0);
    EXPECT_CALL(utils, SendToDirect(testing::_, testing::_)).Times(0);
    message.set_destination_id(table.kKeys().identity);
    std::string relay_id(RandomString(64)), relay_connection_id(RandomString(64));
    message.set_relay_connection_id(relay_connection_id);
    message.set_relay_id(relay_id);
    message_handler.HandleMessage(message);
  }
  {  // Handle direct relay request to other
    EXPECT_CALL(utils, SendToClosestNode(testing::_)).Times(1).RetiresOnSaturation();
    message.set_hops_to_live(1);
    message.set_destination_id(RandomString(64));
    message_handler.HandleMessage(message);
  }
  {  // Handle FindNodes on small network
    EXPECT_CALL(utils, SendToClosestNode(testing::_)).Times(0);
    message.set_type(static_cast<uint32_t>(MessageType::kFindNodes));
    message.set_hops_to_live(1);
    message_handler.HandleMessage(message);
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
