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
#include "maidsafe/routing/routing_pb.h"
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
  asymm::Keys keys;
  keys.identity = RandomString(64);
  NodeInfo node;
  std::string destination = RandomString(64);
  protobuf::Message message = rpcs::Ping(NodeId(destination), keys.identity);
  protobuf::PingRequest ping_request;
  EXPECT_TRUE(ping_request.ParseFromString(message.data(0)));  // us
  EXPECT_TRUE(ping_request.ping());
  EXPECT_TRUE(ping_request.has_timestamp());
  EXPECT_TRUE(ping_request.timestamp() > static_cast<int32_t>(GetTimeStamp() - 2));
  EXPECT_TRUE(ping_request.timestamp() < static_cast<int32_t>(GetTimeStamp() + 1));
  EXPECT_EQ(destination, message.destination_id());
  EXPECT_EQ(keys.identity, message.source_id());
  EXPECT_NE(0, message.data_size());
  EXPECT_EQ(1, message.replication());
  EXPECT_EQ(static_cast<int32_t>(MessageType::kPing), message.type());
  EXPECT_TRUE(message.request());
  EXPECT_FALSE(message.client_node());
  // EXPECT_FALSE(message.has_relay());
}

TEST(RpcsTest, BEH_ConnectMessageInitialised) {
  rudp::EndpointPair our_endpoint;
  our_endpoint.local = Endpoint(boost::asio::ip::address_v4::loopback(), maidsafe::GetRandomPort());
  our_endpoint.external = Endpoint(boost::asio::ip::address_v4::loopback(), maidsafe::GetRandomPort());
  ASSERT_TRUE(rpcs::Connect(NodeId(RandomString(64)), our_endpoint,
                            NodeId(RandomString(64))).IsInitialized());
}

TEST(RpcsTest, BEH_ConnectMessageNode) {
  NodeInfo us(MakeNode());
  rudp::EndpointPair endpoint;
  endpoint.local = Endpoint(boost::asio::ip::address_v4::loopback(), maidsafe::GetRandomPort());
  endpoint.external = Endpoint(boost::asio::ip::address_v4::loopback(), maidsafe::GetRandomPort());
  std::string destination = RandomString(64);
  protobuf::Message message = rpcs::Connect(NodeId(destination), endpoint, us.node_id);
  protobuf::ConnectRequest connect_request;
  EXPECT_TRUE(message.IsInitialized());
  EXPECT_TRUE(connect_request.ParseFromString(message.data(0)));  // us
  EXPECT_FALSE(connect_request.bootstrap());
  EXPECT_TRUE(connect_request.has_timestamp());
  EXPECT_TRUE(connect_request.timestamp() > static_cast<int32_t>(GetTimeStamp() - 2));
  EXPECT_TRUE(connect_request.timestamp() < static_cast<int32_t>(GetTimeStamp() + 1));
  EXPECT_EQ(destination, message.destination_id());
  EXPECT_EQ(us.node_id.String(), message.source_id());
  EXPECT_NE(0, message.data_size());
  EXPECT_TRUE(message.direct());
  EXPECT_EQ(1, message.replication());
  EXPECT_EQ(static_cast<int32_t>(MessageType::kConnect), message.type());
  EXPECT_TRUE(message.request());
  EXPECT_FALSE(message.client_node());
  // EXPECT_FALSE(message.has_relay());
}

TEST(RpcsTest, BEH_ConnectMessageNodeRelayMode) {
  NodeInfo us(MakeNode());
  rudp::EndpointPair endpoint;
  endpoint.local = Endpoint(boost::asio::ip::address_v4::loopback(), maidsafe::GetRandomPort());
  endpoint.external = Endpoint(boost::asio::ip::address_v4::loopback(), maidsafe::GetRandomPort());
  std::string destination = RandomString(64);
  protobuf::Message message = rpcs::Connect(NodeId(destination), endpoint, us.node_id,
                                            std::vector<std::string>(), false,
                                            rudp::NatType::kUnknown, true, NodeId(destination));
  protobuf::ConnectRequest connect_request;
  EXPECT_TRUE(message.IsInitialized());
  EXPECT_TRUE(connect_request.ParseFromString(message.data(0)));  // us
  EXPECT_FALSE(connect_request.bootstrap());
  EXPECT_TRUE(connect_request.has_timestamp());
  EXPECT_TRUE(connect_request.timestamp() > static_cast<int32_t>(GetTimeStamp() - 2));
  EXPECT_TRUE(connect_request.timestamp() < static_cast<int32_t>(GetTimeStamp() + 1));
  EXPECT_EQ(destination, message.destination_id());
  EXPECT_FALSE(message.has_source_id());
  EXPECT_NE(0, message.data_size());
  EXPECT_TRUE(message.direct());
  EXPECT_EQ(message.replication(), 1);
  EXPECT_EQ(static_cast<int32_t>(MessageType::kConnect), message.type());
  EXPECT_TRUE(message.request());
  EXPECT_FALSE(message.client_node());
  EXPECT_TRUE(message.has_relay_id());
  EXPECT_EQ(us.node_id.String(), message.relay_id());
}

TEST(RpcsTest, BEH_FindNodesMessageInitialised) {
  ASSERT_TRUE(rpcs::FindNodes(NodeId(RandomString(64)),
                              NodeId(RandomString(64)),
                              8).IsInitialized());
}

TEST(RpcsTest, BEH_FindNodesMessageNode) {
  NodeInfo us(MakeNode());
  protobuf::Message message = rpcs::FindNodes(us.node_id, us.node_id, 8);
  protobuf::FindNodesRequest find_nodes_request;
  EXPECT_TRUE(find_nodes_request.ParseFromString(message.data(0)));  // us
  EXPECT_TRUE(find_nodes_request.num_nodes_requested() == Parameters::closest_nodes_size);
  EXPECT_EQ(us.node_id.String(), find_nodes_request.target_node());
  EXPECT_TRUE(find_nodes_request.has_timestamp());
  EXPECT_TRUE(find_nodes_request.timestamp() > static_cast<int32_t>(GetTimeStamp() - 2));
  EXPECT_TRUE(find_nodes_request.timestamp() < static_cast<int32_t>(GetTimeStamp() + 1));
  EXPECT_EQ(us.node_id.String(), message.destination_id());
  EXPECT_EQ(us.node_id.String(), message.source_id());
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
  Endpoint relay_endpoint(boost::asio::ip::address_v4::loopback(), maidsafe::GetRandomPort());
  protobuf::Message message = rpcs::FindNodes(us.node_id, us.node_id, 8, true,
                                              NodeId(NodeId::kRandomId));
  protobuf::FindNodesRequest find_nodes_request;
  EXPECT_TRUE(find_nodes_request.ParseFromString(message.data(0)));  // us
  EXPECT_TRUE(find_nodes_request.num_nodes_requested() == Parameters::closest_nodes_size);
  EXPECT_EQ(us.node_id.String(), find_nodes_request.target_node());
  EXPECT_TRUE(find_nodes_request.has_timestamp());
  EXPECT_TRUE(find_nodes_request.timestamp() > static_cast<int32_t>(GetTimeStamp() - 2));
  EXPECT_TRUE(find_nodes_request.timestamp() < static_cast<int32_t>(GetTimeStamp() + 1));
  EXPECT_EQ(us.node_id.String(), message.destination_id());
  EXPECT_FALSE(message.has_source_id());
  EXPECT_NE(0, message.data_size());
  EXPECT_FALSE(message.direct());
  EXPECT_EQ(1, message.replication());
  EXPECT_EQ(static_cast<int32_t>(MessageType::kFindNodes), message.type());
  EXPECT_TRUE(message.request());
  EXPECT_FALSE(message.client_node());
  EXPECT_TRUE(message.has_relay_id());
  EXPECT_EQ(us.node_id.String(), message.relay_id());
  NodeId node(message.relay_id());
  ASSERT_TRUE(node.IsValid());
}

TEST(RpcsTest, BEH_ProxyConnectMessageInitialised) {
  std::string destination = RandomString(64);
  std::string source = RandomString(64);
  rudp::EndpointPair endpoint_pair;
  endpoint_pair.external =  Endpoint(boost::asio::ip::address_v4::loopback(), maidsafe::GetRandomPort());
  endpoint_pair.local =  Endpoint(boost::asio::ip::address_v4::loopback(), maidsafe::GetRandomPort());
  ASSERT_TRUE(rpcs::ProxyConnect(NodeId(destination), NodeId(source),
                                 endpoint_pair).IsInitialized());
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
