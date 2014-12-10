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
#include <future>
#include <memory>
#include <vector>

#include "boost/filesystem/exception.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/passport/passport.h"
#include "maidsafe/rudp/return_codes.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/network.h"
#include "maidsafe/routing/client_routing_table.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/tests/test_utils.h"
#include "maidsafe/routing/acknowledgement.h"

namespace maidsafe {

namespace routing {

namespace test {

namespace bptime = boost::posix_time;

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

void SortFromThisNode(const NodeId& from, std::vector<NodeInfoAndPrivateKey> nodes) {
  std::sort(nodes.begin(), nodes.end(),
            [from](const NodeInfoAndPrivateKey& i, const NodeInfoAndPrivateKey& j) {
    return (i.node_info.id ^ from) < (j.node_info.id ^ from);
  });
}

}  // anonymous namespace

TEST(NetworkTest, BEH_ProcessSendDirectInvalidEndpoint) {
  protobuf::Message message;
  message.set_routing_message(true);
  message.set_client_node(false);
  message.add_data("data");
  message.set_request(true);
  message.set_direct(true);
  message.set_type(10);
  message.set_hops_to_live(Parameters::hops_to_live);
  NodeId node_id(RandomString(NodeId::kSize));
  AsioService asio_service(2);
  Acknowledgement acknowledgement(node_id, asio_service);
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair());
  ClientRoutingTable client_routing_table(routing_table.kNodeId());
  Network network(routing_table, client_routing_table, acknowledgement);
  network.SendToClosestNode(message);
}

TEST(NetworkTest, BEH_ProcessSendUnavailableDirectEndpoint) {
  protobuf::Message message;
  message.set_routing_message(true);
  message.set_client_node(false);
  message.set_request(true);
  message.add_data("data");
  message.set_direct(true);
  message.set_type(10);
  message.set_hops_to_live(Parameters::hops_to_live);
  NodeId node_id(RandomString(NodeId::kSize));
  AsioService asio_service(2);
  Acknowledgement acknowledgement(node_id, asio_service);
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair());
  ClientRoutingTable client_routing_table(routing_table.kNodeId());
  Endpoint endpoint(GetLocalIp(), maidsafe::test::GetRandomPort());
  Network network(routing_table, client_routing_table, acknowledgement);
  network.SendToDirect(message, NodeId(RandomString(NodeId::kSize)),
                       NodeId(RandomString(NodeId::kSize)));
}

TEST(NetworkTest, FUNC_ProcessSendDirectEndpoint) {
  const int kMessageCount(10);
  rudp::ManagedConnections rudp1, rudp2;
  Endpoint endpoint1(GetLocalIp(), maidsafe::test::GetRandomPort());
  Endpoint endpoint2(GetLocalIp(), maidsafe::test::GetRandomPort());

  std::promise<bool> test_completion_promise;
  auto test_completion_future = test_completion_promise.get_future();
  bool promised(true);
  uint32_t expected_message_at_node(kMessageCount + 1);
  uint32_t message_count_at_node2(0);

  std::promise<bool> connection_completion_promise;
  auto connection_completion_future = connection_completion_promise.get_future();

  protobuf::Message sent_message;
  sent_message.set_destination_id(NodeId(RandomString(64)).string());
  sent_message.set_routing_message(true);
  sent_message.set_request(true);
  sent_message.add_data(std::string(1024 * 256, 'A'));
  sent_message.set_direct(true);
  sent_message.set_type(10);
  sent_message.set_client_node(false);
  sent_message.set_hops_to_live(Parameters::hops_to_live);

  rudp::MessageReceivedFunctor message_received_functor1 = [](const std::string& message) {
    LOG(kInfo) << " -- Received: " << message;
  };

  rudp::MessageReceivedFunctor message_received_functor2 = [&](const std::string& message) {
    ++message_count_at_node2;
    LOG(kVerbose) << " Node -2- Received: " << message.substr(0, 16)
                  << ", total count = " << message_count_at_node2;
    protobuf::Message received_message;
    if (received_message.ParseFromString(message))
      EXPECT_EQ(sent_message.data(0), received_message.data(0));
    else
      EXPECT_EQ("validation", message.substr(0, 10));
    if (promised && (message_count_at_node2 == expected_message_at_node)) {
      test_completion_promise.set_value(true);
      promised = false;
    }
  };

  rudp::MessageReceivedFunctor message_received_functor3 = [&](const std::string& message) {
    LOG(kInfo) << " -- Received: " << message;
    if ("validation" == message.substr(0, 10)) {
      connection_completion_promise.set_value(true);
      LOG(kInfo) << " -- Set promise";
    }
  };

  rudp::ConnectionLostFunctor connection_lost_functor = [](const NodeId& node_id) {
    LOG(kInfo) << " -- Lost Connection with : " << HexSubstr(node_id.string());
  };

  auto pmid1(passport::CreatePmidAndSigner().first);
  NodeId node_id1(pmid1.name()->string());
  auto private_key1(std::make_shared<asymm::PrivateKey>(pmid1.private_key()));
  auto public_key1(std::make_shared<asymm::PublicKey>(pmid1.public_key()));
  rudp::NatType nat_type;
  auto a1 = std::async(std::launch::async, [&, this ]()->NodeId {
    std::vector<Endpoint> bootstrap_endpoint(1, endpoint2);
    NodeId chosen_bootstrap_peer;
    if (rudp1.Bootstrap(bootstrap_endpoint, message_received_functor1, connection_lost_functor,
                        node_id1, private_key1, public_key1, chosen_bootstrap_peer, nat_type,
                        endpoint1) != kSuccess) {
      chosen_bootstrap_peer = NodeId();
    }
    return chosen_bootstrap_peer;
  });

  auto pmid2(passport::CreatePmidAndSigner().first);
  NodeId node_id2(pmid2.name()->string());
  auto private_key2(std::make_shared<asymm::PrivateKey>(pmid2.private_key()));
  auto public_key2(std::make_shared<asymm::PublicKey>(pmid2.public_key()));
  auto a2 = std::async(std::launch::async, [&, this ]()->NodeId {
    std::vector<Endpoint> bootstrap_endpoint(1, endpoint1);
    NodeId chosen_bootstrap_peer;
    if (rudp2.Bootstrap(bootstrap_endpoint, message_received_functor2, connection_lost_functor,
                        node_id2, private_key2, public_key2, chosen_bootstrap_peer, nat_type,
                        endpoint2) != kSuccess) {
      chosen_bootstrap_peer = NodeId();
    }
    return chosen_bootstrap_peer;
  });

  EXPECT_EQ(node_id2, a1.get());  // wait for promise !
  EXPECT_EQ(node_id1, a2.get());  // wait for promise !
  rudp::EndpointPair endpoint_pair_1, endpoint_pair_2, endpoint_pair_3;
  endpoint_pair_1.local = endpoint1;
  endpoint_pair_2.local = endpoint2;

  Sleep(std::chrono::milliseconds(250));
  EXPECT_EQ(rudp::kBootstrapConnectionAlreadyExists,
            rudp1.GetAvailableEndpoint(node_id2, endpoint_pair_2, endpoint_pair_1, nat_type));
  EXPECT_EQ(rudp::kBootstrapConnectionAlreadyExists,
            rudp2.GetAvailableEndpoint(node_id1, endpoint_pair_1, endpoint_pair_2, nat_type));

  EXPECT_EQ(kSuccess, rudp1.Add(node_id2, endpoint_pair_2, "validation_1->2"));
  EXPECT_EQ(kSuccess, rudp2.Add(node_id1, endpoint_pair_1, "validation_2->1"));
  Endpoint endpoint;
  rudp1.MarkConnectionAsValid(node_id2, endpoint);
  rudp2.MarkConnectionAsValid(node_id1, endpoint);
  LOG(kVerbose) << " ------------------------   Zero state setup done  ----------------------- ";

  NodeId node_id(RandomString(NodeId::kSize));
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair());
  NodeId node_id3(routing_table.kNodeId());
  AsioService asio_service(2);
  Acknowledgement acknowledgement(node_id, asio_service);
  ClientRoutingTable client_routing_table(routing_table.kNodeId());
  Network network(routing_table, client_routing_table, acknowledgement);

  ScopedBootstrapFile bootstrap_file({endpoint2});
  EXPECT_EQ(kSuccess, network.Bootstrap(message_received_functor3, connection_lost_functor));
  rudp::NatType this_nat_type;
  // RUDP NOTE: sleep here will let the test pass. Seems like rudp doesn't updates info about
  // bootstrap node endpoints before returning from Bootstrap() and so some times this
  // update happens after GetAvailableEndpoint called by test code and so it doesn't return
  // kBootstrapConnectionAlreadyExists even though it has bootstrapped off the same node
  // std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(
      rudp::kBootstrapConnectionAlreadyExists,
      network.GetAvailableEndpoint(node_id2, endpoint_pair_2, endpoint_pair_3, this_nat_type));
  EXPECT_EQ(rudp::kBootstrapConnectionAlreadyExists,
            rudp2.GetAvailableEndpoint(node_id3, endpoint_pair_3, endpoint_pair_2, this_nat_type));
  EXPECT_EQ(rudp::kSuccess, network.Add(node_id2, endpoint_pair_2, "validation_3->2"));

  EXPECT_EQ(kSuccess, rudp2.Add(node_id3, endpoint_pair_3, "validation_2->3"));
  if (connection_completion_future.wait_for(std::chrono::seconds(10)) !=
      std::future_status::ready) {
    ASSERT_TRUE(false) << "Failed waiting for node-3 to receive validation data";
  }

  for (auto i(0); i != kMessageCount; ++i) {
    network.SendToDirect(sent_message, node_id2, node_id2);
    Sleep(std::chrono::milliseconds(100));
  }
  if (test_completion_future.wait_for(std::chrono::seconds(60)) != std::future_status::ready) {
    ASSERT_TRUE(false) << "Failed waiting for node-2 to receive " << expected_message_at_node
                       << "messsages";
  }
}

// RT with only 1 active node and 7 inactive node
TEST(NetworkTest, FUNC_ProcessSendRecursiveSendOn) {
  const int kMessageCount(1);
  rudp::ManagedConnections rudp1, rudp2;
  Endpoint endpoint1(GetLocalIp(), maidsafe::test::GetRandomPort());
  Endpoint endpoint2(GetLocalIp(), maidsafe::test::GetRandomPort());

  std::promise<bool> test_completion_promise;
  auto test_completion_future = test_completion_promise.get_future();
  bool promised(true);
  uint32_t expected_message_at_node(kMessageCount + 1);
  uint32_t message_count_at_node2(0);

  std::promise<bool> connection_completion_promise;
  auto connection_completion_future = connection_completion_promise.get_future();

  protobuf::Message sent_message;
  //  sent_message.add_data(std::string(1024 * 256, 'B'));
  sent_message.add_data(std::string(10, 'B'));
  sent_message.set_direct(true);
  sent_message.set_type(10);
  sent_message.set_routing_message(true);
  sent_message.set_request(true);
  sent_message.set_client_node(false);
  sent_message.set_hops_to_live(Parameters::hops_to_live);
  sent_message.set_source_id(NodeId(RandomString(NodeId::kSize)).string());
  NodeId node_id(RandomString(NodeId::kSize));
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair());
  NodeId node_id3(routing_table.kNodeId());
  AsioService asio_service(2);
  Acknowledgement acknowledgement(node_id, asio_service);
  ClientRoutingTable client_routing_table(routing_table.kNodeId());
  Network network(routing_table, client_routing_table, acknowledgement);

  rudp::MessageReceivedFunctor message_received_functor1 = [](const std::string& message) {
    LOG(kInfo) << " -- Received: " << message;
  };

  rudp::MessageReceivedFunctor message_received_functor2 = [&](const std::string& message) {
    ++message_count_at_node2;
    LOG(kVerbose) << " -2- Received: " << message.substr(0, 16)
                  << ", total count = " << message_count_at_node2;
    protobuf::Message received_message;
    if (received_message.ParseFromString(message))
      EXPECT_EQ(sent_message.data(0), received_message.data(0));
    else
      EXPECT_EQ("validation", message.substr(0, 10));
    if (promised && (message_count_at_node2 == expected_message_at_node)) {
      test_completion_promise.set_value(true);
      promised = false;
    }
  };

  rudp::MessageReceivedFunctor message_received_functor3 = [&](const std::string& message) {
    LOG(kInfo) << " -- Received: " << message;
    if ("validation" == message.substr(0, 10)) {
      connection_completion_promise.set_value(true);
      LOG(kInfo) << " -- Set promise";
    }
  };

  rudp::ConnectionLostFunctor connection_lost_functor = [](const NodeId& node_id) {
    LOG(kInfo) << " -- Lost Connection with : " << HexSubstr(node_id.string());
  };

  rudp::ConnectionLostFunctor connection_lost_functor3 = [&](const NodeId& node_id) {
    routing_table.DropNode(node_id, true);
    LOG(kInfo) << " -- Lost Connection with : " << HexSubstr(node_id.string());
  };

  auto pmid1(passport::CreatePmidAndSigner().first);
  NodeId node_id1(pmid1.name()->string());
  auto private_key1(std::make_shared<asymm::PrivateKey>(pmid1.private_key()));
  auto public_key1(std::make_shared<asymm::PublicKey>(pmid1.public_key()));
  rudp::NatType nat_type;
  NodeInfoAndPrivateKey node2 = MakeNodeInfoAndKeys();
  auto pmid2(passport::CreatePmidAndSigner().first);
  NodeId node_id2(pmid2.name()->string());
  auto private_key2(std::make_shared<asymm::PrivateKey>(pmid2.private_key()));
  auto public_key2(std::make_shared<asymm::PublicKey>(pmid2.public_key()));

  auto a1 = std::async(std::launch::async, [ =, &rudp1, &nat_type ]()->NodeId {
    std::vector<Endpoint> bootstrap_endpoint(1, endpoint2);
    NodeId chosen_bootstrap_peer;
    if (rudp1.Bootstrap(bootstrap_endpoint, message_received_functor1, connection_lost_functor,
                        node_id1, private_key1, public_key1, chosen_bootstrap_peer, nat_type,
                        endpoint1) != kSuccess) {
      chosen_bootstrap_peer = NodeId();
    }
    return chosen_bootstrap_peer;
  });
  auto a2 = std::async(std::launch::async, [ =, &rudp2, &nat_type ]()->NodeId {
    std::vector<Endpoint> bootstrap_endpoint(1, endpoint1);
    NodeId chosen_bootstrap_peer;
    if (rudp2.Bootstrap(bootstrap_endpoint, message_received_functor2, connection_lost_functor,
                        node_id2, private_key2, public_key2, chosen_bootstrap_peer, nat_type,
                        endpoint2) != kSuccess) {
      chosen_bootstrap_peer = NodeId();
    }
    return chosen_bootstrap_peer;
  });

  EXPECT_EQ(node_id2, a1.get());  // wait for promise !
  EXPECT_EQ(node_id1, a2.get());  // wait for promise !
  rudp::EndpointPair endpoint_pair_1, endpoint_pair_2;
  endpoint_pair_1.local = endpoint1;
  endpoint_pair_2.local = endpoint2;

  Sleep(std::chrono::milliseconds(250));
  EXPECT_EQ(rudp::kBootstrapConnectionAlreadyExists,
            rudp1.GetAvailableEndpoint(node_id2, endpoint_pair_2, endpoint_pair_1, nat_type));
  EXPECT_EQ(rudp::kBootstrapConnectionAlreadyExists,
            rudp2.GetAvailableEndpoint(node_id1, endpoint_pair_1, endpoint_pair_2, nat_type));

  EXPECT_EQ(kSuccess, rudp1.Add(node_id2, endpoint_pair_2, "validation_1->2"));
  EXPECT_EQ(kSuccess, rudp2.Add(node_id1, endpoint_pair_1, "validation_2->1"));
  Endpoint endpoint;
  rudp1.MarkConnectionAsValid(node_id2, endpoint);
  rudp2.MarkConnectionAsValid(node_id1, endpoint);
  LOG(kVerbose) << " ------------------------   Zero state setup done  ----------------------- ";

  ScopedBootstrapFile bootstrap_file({endpoint2});

  EXPECT_EQ(kSuccess, network.Bootstrap(message_received_functor3, connection_lost_functor3));
  rudp::EndpointPair endpoint_pair2, endpoint_pair3;
  rudp::NatType this_nat_type;
  // RUDP NOTE: sleep here will let the test pass. Seems like rudp doesn't updates info about
  // bootstrap node endpoints before returning from Bootstrap() and so some times this
  // update happens after GetAvailableEndpoint called by test code and so it doesn't return
  // kBootstrapConnectionAlreadyExists even though it has bootstrapped off the same node
  // std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(rudp::kBootstrapConnectionAlreadyExists,
            network.GetAvailableEndpoint(node_id2, endpoint_pair_2, endpoint_pair3, this_nat_type));
  EXPECT_EQ(rudp::kBootstrapConnectionAlreadyExists,
            rudp2.GetAvailableEndpoint(node_id3, endpoint_pair3, endpoint_pair2, this_nat_type));
  EXPECT_EQ(kSuccess, network.Add(node_id2, endpoint_pair2, "validation_3->2"));
  EXPECT_EQ(kSuccess, rudp2.Add(node_id3, endpoint_pair3, "validation_2->3"));

  if (connection_completion_future.wait_for(std::chrono::seconds(10)) !=
      std::future_status::ready) {
    ASSERT_TRUE(false) << "Failed waiting for node-3 to receive validation data";
  }

  LOG(kVerbose) << " ------------------------ 3rd node setup done ------------------------------ ";

  // setup 7 inactive & 1 active node
  std::vector<NodeInfoAndPrivateKey> nodes;
  for (auto i(0); i != 8; ++i)
    nodes.push_back(MakeNodeInfoAndKeys());
  SortFromThisNode(node_id3, nodes);

  // add the active node at the end of the RT
  nodes.at(7) = node2;  //  second node
  sent_message.set_destination_id(NodeId(nodes.at(0).node_info.id).string());

  for (auto i(0); i != 8; ++i)
    ASSERT_TRUE(routing_table.AddNode(nodes.at(i).node_info));

  ASSERT_EQ(8, routing_table.size());
  for (auto i(0); i != kMessageCount; ++i)
    network.SendToClosestNode(sent_message);

  if (test_completion_future.wait_for(std::chrono::seconds(60)) != std::future_status::ready) {
    ASSERT_TRUE(false) << "Failed waiting for node-2 to receive " << expected_message_at_node
                       << "messsages";
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
