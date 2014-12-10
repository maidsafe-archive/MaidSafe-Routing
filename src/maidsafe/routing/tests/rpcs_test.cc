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

#include <chrono>
#include <memory>
#include <vector>

#include "boost/asio/ip/address.hpp"

#include "maidsafe/common/log.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // unnamed namespace

TEST(RpcsTest, BEH_PingMessageInitialised) {
  // check with assert in debug mode, should NEVER fail
  std::string destination = RandomString(64);
  ASSERT_TRUE(rpcs::Ping(NodeId(destination), "me").IsInitialized());
}

TEST(RpcsTest, BEH_PingMessageNode) {
  std::string source(RandomString(64)), destination(RandomString(64));
  protobuf::Message message = rpcs::Ping(NodeId(destination), source);
  protobuf::PingRequest ping_request;
  EXPECT_TRUE(ping_request.ParseFromString(message.data(0)));  // us
  EXPECT_TRUE(ping_request.ping());
  EXPECT_TRUE(ping_request.has_timestamp());
  EXPECT_TRUE(ping_request.timestamp() > GetTimeStamp() - 2000);
  EXPECT_TRUE(ping_request.timestamp() < GetTimeStamp() + 1000);
  EXPECT_EQ(destination, message.destination_id());
  EXPECT_EQ(source, message.source_id());
  EXPECT_NE(0, message.data_size());
  EXPECT_EQ(1, message.replication());
  EXPECT_EQ(static_cast<int32_t>(MessageType::kPing), message.type());
  EXPECT_TRUE(message.request());
  EXPECT_FALSE(message.client_node());
  // EXPECT_FALSE(message.has_relay());
}

TEST(RpcsTest, BEH_ConnectMessageInitialised) {
  rudp::EndpointPair our_endpoint;
  our_endpoint.local =
      Endpoint(boost::asio::ip::address_v4::loopback(), maidsafe::test::GetRandomPort());
  our_endpoint.external =
      Endpoint(boost::asio::ip::address_v4::loopback(), maidsafe::test::GetRandomPort());
  ASSERT_TRUE(rpcs::Connect(NodeId(RandomString(64)), our_endpoint, NodeId(RandomString(64)),
                            NodeId(RandomString(64))).IsInitialized());
}

TEST(RpcsTest, BEH_ConnectMessageNode) {
  NodeInfo us(MakeNode());
  rudp::EndpointPair endpoint;
  endpoint.local =
      Endpoint(boost::asio::ip::address_v4::loopback(), maidsafe::test::GetRandomPort());
  endpoint.external =
      Endpoint(boost::asio::ip::address_v4::loopback(), maidsafe::test::GetRandomPort());
  std::string destination = RandomString(64);
  protobuf::Message message = rpcs::Connect(NodeId(destination), endpoint, us.id, us.connection_id);
  protobuf::ConnectRequest connect_request;
  EXPECT_TRUE(message.IsInitialized());
  EXPECT_TRUE(connect_request.ParseFromString(message.data(0)));  // us
  EXPECT_FALSE(connect_request.bootstrap());
  EXPECT_TRUE(connect_request.has_timestamp());
  EXPECT_TRUE(connect_request.timestamp() > GetTimeStamp() - 2000);
  EXPECT_TRUE(connect_request.timestamp() < GetTimeStamp() + 1000);
  EXPECT_EQ(us.id.string(), connect_request.contact().node_id());
  EXPECT_EQ(us.connection_id.string(), connect_request.contact().connection_id());
  EXPECT_EQ(destination, message.destination_id());
  EXPECT_EQ(us.id.string(), message.source_id());
  EXPECT_NE(0, message.data_size());
  EXPECT_TRUE(message.direct());
  EXPECT_EQ(1, message.replication());
  EXPECT_EQ(static_cast<int32_t>(MessageType::kConnect), message.type());
  EXPECT_TRUE(message.request());
  EXPECT_FALSE(message.client_node());
}

TEST(RpcsTest, BEH_ConnectMessageNodeRelayMode) {
  NodeInfo us(MakeNode());
  rudp::EndpointPair endpoint;
  endpoint.local =
      Endpoint(boost::asio::ip::address_v4::loopback(), maidsafe::test::GetRandomPort());
  endpoint.external =
      Endpoint(boost::asio::ip::address_v4::loopback(), maidsafe::test::GetRandomPort());
  std::string destination = RandomString(64);
  protobuf::Message message =
      rpcs::Connect(NodeId(destination), endpoint, us.id, us.connection_id, false,
                    rudp::NatType::kUnknown, true, NodeId(destination));
  protobuf::ConnectRequest connect_request;
  EXPECT_TRUE(message.IsInitialized());
  EXPECT_TRUE(connect_request.ParseFromString(message.data(0)));  // us
  EXPECT_FALSE(connect_request.bootstrap());
  EXPECT_TRUE(connect_request.has_timestamp());
  EXPECT_TRUE(connect_request.timestamp() > GetTimeStamp() - 2000);
  EXPECT_TRUE(connect_request.timestamp() < GetTimeStamp() + 1000);
  EXPECT_EQ(us.id.string(), connect_request.contact().node_id());
  EXPECT_EQ(us.connection_id.string(), connect_request.contact().connection_id());
  EXPECT_EQ(destination, message.destination_id());
  EXPECT_FALSE(message.has_source_id());
  EXPECT_NE(0, message.data_size());
  EXPECT_TRUE(message.direct());
  EXPECT_EQ(message.replication(), 1);
  EXPECT_EQ(static_cast<int32_t>(MessageType::kConnect), message.type());
  EXPECT_TRUE(message.request());
  EXPECT_FALSE(message.client_node());
  EXPECT_TRUE(message.has_relay_id());
  EXPECT_EQ(us.id.string(), message.relay_id());
}

TEST(RpcsTest, BEH_FindNodesMessageInitialised) {
  ASSERT_TRUE(
      rpcs::FindNodes(NodeId(RandomString(64)), NodeId(RandomString(64)), 8).IsInitialized());
}

TEST(RpcsTest, BEH_FindNodesMessageNode) {
  NodeInfo us(MakeNode());
  protobuf::Message message = rpcs::FindNodes(us.id, us.id, Parameters::closest_nodes_size);
  protobuf::FindNodesRequest find_nodes_request;
  EXPECT_TRUE(find_nodes_request.ParseFromString(message.data(0)));  // us
  EXPECT_EQ(static_cast<unsigned int>(find_nodes_request.num_nodes_requested()),
            Parameters::closest_nodes_size);
  EXPECT_EQ(us.id.string(), message.destination_id());
  EXPECT_TRUE(find_nodes_request.has_timestamp());
  EXPECT_TRUE(find_nodes_request.timestamp() > GetTimeStamp() - 2000);
  EXPECT_TRUE(find_nodes_request.timestamp() < GetTimeStamp() + 1000);
  EXPECT_EQ(us.id.string(), message.destination_id());
  EXPECT_EQ(us.id.string(), message.source_id());
  EXPECT_NE(0, message.data_size());
  EXPECT_FALSE(message.direct());
  EXPECT_EQ(1, message.replication());
  EXPECT_EQ(static_cast<int32_t>(MessageType::kFindNodes), message.type());
  EXPECT_TRUE(message.request());
  EXPECT_FALSE(message.client_node());
  EXPECT_FALSE(message.has_relay_id());
}

TEST(RpcsTest, BEH_FindNodesMessageNodeRelayMode) {
  NodeInfo us(MakeNode());
  protobuf::Message message = rpcs::FindNodes(us.id, us.id, Parameters::closest_nodes_size, true,
                                              NodeId(RandomString(NodeId::kSize)));
  protobuf::FindNodesRequest find_nodes_request;
  EXPECT_TRUE(find_nodes_request.ParseFromString(message.data(0)));  // us
  EXPECT_TRUE(find_nodes_request.num_nodes_requested() == Parameters::closest_nodes_size);
  EXPECT_EQ(us.id.string(), message.destination_id());
  EXPECT_TRUE(find_nodes_request.has_timestamp());
  EXPECT_TRUE(find_nodes_request.timestamp() > GetTimeStamp() - 2000);
  EXPECT_TRUE(find_nodes_request.timestamp() < GetTimeStamp() + 1000);
  EXPECT_EQ(us.id.string(), message.destination_id());
  EXPECT_FALSE(message.has_source_id());
  EXPECT_NE(0, message.data_size());
  EXPECT_FALSE(message.direct());
  EXPECT_EQ(1, message.replication());
  EXPECT_EQ(static_cast<int32_t>(MessageType::kFindNodes), message.type());
  EXPECT_TRUE(message.request());
  EXPECT_FALSE(message.client_node());
  EXPECT_TRUE(message.has_relay_id());
  EXPECT_EQ(us.id.string(), message.relay_id());
  NodeId node(message.relay_id());
  ASSERT_TRUE(node.IsValid());
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
