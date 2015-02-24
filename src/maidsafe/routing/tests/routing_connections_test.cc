/*  Copyright 2014 MaidSafe.net limited

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


#include "maidsafe/common/node_id.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/connections.h"
#include "maidsafe/routing/tests/utils/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST(ConnectionsTest, FUNC_TwoConnections) {
  NodeId c1_id(NodeId(RandomString(NodeId::kSize)));
  NodeId c2_id(NodeId(RandomString(NodeId::kSize)));

  Connections c1(c1_id, [](NodeId, const std::vector<unsigned char>&){},
                        [](NodeId){});

  Connections c2(c2_id, [](NodeId, const std::vector<unsigned char>&){},
                        [](NodeId){});

  unsigned short port = 8080;

  c1.Accept(port, [=](asio::error_code error, asio::ip::udp::endpoint, NodeId) {
      });

  c1.Connect(asio::ip::udp::endpoint(asio::ip::address_v4::loopback(), port),
      [=](asio::error_code error, NodeId) {
      });
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
