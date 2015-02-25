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


#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/connections.h"
#include "maidsafe/routing/tests/utils/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST(ConnectionsTest, FUNC_TwoConnections) {
  boost::asio::io_service ios;

  NodeId c1_id(NodeId(RandomString(NodeId::kSize)));
  NodeId c2_id(NodeId(RandomString(NodeId::kSize)));

  Connections c1(ios, c1_id);
  Connections c2(ios, c2_id);

  unsigned short port = 8080;

  bool c1_finished = false;
  bool c2_finished = false;

  c1.Accept(port,
      [&](asio::error_code error, asio::ip::udp::endpoint, NodeId his_id) {
        ASSERT_FALSE(error);
        ASSERT_EQ(his_id, c2.OurId());
        std::string msg = "hello";

        c1.Send(his_id,
                std::vector<unsigned char>(msg.begin(), msg.end()),
                [&](asio::error_code error) {
                  ASSERT_FALSE(error);
                  c1.Shutdown();
                  c1_finished = true;
                });
      });

  c2.Connect(asio::ip::udp::endpoint(asio::ip::address_v4::loopback(), port),
      [&](asio::error_code error, NodeId his_id) {
        ASSERT_FALSE(error);
        ASSERT_EQ(his_id, c1.OurId());

        c2.Receive([&, his_id](asio::error_code error,
                       NodeId sender_id,
                       const std::vector<unsigned char>& bytes) {
          ASSERT_FALSE(error);
          ASSERT_EQ(sender_id, his_id);
          ASSERT_EQ(std::string(bytes.begin(), bytes.end()), "hello");

          c2.Shutdown();
          c2_finished = true;
        });
      });

  ios.run();

  ASSERT_TRUE(c1_finished && c2_finished);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
