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

#ifndef MAIDSAFE_ROUTING_TESTS_MOCK_RESPONSE_HANDLER_H_
#define MAIDSAFE_ROUTING_TESTS_MOCK_RESPONSE_HANDLER_H_

#include "gmock/gmock.h"

#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/response_handler.h"

namespace maidsafe {

namespace routing {

namespace test {

class MockResponseHandler : public ResponseHandler {
 public:
  MockResponseHandler(RoutingTable& routing_table, ClientRoutingTable& non_routing_table,
                   NetworkUtils& network_utils, GroupChangeHandler &group_change_handler);
  virtual ~MockResponseHandler();

  MOCK_METHOD1(Ping, void(protobuf::Message& message));
  MOCK_METHOD1(Connect, void(protobuf::Message& message));
  MOCK_METHOD1(FindNodes, void(const protobuf::Message& message));
  MOCK_METHOD1(ConnectSuccess, void(protobuf::Message& message));

 private:
  MockResponseHandler &operator=(const MockResponseHandler&);
  MockResponseHandler(const MockResponseHandler&);
};

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TESTS_MOCK_RESPONSE_HANDLER_H_
