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

#ifndef MAIDSAFE_ROUTING_TESTS_MOCK_RESPONSE_HANDLER_H_
#define MAIDSAFE_ROUTING_TESTS_MOCK_RESPONSE_HANDLER_H_

#include <string>

#include "gmock/gmock.h"

#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/response_handler.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

namespace test {

class MockResponseHandler : public ResponseHandler {
 public:
  MockResponseHandler(RoutingTable& routing_table, ClientRoutingTable& client_routing_table,
                      Network& network_utils, PublicKeyHolder& public_key_holder);
  virtual ~MockResponseHandler();

  MOCK_METHOD1(Ping, void(protobuf::Message& message));
  MOCK_METHOD1(Connect, void(protobuf::Message& message));
  MOCK_METHOD1(FindNodes, void(const protobuf::Message& message));
  MOCK_METHOD1(ConnectSuccess, void(protobuf::Message& message));

 private:
  MockResponseHandler& operator=(const MockResponseHandler&);
  MockResponseHandler(const MockResponseHandler&);
};

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TESTS_MOCK_RESPONSE_HANDLER_H_
