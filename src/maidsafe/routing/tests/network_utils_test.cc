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

#include <boost/exception/all.hpp>
#include <chrono>
#include <future>

#include <memory>
#include <vector>

#include "boost/filesystem/exception.hpp"
#include "boost/thread/future.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/tests/test_utils.h"


namespace maidsafe {

namespace routing {

namespace test {

namespace bptime = boost::posix_time;

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

void SortFromThisNode(const NodeId& from, std::vector<NodeInfoAndPrivateKey> nodes) {
  std::sort(nodes.begin(), nodes.end(), [from](const NodeInfoAndPrivateKey& i,
                                               const NodeInfoAndPrivateKey& j) {
                return (i.node_info.node_id ^ from) < (j.node_info.node_id ^ from);
             });
}

}  // anonymous namespace

TEST(NetworkUtilsTest, BEH_ProcessSendDirectInvalidEndpoint) {
  protobuf::Message message;
  message.set_routing_message(true);
  message.set_client_node(false);
  message.add_data("data");
  message.set_request(true);
  message.set_direct(true);
  message.set_type(10);
  rudp::ManagedConnections rudp;
  asymm::Keys keys(MakeKeys());
  RoutingTable routing_table(keys, false);
  NonRoutingTable non_routing_table(keys);
  AsioService asio_service(0);
  Timer timer(asio_service);
  NetworkUtils network(routing_table, non_routing_table, timer);
  network.SendToClosestNode(message);
  network.Stop();
}

TEST(NetworkUtilsTest, BEH_ProcessSendUnavailableDirectEndpoint) {
  protobuf::Message message;
  message.set_routing_message(true);
  message.set_client_node(false);
  message.set_request(true);
  message.add_data("data");
  message.set_direct(true);
  message.set_type(10);
  rudp::ManagedConnections rudp;
  asymm::Keys keys(MakeKeys());
  RoutingTable routing_table(keys, false);
  NonRoutingTable non_routing_table(keys);
  Endpoint endpoint(GetLocalIp(),  maidsafe::test::GetRandomPort());
  AsioService asio_service(0);
  Timer timer(asio_service);
  NetworkUtils network(routing_table, non_routing_table, timer);
  network.SendToDirect(message, NodeId(NodeId::kRandomId), NodeId(NodeId::kRandomId));
  network.Stop();
}

TEST(NetworkUtilsTest, FUNC_ProcessSendDirectEndpoint) {
  const int kMessageCount(10);
  rudp::ManagedConnections rudp1, rudp2;
  Endpoint endpoint1(GetLocalIp(),  maidsafe::test::GetRandomPort());
  Endpoint endpoint2(GetLocalIp(),  maidsafe::test::GetRandomPort());

  boost::promise<bool> test_completion_promise;
  auto test_completion_future = test_completion_promise.get_future();
  bool promised(true);
  uint32_t expected_message_at_node(kMessageCount + 1);
  uint32_t message_count_at_node2(0);

  boost::promise<bool> connection_completion_promise;
  auto connection_completion_future = connection_completion_promise.get_future();

  protobuf::Message sent_message;
  sent_message.set_destination_id(NodeId(RandomString(64)).String());
  sent_message.set_routing_message(true);
  sent_message.set_request(true);
  sent_message.add_data(std::string(1024 * 256, 'A'));
  sent_message.set_direct(true);
  sent_message.set_type(10);
  sent_message.set_client_node(false);

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
          LOG(kInfo) << " -- Lost Connection with : " << HexSubstr(node_id.String());
    };

  asymm::Keys keys1(MakeKeys());
  NodeId node_id1(keys1.identity);
  std::shared_ptr<asymm::PrivateKey>
      private_key1(std::make_shared<asymm::PrivateKey>(keys1.private_key));
  std::shared_ptr<asymm::PublicKey>
      public_key1(std::make_shared<asymm::PublicKey>(keys1.public_key));
  rudp::NatType nat_type;
  auto a1 = std::async(std::launch::async, [=, &rudp1, &nat_type]()->NodeId {
      std::vector<Endpoint> bootstrap_endpoint(1, endpoint2);
      NodeId chosen_bootstrap_peer;
      if (rudp1.Bootstrap(bootstrap_endpoint,
                          message_received_functor1,
                          connection_lost_functor,
                          node_id1,
                          private_key1,
                          public_key1,
                          chosen_bootstrap_peer,
                          nat_type,
                          endpoint1) != kSuccess) {
        chosen_bootstrap_peer = NodeId();
      }
      return chosen_bootstrap_peer;
  });
  asymm::Keys keys2(MakeKeys());
  NodeId node_id2(keys2.identity);
  std::shared_ptr<asymm::PrivateKey> private_key2(new asymm::PrivateKey(keys2.private_key));
  std::shared_ptr<asymm::PublicKey> public_key2(new asymm::PublicKey(keys2.public_key));
  auto a2 = std::async(std::launch::async, [=, &rudp2, &nat_type]()->NodeId {
      std::vector<Endpoint> bootstrap_endpoint(1, endpoint1);
      NodeId chosen_bootstrap_peer;
      if (rudp2.Bootstrap(bootstrap_endpoint,
                          message_received_functor2,
                          connection_lost_functor,
                          node_id2,
                          private_key2,
                          public_key2,
                          chosen_bootstrap_peer,
                          nat_type,
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
//  EXPECT_EQ(kSuccess, rudp1.Add(node_id2, endpoint_pair_2, "validation_1->2"));
//  EXPECT_EQ(kSuccess, rudp2.Add(node_id1, endpoint_pair_1, "validation_2->1"));
  Endpoint endpoint;
  rudp1.MarkConnectionAsValid(node_id2, endpoint);
  rudp2.MarkConnectionAsValid(node_id1, endpoint);
  LOG(kVerbose) << " ------------------------   Zero state setup done  ----------------------- ";

  asymm::Keys keys(MakeKeys());
  NodeId node_id3(keys.identity);
  RoutingTable routing_table(keys, false);
  NonRoutingTable non_routing_table(keys);
  AsioService asio_service(0);
  Timer timer(asio_service);
  NetworkUtils network(routing_table, non_routing_table, timer);

  std::vector<Endpoint> bootstrap_endpoint(1, endpoint2);
  EXPECT_EQ(kSuccess, network.Bootstrap(bootstrap_endpoint,
                                        message_received_functor3,
                                        connection_lost_functor));
//   rudp::EndpointPair endpoint_pair2, endpoint_pair3;
  rudp::NatType this_nat_type;
  network.GetAvailableEndpoint(node_id2, endpoint_pair_2, endpoint_pair_3, this_nat_type);
  EXPECT_EQ(kSuccess,
            rudp2.GetAvailableEndpoint(node_id3, endpoint_pair_3, endpoint_pair_2, this_nat_type));
  EXPECT_EQ(kSuccess, network.Add(node_id2, endpoint_pair_2, "validation_3->2"));

  EXPECT_EQ(kSuccess, rudp2.Add(node_id3, endpoint_pair_3, "validation_2->3"));
  if (!connection_completion_future.timed_wait(bptime::seconds(10))) {
    ASSERT_TRUE(false) << "Failed waiting for node-3 to receive validation data";
  }

  for (auto i(0); i != kMessageCount; ++i) {
    network.SendToDirect(sent_message, node_id2, node_id2);
    Sleep(boost::posix_time::milliseconds(100));
  }
  if (!test_completion_future.timed_wait(bptime::seconds(60))) {
    ASSERT_TRUE(false) << "Failed waiting for node-2 to receive "
                       << expected_message_at_node << "messsages";
  }
  network.Stop();
}

// RT with only 1 active node and 7 inactive node
TEST(NetworkUtilsTest, FUNC_ProcessSendRecursiveSendOn) {
  const int kMessageCount(1);
  rudp::ManagedConnections rudp1, rudp2;
  Endpoint endpoint1(GetLocalIp(),  maidsafe::test::GetRandomPort());
  Endpoint endpoint2(GetLocalIp(),  maidsafe::test::GetRandomPort());

  boost::promise<bool> test_completion_promise;
  auto test_completion_future = test_completion_promise.get_future();
  bool promised(true);
  uint32_t expected_message_at_node(kMessageCount + 1);
  uint32_t message_count_at_node2(0);

  boost::promise<bool> connection_completion_promise;
  auto connection_completion_future = connection_completion_promise.get_future();

  protobuf::Message sent_message;
//  sent_message.add_data(std::string(1024 * 256, 'B'));
  sent_message.add_data(std::string(10, 'B'));
  sent_message.set_direct(true);
  sent_message.set_type(10);
  sent_message.set_routing_message(true);
  sent_message.set_request(true);
  sent_message.set_client_node(false);
  asymm::Keys keys(MakeKeys());
  NodeId node_id3(keys.identity);
  RoutingTable routing_table(keys, false);
  NonRoutingTable non_routing_table(keys);
  AsioService asio_service(0);
  Timer timer(asio_service);
  NetworkUtils network(routing_table, non_routing_table, timer);

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
      LOG(kInfo) << " -- Lost Connection with : " << HexSubstr(node_id.String());
    };

  rudp::ConnectionLostFunctor connection_lost_functor3 = [&](const NodeId& node_id) {
      routing_table.DropNode(node_id, true);
      LOG(kInfo) << " -- Lost Connection with : " << HexSubstr(node_id.String());
    };

  asymm::Keys keys1(MakeKeys());
  NodeId node_id1(keys1.identity);
  std::shared_ptr<asymm::PrivateKey> private_key1(new asymm::PrivateKey(keys1.private_key));
  std::shared_ptr<asymm::PublicKey> public_key1(new asymm::PublicKey(keys1.public_key));
  rudp::NatType nat_type;
  auto a1 = std::async(std::launch::async, [=, &rudp1, &nat_type]()->NodeId {
      std::vector<Endpoint> bootstrap_endpoint(1, endpoint2);
      NodeId chosen_bootstrap_peer;
      if (rudp1.Bootstrap(bootstrap_endpoint,
                          message_received_functor1,
                          connection_lost_functor,
                          node_id1,
                          private_key1,
                          public_key1,
                          chosen_bootstrap_peer,
                          nat_type,
                          endpoint1) != kSuccess) {
        chosen_bootstrap_peer = NodeId();
      }
      return chosen_bootstrap_peer;
  });
  NodeInfoAndPrivateKey node2 = MakeNodeInfoAndKeys();
  asymm::Keys keys2(GetKeys(node2));
  NodeId node_id2(keys2.identity);
  std::shared_ptr<asymm::PrivateKey> private_key2(new asymm::PrivateKey(keys2.private_key));
  std::shared_ptr<asymm::PublicKey> public_key2(new asymm::PublicKey(keys2.public_key));
  auto a2 = std::async(std::launch::async, [=, &rudp2, &nat_type]()->NodeId {
      std::vector<Endpoint> bootstrap_endpoint(1, endpoint1);
      NodeId chosen_bootstrap_peer;
      if (rudp2.Bootstrap(bootstrap_endpoint,
                          message_received_functor2,
                          connection_lost_functor,
                          node_id2,
                          private_key2,
                          public_key2,
                          chosen_bootstrap_peer,
                          nat_type,
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
//  EXPECT_EQ(kSuccess, rudp1.Add(node_id2, endpoint_pair_2, "validation_1->2"));
//  EXPECT_EQ(kSuccess, rudp2.Add(node_id1, endpoint_pair_1, "validation_2->1"));
  Endpoint endpoint;
  rudp1.MarkConnectionAsValid(node_id2, endpoint);
  rudp2.MarkConnectionAsValid(node_id1, endpoint);
  LOG(kVerbose) << " ------------------------   Zero state setup done  ----------------------- ";



  std::vector<Endpoint> bootstrap_endpoint(1, endpoint2);
  EXPECT_EQ(kSuccess, network.Bootstrap(bootstrap_endpoint,
                                        message_received_functor3,
                                        connection_lost_functor3));
  rudp::EndpointPair endpoint_pair2, endpoint_pair3;
  rudp::NatType this_nat_type;
  EXPECT_EQ(kSuccess, network.GetAvailableEndpoint(node_id2, endpoint_pair_2,
                                                   endpoint_pair3, this_nat_type));
  EXPECT_EQ(kSuccess,
            rudp2.GetAvailableEndpoint(node_id3, endpoint_pair3, endpoint_pair2, this_nat_type));
  EXPECT_EQ(kSuccess, network.Add(node_id2, endpoint_pair2, "validation_3->2"));
  EXPECT_EQ(kSuccess, rudp2.Add(node_id3, endpoint_pair3, "validation_2->3"));

  if (!connection_completion_future.timed_wait(bptime::seconds(10))) {
    ASSERT_TRUE(false) << "Failed waiting for node-3 to receive validation data";
  }

  LOG(kVerbose) << " ------------------------ 3rd node setup done ------------------------------ ";

  // setup 7 inactive & 1 active node
  std::vector<NodeInfoAndPrivateKey> nodes;
  for (auto i(0); i != 8; ++i)
    nodes.push_back(MakeNodeInfoAndKeys());
  SortFromThisNode(NodeId(keys.identity), nodes);

  // add the active node at the end of the RT
  nodes.at(7) = node2;  //  second node
  sent_message.set_destination_id(NodeId(nodes.at(0).node_info.node_id).String());

  for (auto i(0); i != 8; ++i)
    ASSERT_TRUE(routing_table.AddNode(nodes.at(i).node_info));

  ASSERT_EQ(8, routing_table.Size());
  for (auto i(0); i != kMessageCount; ++i)
    network.SendToClosestNode(sent_message);

  if (!test_completion_future.timed_wait(bptime::seconds(60))) {
    ASSERT_TRUE(false) << "Failed waiting for node-2 to receive "
                       << expected_message_at_node << "messsages";
  }

  network.Stop();
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
