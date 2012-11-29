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

#include <algorithm>
#include <memory>
#include <vector>

#include "maidsafe/common/log.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/response_handler.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/service.h"
#include "maidsafe/routing/utils.h"
#include "maidsafe/routing/tests/mock_network_utils.h"
#include "maidsafe/routing/tests/test_utils.h"


namespace maidsafe {

namespace routing {

namespace test {

class ResponseHandlerTest : public testing::Test {
 public:
  ResponseHandlerTest()
     : fob_(MakeFob()),
       routing_table_(fob_, false),
       non_routing_table_(fob_),
       network_(routing_table_, non_routing_table_),
       response_handler_(routing_table_, non_routing_table_, network_) {}

  int GetAvailableEndpoint(rudp::EndpointPair& this_endpoint_pair,
                           rudp::NatType& this_nat_type) {
    this_endpoint_pair.local = boost::asio::ip::udp::endpoint(
                                  boost::asio::ip::address_v4::loopback(),
                                  maidsafe::test::GetRandomPort());
    this_endpoint_pair.external = boost::asio::ip::udp::endpoint(
                                      boost::asio::ip::address_v4::loopback(),
                                      maidsafe::test::GetRandomPort());
    this_nat_type = rudp::NatType::kUnknown;
    return kSuccess;
  }

 protected:
  void SetUp() {}

  void TearDown() {}

  void SetProtobufContact(protobuf::Contact* contact,
                          const NodeId &respondent_contact_node_id,
                          bool respondent_contact_peer_endpoint) {
    if (respondent_contact_peer_endpoint) {
      SetProtobufEndpoint(boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::loopback(),
                                                        maidsafe::test::GetRandomPort()),
                          contact->mutable_public_endpoint());
      SetProtobufEndpoint(boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::loopback(),
                                                        maidsafe::test::GetRandomPort()),
                          contact->mutable_private_endpoint());
    } else {
      SetProtobufEndpoint(boost::asio::ip::udp::endpoint(), contact->mutable_public_endpoint());
      SetProtobufEndpoint(boost::asio::ip::udp::endpoint(), contact->mutable_private_endpoint());
    }
    contact->set_node_id(respondent_contact_node_id.string());
    contact->set_connection_id(RandomString(64));
    contact->set_nat_type(NatTypeProtobuf(rudp::NatType::kUnknown));
  }

  protobuf::FindNodesResponse ComposeFindNodesResponse(
      const std::string &ori_find_nodes_request,
      size_t num_of_requested,
      std::vector<NodeId> nodes = std::vector<NodeId>()) {
    if (nodes.empty())
      for (size_t i(0); i < num_of_requested; ++i)
        nodes.push_back(NodeId(RandomString(64)));

    protobuf::FindNodesResponse found_nodes;
    for (auto node : nodes)
      found_nodes.add_nodes(node.string());
    found_nodes.set_original_request(ori_find_nodes_request);
    found_nodes.set_original_signature(routing_table_.kFob().identity.string());
    found_nodes.set_timestamp(GetTimeStamp());

    return found_nodes;
  }

  protobuf::ConnectResponse ComposeConnectResponse(protobuf::ConnectResponseType response_type,
                                                   const std::string &ori_connect_request,
                                                   const NodeId &respondent_contact_node_id,
                                                   bool respondent_contact_peer_endpoint) {
    protobuf::ConnectResponse connect_response;
    SetProtobufContact(connect_response.mutable_contact(),
                       respondent_contact_node_id,
                       respondent_contact_peer_endpoint);
    connect_response.set_answer(response_type);
    connect_response.set_original_request(ori_connect_request);
    connect_response.set_original_signature(routing_table_.kFob().identity.string());
    connect_response.set_timestamp(GetTimeStamp());
    return connect_response;
  }

  protobuf::Message ComposeMsg(const std::string &data) {
    protobuf::Message message;
//     message.set_destination_id(message.source_id());
    message.set_source_id(routing_table_.kFob().identity.string());
    message.clear_route_history();
    message.clear_data();
    message.add_data(data);
    message.set_direct(true);
    message.set_replication(1);
    message.set_client_node(routing_table_.client_mode());
    message.set_request(false);
    message.set_hops_to_live(Parameters::hops_to_live);
    return message;
  }

  protobuf::Message ComposeFindNodesResponseMsg(
      size_t num_of_requested,
      std::vector<NodeId> nodes = std::vector<NodeId>()) {
    protobuf::FindNodesRequest find_nodes;
    find_nodes.set_num_nodes_requested(num_of_requested);
    find_nodes.set_target_node(routing_table_.kFob().identity.string());
    find_nodes.set_timestamp(GetTimeStamp());
    return ComposeMsg(ComposeFindNodesResponse(find_nodes.SerializeAsString(),
                                               num_of_requested,
                                               nodes).SerializeAsString());
  }

  protobuf::Message ComposeConnectResponseMsg(protobuf::ConnectResponseType response_type,
                        const NodeId &respondent_contact_node_id = NodeId(RandomString(64)),
                        bool respondent_contact_peer_endpoint = true) {
    protobuf::ConnectRequest connect;
    SetProtobufContact(connect.mutable_contact(),
                       NodeId(routing_table_.kFob().identity),
                       respondent_contact_peer_endpoint);
    connect.set_peer_id(routing_table_.kFob().identity.string());
    connect.set_bootstrap(false);
    connect.set_timestamp(GetTimeStamp());
    return ComposeMsg(ComposeConnectResponse(response_type, connect.SerializeAsString(),
        respondent_contact_node_id, respondent_contact_peer_endpoint).SerializeAsString());
  }

  Fob fob_;
  RoutingTable routing_table_;
  NonRoutingTable non_routing_table_;
  MockNetworkUtils network_;
  ResponseHandler response_handler_;
};

TEST_F(ResponseHandlerTest, BEH_FindNodes) {
  protobuf::Message message;
  // Incorrect FindNodeResponse msg
  message = ComposeFindNodesResponseMsg(4);
  message.clear_data();
  message.add_data(RandomString(128));
  response_handler_.FindNodes(message);

  // Incorrect Original FindNodesRequest part
  message = ComposeFindNodesResponseMsg(4);
  message.clear_data();
  message.add_data(ComposeFindNodesResponse(RandomString(128), 4).SerializeAsString());
  response_handler_.FindNodes(message);

  // In case of collision
  std::vector<NodeId> nodes;
  nodes.push_back(NodeId(routing_table_.kFob().identity));
  message = ComposeFindNodesResponseMsg(1, nodes);
  response_handler_.FindNodes(message);

  // In case of need to re-bootstrap
  message = ComposeFindNodesResponseMsg(4);
  response_handler_.FindNodes(message);

  NodeInfo node_info = MakeNodeInfoAndKeys().node_info;
  routing_table_.AddNode(node_info);

  // In case of trying to connect to self
  nodes.push_back(NodeId(RandomString(64)));
  message = ComposeFindNodesResponseMsg(2, nodes);
  EXPECT_CALL(network_, GetAvailableEndpoint(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(testing::WithArgs<2, 3>(testing::Invoke(
            boost::bind(&ResponseHandlerTest::GetAvailableEndpoint, this, _1, _2))));
  EXPECT_CALL(network_, SendToClosestNode(testing::_)).Times(1);
  response_handler_.FindNodes(message);

  // Properly found 4 nodes and trying to connect
  message = ComposeFindNodesResponseMsg(4);
  EXPECT_CALL(network_, GetAvailableEndpoint(testing::_, testing::_, testing::_, testing::_))
      .Times(4)
      .WillRepeatedly(testing::WithArgs<2, 3>(testing::Invoke(
            boost::bind(&ResponseHandlerTest::GetAvailableEndpoint, this, _1, _2))));
  EXPECT_CALL(network_, SendToClosestNode(testing::_)).Times(4);
  response_handler_.FindNodes(message);

  // In case routing_table_ is full
  while (routing_table_.size() < Parameters::greedy_fraction) {
    NodeInfo node_info = MakeNodeInfoAndKeys().node_info;
    routing_table_.AddNode(node_info);
  }
  size_t num_of_found_nodes(4), num_of_closer(0);
  nodes.clear();
  for (size_t i(0); i < num_of_found_nodes; ++i) {
    NodeId node_id(RandomString(64));
    if (num_of_closer < 2) {
      if (NodeId::CloserToTarget(node_id,
                                 routing_table_.GetNthClosestNode(routing_table_.kNodeId(),
                                                    Parameters::greedy_fraction).node_id,
                                 routing_table_.kNodeId()))
        ++num_of_closer;
    } else {
      while (NodeId::CloserToTarget(node_id,
                                    routing_table_.GetNthClosestNode(routing_table_.kNodeId(),
                                                        Parameters::greedy_fraction).node_id,
                                    routing_table_.kNodeId()))
        node_id = NodeId(RandomString(64));
    }
    nodes.push_back(node_id);
  }
  message = ComposeFindNodesResponseMsg(num_of_found_nodes, nodes);
  EXPECT_CALL(network_, GetAvailableEndpoint(testing::_, testing::_, testing::_, testing::_))
      .Times(num_of_closer)
      .WillRepeatedly(testing::WithArgs<2, 3>(testing::Invoke(
            boost::bind(&ResponseHandlerTest::GetAvailableEndpoint, this, _1, _2))));
  EXPECT_CALL(network_, SendToClosestNode(testing::_)).Times(num_of_closer);
  response_handler_.FindNodes(message);
}

TEST_F(ResponseHandlerTest, BEH_Connect) {
  protobuf::Message message;
  // Incorrect ConnectResponse msg
  message = ComposeConnectResponseMsg(protobuf::ConnectResponseType::kAccepted);
  message.clear_data();
  message.add_data(RandomString(128));
  response_handler_.Connect(message);

  // Incorrect Original ConnectRequest part
  message = ComposeConnectResponseMsg(protobuf::ConnectResponseType::kAccepted);
  message.clear_data();
  message.add_data(ComposeConnectResponse(protobuf::ConnectResponseType::kAccepted,
      RandomString(128), NodeId(RandomString(64)), true).SerializeAsString());
  response_handler_.Connect(message);

  // In case of rejected
  message = ComposeConnectResponseMsg(protobuf::ConnectResponseType::kRejected);
  response_handler_.Connect(message);

  // In case of Already ongoing connection attempt
  message = ComposeConnectResponseMsg(
      protobuf::ConnectResponseType::kConnectAttemptAlreadyRunning);
  response_handler_.Connect(message);

  // In case of node already added
  message = ComposeConnectResponseMsg(protobuf::ConnectResponseType::kAccepted,
                                      NodeId(routing_table_.kFob().identity));
  response_handler_.Connect(message);

  // Invalid contact node_id details
  message = ComposeConnectResponseMsg(protobuf::ConnectResponseType::kAccepted, NodeId());
  response_handler_.Connect(message);

  // Invalid contact peer endpoint details
  message = ComposeConnectResponseMsg(protobuf::ConnectResponseType::kAccepted,
                                      NodeId(RandomString(64)), false);
  response_handler_.Connect(message);

  // Failed add to RUDP
  message = ComposeConnectResponseMsg(protobuf::ConnectResponseType::kAccepted);
  EXPECT_CALL(network_, Add(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(-350023));
  response_handler_.Connect(message);

  // Succeed add to RUDP
  message = ComposeConnectResponseMsg(protobuf::ConnectResponseType::kAccepted);
  EXPECT_CALL(network_, Add(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(kSuccess));
  response_handler_.Connect(message);

  // Special case with bootstrapping peer in which kSuccess comes before connect response
  NodeId node_id(RandomString(64));
  network_.SetBootstrapConnectionId(node_id);
  message = ComposeConnectResponseMsg(protobuf::ConnectResponseType::kAccepted, node_id);
  EXPECT_CALL(network_, Add(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(kSuccess));
  EXPECT_CALL(network_, SendToDirect(testing::_, testing::_, testing::_)).Times(1);
  response_handler_.Connect(message);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
