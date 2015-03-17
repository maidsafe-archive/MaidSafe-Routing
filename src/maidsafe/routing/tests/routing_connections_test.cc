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


#include "asio/use_future.hpp"

#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/connections.h"
#include "maidsafe/routing/tests/utils/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

static SerialisedMessage str_to_msg(const std::string& str) {
  return SerialisedMessage(str.begin(), str.end());
}

static std::string msg_to_str(const SerialisedMessage& msg) {
  return std::string(msg.begin(), msg.end());
}

TEST(ConnectionsTest, FUNC_TwoConnections) {
  bool c1_finished = false;
  bool c2_finished = false;

  asio::io_service ios;

  Address c1_id(MakeIdentity());
  Address c2_id(MakeIdentity());

  Connections c1(ios, c1_id);
  Connections c2(ios, c2_id);

  unsigned short port = 8080;

  c1.Accept(port, nullptr,
      [&](asio::error_code error, Connections::AcceptResult result) {
        ASSERT_FALSE(error);
        ASSERT_EQ(result.his_address, c2.OurId());
        ASSERT_EQ(result.our_endpoint.port(), port);

        c1.Send(result.his_address,
                str_to_msg("hello"),
                [&](asio::error_code error) {
                  ASSERT_FALSE(error);
                  c1.Shutdown();
                  c1_finished = true;
                });
      });

  c2.Connect(asio::ip::udp::endpoint(asio::ip::address_v4::loopback(), port),
      [&](asio::error_code error, Connections::ConnectResult result) {
        ASSERT_FALSE(error);
        ASSERT_EQ(result.his_address, c1.OurId());

        c2.Receive([&, result](asio::error_code error, Connections::ReceiveResult recv_result) {
          ASSERT_FALSE(error);
          ASSERT_EQ(recv_result.his_address, result.his_address);
          ASSERT_EQ(msg_to_str(recv_result.message), "hello");

          c2.Shutdown();
          c2_finished = true;
        });
      });

  ios.run();

  ASSERT_TRUE(c1_finished && c2_finished);
}

TEST(ConnectionsTest, FUNC_TwoConnectionsWithFutures) {
  Address c1_id(MakeIdentity());
  Address c2_id(MakeIdentity());

  asio::io_service ios;

  Connections c1(ios, c1_id);
  Connections c2(ios, c2_id);

  std::thread thread([&]() { ios.run(); });

  unsigned short port = 8080;

  auto accept_f  = c1.Accept(port, nullptr, asio::use_future);
  auto connect_f = c2.Connect(asio::ip::udp::endpoint(asio::ip::address_v4::loopback(), port), asio::use_future);

  auto accept_result  = accept_f.get();
  auto connect_result = connect_f.get();

  ASSERT_EQ(accept_result.his_address, c2.OurId());
  ASSERT_EQ(accept_result.our_endpoint.port(), port);

  ASSERT_EQ(connect_result.his_address, c1.OurId());

  auto recv_f = c2.Receive(asio::use_future);
  auto send_f = c1.Send(accept_result.his_address, str_to_msg("hello"), asio::use_future);

  send_f.get();
  recv_f.get();

  c1.Shutdown();
  c2.Shutdown();

  thread.join();
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
