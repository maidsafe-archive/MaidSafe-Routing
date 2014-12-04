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

#include <algorithm>
#include <memory>
#include <vector>

#include "maidsafe/common/log.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/passport/passport.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/client_routing_table.h"
#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/response_handler.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/service.h"
#include "maidsafe/routing/utils.h"
#include "maidsafe/routing/tests/mock_network.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

class ResponseHandlerTest : public testing::Test {
 public:
  ResponseHandlerTest()
      : node_id_(RandomString(NodeId::kSize)),
        asio_service_(2),
        network_utils_(node_id_, asio_service_),
        routing_table_(false, NodeId(RandomString(NodeId::kSize)), asymm::GenerateKeyPair()),
        client_routing_table_(routing_table_.kNodeId()),
        network_(routing_table_, client_routing_table_, network_utils_.acknowledgement_),
        public_key_holder_(asio_service_, network_),
        response_handler_(new ResponseHandler(routing_table_, client_routing_table_, network_,
                                              public_key_holder_)) {}

  int GetAvailableEndpoint(rudp::EndpointPair& this_endpoint_pair, rudp::NatType& this_nat_type,
                           int return_val) {
    this_endpoint_pair.local = boost::asio::ip::udp::endpoint(
        boost::asio::ip::address_v4::loopback(), maidsafe::test::GetRandomPort());
    this_endpoint_pair.external = boost::asio::ip::udp::endpoint(
        boost::asio::ip::address_v4::loopback(), maidsafe::test::GetRandomPort());
    this_nat_type = rudp::NatType::kUnknown;
    return return_val;
  }

  void RequestPublicKey(NodeId /*node_id*/, GivePublicKeyFunctor give_public_key) {
    give_public_key(passport::CreatePmidAndSigner().first.public_key());
  }

 protected:
  void SetUp() override {}

  void TearDown() override {}

  void SetProtobufContact(protobuf::Contact* contact, const NodeId& respondent_contact_node_id,
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

  protobuf::FindNodesResponse ComposeFindNodesResponse(const std::string& ori_find_nodes_request,
                                                       size_t num_of_requested,
                                                       std::vector<NodeId> nodes =
                                                           std::vector<NodeId>()) {
    if (nodes.empty())
      for (size_t i(0); i < num_of_requested; ++i)
        nodes.push_back(NodeId(RandomString(64)));

    protobuf::FindNodesResponse found_nodes;
    for (const auto& node : nodes)
      found_nodes.add_nodes(node.string());
    found_nodes.set_original_request(ori_find_nodes_request);
    found_nodes.set_original_signature(routing_table_.kNodeId().string());
    found_nodes.set_timestamp(GetTimeStamp());

    return found_nodes;
  }

  protobuf::ConnectResponse ComposeConnectResponse(protobuf::ConnectResponseType response_type,
                                                   const std::string& ori_connect_request,
                                                   const NodeId& respondent_contact_node_id,
                                                   bool respondent_contact_peer_endpoint) {
    protobuf::ConnectResponse connect_response;
    SetProtobufContact(connect_response.mutable_contact(), respondent_contact_node_id,
                       respondent_contact_peer_endpoint);
    connect_response.set_answer(response_type);
    connect_response.set_original_request(ori_connect_request);
    connect_response.set_original_signature(routing_table_.kNodeId().string());
    connect_response.set_timestamp(GetTimeStamp());
    return connect_response;
  }

  protobuf::ConnectSuccessAcknowledgement ComposeConnectSuccessAcknowledgement(
      const NodeId& node_id, const NodeId& connection_id, bool requestor = true,
      const std::vector<std::string>& close_ids = std::vector<std::string>()) {
    protobuf::ConnectSuccessAcknowledgement connect_ack;
    connect_ack.set_node_id(node_id.string());
    connect_ack.set_connection_id(connection_id.string());
    for (const auto& close_id : close_ids)
      connect_ack.add_close_ids(close_id);
    connect_ack.set_requestor(requestor);
    return connect_ack;
  }

  protobuf::PingResponse ComposePingResponse(const std::string& ori_ping_request) {
    protobuf::PingResponse ping_response;
    ping_response.set_pong(true);
    ping_response.set_timestamp(GetTimeStamp());
    ping_response.set_original_request(ori_ping_request);
    ping_response.set_original_signature(RandomString(128));
    return ping_response;
  }

  protobuf::Message ComposeMsg(const std::string& data) {
    protobuf::Message message;
    //     message.set_destination_id(message.source_id());
    message.set_source_id(routing_table_.kNodeId().string());
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

  protobuf::Message ComposeFindNodesResponseMsg(size_t num_of_requested,
                                                std::vector<NodeId> nodes = std::vector<NodeId>()) {
    protobuf::FindNodesRequest find_nodes;
    find_nodes.set_num_nodes_requested(static_cast<int32_t>(num_of_requested));
    find_nodes.set_timestamp(GetTimeStamp());
    return ComposeMsg(ComposeFindNodesResponse(find_nodes.SerializeAsString(), num_of_requested,
                                               nodes).SerializeAsString());
  }

  protobuf::Message ComposeConnectResponseMsg(protobuf::ConnectResponseType response_type,
                                              const NodeId& respondent_contact_node_id =
                                                  NodeId(RandomString(64)),
                                              bool respondent_contact_peer_endpoint = true) {
    protobuf::ConnectRequest connect;
    SetProtobufContact(connect.mutable_contact(), routing_table_.kNodeId(),
                       respondent_contact_peer_endpoint);
    connect.set_peer_id(routing_table_.kNodeId().string());
    connect.set_bootstrap(false);
    connect.set_timestamp(GetTimeStamp());
    return ComposeMsg(ComposeConnectResponse(response_type, connect.SerializeAsString(),
                                             respondent_contact_node_id,
                                             respondent_contact_peer_endpoint).SerializeAsString());
  }

  protobuf::Message ComposePingResponseMsg() {
    protobuf::PingRequest ping_request;
    ping_request.set_ping(true);
    ping_request.set_timestamp(GetTimeStamp());
    return ComposeMsg(ComposePingResponse(ping_request.SerializeAsString()).SerializeAsString());
  }

  NodeId node_id_;
  AsioService asio_service_;
  NetworkUtils network_utils_;
  RoutingTable routing_table_;
  ClientRoutingTable client_routing_table_;
  MockNetwork network_;
  PublicKeyHolder public_key_holder_;
  std::shared_ptr<ResponseHandler> response_handler_;
};

TEST_F(ResponseHandlerTest, DISABLED_BEH_FindNodes) {
  protobuf::Message message;
  // Incorrect FindNodeResponse msg
  message = ComposeMsg(RandomString(128));
  response_handler_->FindNodes(message);

  // Incorrect Original FindNodesRequest part
  message = ComposeMsg(ComposeFindNodesResponse(RandomString(128), 4).SerializeAsString());
  response_handler_->FindNodes(message);

  // In case of collision
  std::vector<NodeId> nodes;
  nodes.push_back(routing_table_.kNodeId());
  message = ComposeFindNodesResponseMsg(1, nodes);
  response_handler_->FindNodes(message);

  // In case of need to re-bootstrap
  message = ComposeFindNodesResponseMsg(4);
  response_handler_->FindNodes(message);

  NodeInfo node_info = MakeNodeInfoAndKeys().node_info;
  routing_table_.AddNode(node_info);

  // In case of trying to connect to self
  nodes.push_back(NodeId(RandomString(64)));
  message = ComposeFindNodesResponseMsg(2, nodes);
  EXPECT_CALL(network_, GetAvailableEndpoint(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(testing::WithArgs<2, 3>(testing::Invoke(
           boost::bind(&ResponseHandlerTest::GetAvailableEndpoint, this, _1, _2, kSuccess))));
  EXPECT_CALL(network_, SendToClosestNode(testing::_)).Times(1);
  response_handler_->FindNodes(message);

  // Properly found 4 nodes and trying to connect
  message = ComposeFindNodesResponseMsg(4);
  EXPECT_CALL(network_, GetAvailableEndpoint(testing::_, testing::_, testing::_, testing::_))
      .Times(4)
      .WillRepeatedly(testing::WithArgs<2, 3>(testing::Invoke(
           boost::bind(&ResponseHandlerTest::GetAvailableEndpoint, this, _1, _2, kSuccess))));
  EXPECT_CALL(network_, SendToClosestNode(testing::_)).Times(4);
  response_handler_->FindNodes(message);

  // In case routing_table_ is full
  while (routing_table_.size() < size_t(Parameters::max_routing_table_size)) {
    NodeInfo node_info = MakeNodeInfoAndKeys().node_info;
    routing_table_.AddNode(node_info);
  }
  size_t num_of_found_nodes(2);
  nodes.clear();
  for (size_t i(0); i < num_of_found_nodes; ++i) {
    NodeId node_id(RandomString(64));
    while (
        NodeId::CloserToTarget(routing_table_.GetNthClosestNode(routing_table_.kNodeId(),
                                                                Parameters::closest_nodes_size).id,
                               node_id, routing_table_.kNodeId()))
      node_id = NodeId(RandomString(64));
    nodes.push_back(node_id);
  }
  for (size_t i(0); i < num_of_found_nodes; ++i) {
    NodeId node_id(RandomString(64));
    while (NodeId::CloserToTarget(
        node_id, routing_table_.GetNthClosestNode(routing_table_.kNodeId(),
                                                  Parameters::closest_nodes_size).id,
        routing_table_.kNodeId()))
      node_id = NodeId(RandomString(64));
    nodes.push_back(node_id);
  }
  ASSERT_EQ(nodes.size(), num_of_found_nodes * 2);
  message = ComposeFindNodesResponseMsg(num_of_found_nodes, nodes);
  EXPECT_CALL(network_, GetAvailableEndpoint(testing::_, testing::_, testing::_, testing::_))
      .Times(static_cast<int>(num_of_found_nodes))
      .WillRepeatedly(testing::WithArgs<2, 3>(testing::Invoke(
           boost::bind(&ResponseHandlerTest::GetAvailableEndpoint, this, _1, _2, kSuccess))));
  EXPECT_CALL(network_, SendToClosestNode(testing::_)).Times(static_cast<int>(num_of_found_nodes));
  response_handler_->FindNodes(message);
}

TEST_F(ResponseHandlerTest, DISABLED_BEH_Connect) {
  protobuf::Message message;
  // Incorrect ConnectResponse msg
  message = ComposeMsg(RandomString(128));
  response_handler_->Connect(message);

  // Incorrect Original ConnectRequest part
  message =
      ComposeMsg(ComposeConnectResponse(protobuf::ConnectResponseType::kAccepted, RandomString(128),
                                        NodeId(RandomString(64)), true).SerializeAsString());
  response_handler_->Connect(message);

  // In case of rejected
  message = ComposeConnectResponseMsg(protobuf::ConnectResponseType::kRejected);
  response_handler_->Connect(message);

  // In case of Already ongoing connection attempt
  message = ComposeConnectResponseMsg(protobuf::ConnectResponseType::kConnectAttemptAlreadyRunning);
  response_handler_->Connect(message);

  // In case of node already added
  message =
      ComposeConnectResponseMsg(protobuf::ConnectResponseType::kAccepted, routing_table_.kNodeId());
  response_handler_->Connect(message);

  // Invalid contact node_id details
  message = ComposeConnectResponseMsg(protobuf::ConnectResponseType::kAccepted, NodeId());
  response_handler_->Connect(message);

  // Invalid contact peer endpoint details
  message = ComposeConnectResponseMsg(protobuf::ConnectResponseType::kAccepted,
                                      NodeId(RandomString(64)), false);
  response_handler_->Connect(message);

  // Failed add to RUDP
  message = ComposeConnectResponseMsg(protobuf::ConnectResponseType::kAccepted);
  EXPECT_CALL(network_, Add(testing::_, testing::_, testing::_)).WillOnce(testing::Return(-350023));
  response_handler_->Connect(message);

  // Succeed add to RUDP
  message = ComposeConnectResponseMsg(protobuf::ConnectResponseType::kAccepted);
  EXPECT_CALL(network_, Add(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(kSuccess));
  response_handler_->Connect(message);

  // Special case with bootstrapping peer in which kSuccess comes before connect response
  NodeId node_id(RandomString(64));
  network_.SetBootstrapConnectionId(node_id);
  message = ComposeConnectResponseMsg(protobuf::ConnectResponseType::kAccepted, node_id);
  EXPECT_CALL(network_, Add(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(kSuccess));
  EXPECT_CALL(network_, SendToDirect(testing::_, testing::_, testing::_)).Times(1);
  response_handler_->Connect(message);
}

TEST_F(ResponseHandlerTest, DISABLED_BEH_ConnectSuccessAcknowledgement) {
  protobuf::Message message;
  NodeId node_id(RandomString(64)), connection_id(RandomString(64));
  // Incorrect ConnectSuccessAcknowledgement msg
  message = ComposeMsg(RandomString(128));
  response_handler_->ConnectSuccessAcknowledgement(message);

  // Invalid node_id
  message =
      ComposeMsg(ComposeConnectSuccessAcknowledgement(NodeId(), connection_id).SerializeAsString());
  response_handler_->ConnectSuccessAcknowledgement(message);

  // Invalid peer connection_id
  message = ComposeMsg(ComposeConnectSuccessAcknowledgement(node_id, NodeId()).SerializeAsString());
  response_handler_->ConnectSuccessAcknowledgement(message);

  // shared_from_this function inside requires the response_handler holder to be shared_ptr
  // if holding as a normal object, shared_from_this will throw an exception
  std::shared_ptr<ResponseHandler> response_handler(
      std::make_shared<ResponseHandler>(routing_table_, client_routing_table_, network_,
                                        public_key_holder_));

  // request_public_key_functor_ doesn't setup
  message =
      ComposeMsg(ComposeConnectSuccessAcknowledgement(node_id, connection_id).SerializeAsString());
  response_handler->ConnectSuccessAcknowledgement(message);

  // Setup request_public_key_functor_ testing accessor/modifier
  EXPECT_EQ(nullptr, response_handler->request_public_key_functor());
  response_handler->set_request_public_key_functor(
      boost::bind(&ResponseHandlerTest::RequestPublicKey, this, _1, _2));
  EXPECT_NE(nullptr, response_handler->request_public_key_functor());

  // Rudp failed to validate connection
  EXPECT_CALL(network_, MarkConnectionAsValid(testing::_)).WillOnce(testing::Return(-350020));
  response_handler->ConnectSuccessAcknowledgement(message);

  // Rudp succeed to validate connection, HandleSuccessAcknowledgementAsReponder
  EXPECT_CALL(network_, MarkConnectionAsValid(testing::_)).WillOnce(testing::Return(kSuccess));
  EXPECT_CALL(network_, SendToDirect(testing::_, testing::_, testing::_)).Times(1);
  response_handler->ConnectSuccessAcknowledgement(message);

  // Rudp succeed to validate connection, HandleSuccessAcknowledgementAsRequestor
  std::vector<std::string> close_ids;
  size_t num_close_ids(4);
  for (size_t i(0); i < num_close_ids; ++i)
    close_ids.push_back(RandomString(64));
  message = ComposeMsg(ComposeConnectSuccessAcknowledgement(NodeId(RandomString(64)), connection_id,
                                                            false, close_ids).SerializeAsString());
  EXPECT_CALL(network_, MarkConnectionAsValid(testing::_)).WillOnce(testing::Return(kSuccess));
  EXPECT_CALL(network_, GetAvailableEndpoint(testing::_, testing::_, testing::_, testing::_))
      .Times(static_cast<int>(num_close_ids))
      .WillRepeatedly(testing::WithArgs<2, 3>(testing::Invoke(
           boost::bind(&ResponseHandlerTest::GetAvailableEndpoint, this, _1, _2, kSuccess))));
  EXPECT_CALL(network_, SendToClosestNode(testing::_)).Times(static_cast<int>(num_close_ids));
  response_handler->ConnectSuccessAcknowledgement(message);

  // Rudp succeed to validate connection, HandleSuccessAcknowledgementAsRequestor
  // rudp::kUnvalidatedConnectionAlreadyExists
  close_ids.clear();
  num_close_ids = 1;
  for (size_t i(0); i < num_close_ids; ++i)
    close_ids.push_back(RandomString(64));
  message = ComposeMsg(ComposeConnectSuccessAcknowledgement(NodeId(RandomString(64)), connection_id,
                                                            false, close_ids).SerializeAsString());
  EXPECT_CALL(network_, MarkConnectionAsValid(testing::_)).WillOnce(testing::Return(kSuccess));
  EXPECT_CALL(network_, GetAvailableEndpoint(testing::_, testing::_, testing::_, testing::_))
      .Times(static_cast<int>(num_close_ids))
      .WillRepeatedly(testing::WithArgs<2, 3>(
           testing::Invoke(boost::bind(&ResponseHandlerTest::GetAvailableEndpoint, this, _1, _2,
                                       rudp::kUnvalidatedConnectionAlreadyExists))));
  response_handler->ConnectSuccessAcknowledgement(message);

  // Rudp succeed to validate connection, HandleSuccessAcknowledgementAsRequestor
  // rudp::kInvalidAddress
  close_ids.clear();
  num_close_ids = 1;
  for (size_t i(0); i < num_close_ids; ++i)
    close_ids.push_back(RandomString(64));
  message = ComposeMsg(ComposeConnectSuccessAcknowledgement(NodeId(RandomString(64)), connection_id,
                                                            false, close_ids).SerializeAsString());
  EXPECT_CALL(network_, MarkConnectionAsValid(testing::_)).WillOnce(testing::Return(kSuccess));
  EXPECT_CALL(network_, GetAvailableEndpoint(testing::_, testing::_, testing::_, testing::_))
      .Times(static_cast<int>(num_close_ids))
      .WillRepeatedly(testing::WithArgs<2, 3>(testing::Invoke(boost::bind(
           &ResponseHandlerTest::GetAvailableEndpoint, this, _1, _2, rudp::kInvalidAddress))));
  response_handler->ConnectSuccessAcknowledgement(message);

  // Not in any peer's routing table, need a path back through relay IP.
  network_.SetBootstrapConnectionId(NodeId(RandomString(64)));
  close_ids.clear();
  num_close_ids = 1;
  for (size_t i(0); i < num_close_ids; ++i)
    close_ids.push_back(RandomString(64));
  message = ComposeMsg(ComposeConnectSuccessAcknowledgement(NodeId(RandomString(64)), connection_id,
                                                            false, close_ids).SerializeAsString());
  EXPECT_CALL(network_, MarkConnectionAsValid(testing::_)).WillOnce(testing::Return(kSuccess));
  EXPECT_CALL(network_, GetAvailableEndpoint(testing::_, testing::_, testing::_, testing::_))
      .Times(static_cast<int>(num_close_ids))
      .WillRepeatedly(testing::WithArgs<2, 3>(testing::Invoke(
           boost::bind(&ResponseHandlerTest::GetAvailableEndpoint, this, _1, _2, rudp::kSuccess))));
  EXPECT_CALL(network_, SendToDirect(testing::_, testing::_, testing::_))
      .Times(static_cast<int>(num_close_ids));
  response_handler->ConnectSuccessAcknowledgement(message);
}

TEST_F(ResponseHandlerTest, BEH_Ping) {
  protobuf::Message message;
  // Incorrect Ping msg
  message = ComposeMsg(RandomString(128));
  response_handler_->Ping(message);

  // Correct Ping msg
  message = ComposePingResponseMsg();
  response_handler_->Ping(message);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
