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

#ifndef MAIDSAFE_ROUTING_TESTS_MOCK_RESPONSE_HANDLER_H_
#define MAIDSAFE_ROUTING_TESTS_MOCK_RESPONSE_HANDLER_H_

#include "gmock/gmock.h"

#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/response_handler.h"

namespace maidsafe {

namespace routing {

namespace test {

class MockResponseHandler : public ResponseHandler {
 public:
  MockResponseHandler(RoutingTable& routing_table, ClientRoutingTable& client_routing_table,
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
