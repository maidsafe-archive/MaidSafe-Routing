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

#ifndef MAIDSAFE_ROUTING_TESTS_MOCK_SERVICE_H_
#define MAIDSAFE_ROUTING_TESTS_MOCK_SERVICE_H_

#include "gmock/gmock.h"

#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/service.h"

namespace maidsafe {

namespace routing {

namespace test {

class MockService : public Service {
 public:
  MockService(RoutingTable& routing_table, ClientRoutingTable& client_routing_table,
                   NetworkUtils& network_utils);
  virtual ~MockService();

  MOCK_METHOD1(Ping, void(protobuf::Message& message));
  MOCK_METHOD1(Connect, void(protobuf::Message& message));
  MOCK_METHOD1(FindNodes, void(protobuf::Message& message));

 private:
  MockService &operator=(const MockService&);
  MockService(const MockService&);
};

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TESTS_MOCK_SERVICE_H_
