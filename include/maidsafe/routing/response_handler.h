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

#ifndef MAIDSAFE_ROUTING_RESPONSE_HANDLER_H_
#define MAIDSAFE_ROUTING_RESPONSE_HANDLER_H_

#include <mutex>
#include <string>
#include <vector>
#include <utility>
#include <deque>
#include <memory>

#include "boost/asio/deadline_timer.hpp"
#include "boost/date_time/posix_time/ptime.hpp"

#include "maidsafe/common/rsa.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/timer.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/utils.h"
#include "maidsafe/routing/utils2.h"

namespace maidsafe {

namespace routing {

namespace test {
class ResponseHandlerTest_BEH_ConnectAttempts_Test;
}  // namespace test

class ClientRoutingTable;
class GroupChangeHandler;

template <typename NodeType>
class ResponseHandler : public std::enable_shared_from_this<ResponseHandler<NodeType>> {
 public:
  ResponseHandler(Connections<NodeType>& connections, NetworkUtils<NodeType>& network);
  virtual ~ResponseHandler();
  virtual void Ping(protobuf::Message& message);
  virtual void Connect(protobuf::Message& message);
  virtual void FindNodes(const protobuf::Message& message);
  template <typename PeerNodeType>
  void ConnectSuccessAcknowledgement(protobuf::Message& message);
  protobuf::Message HandleMessage(ConnectSuccessAcknowledgementRequestFromVault message_wrapper);
  protobuf::Message HandleMessage(ConnectSuccessAcknowledgementRequestFromClient message_wrapper);
  void set_request_public_key_functor(RequestPublicKeyFunctor request_public_key);
  RequestPublicKeyFunctor request_public_key_functor() const;
  void GetGroup(Timer<std::string>& timer, protobuf::Message& message);
  void CloseNodeUpdateForClient(protobuf::Message& message);
  void InformClientOfNewCloseNode(protobuf::Message& message);

  friend class test::ResponseHandlerTest_BEH_ConnectAttempts_Test;

 private:
  void SendConnectRequest(const NodeId peer_node_id);
  void CheckAndSendConnectRequest(const NodeId& node_id);
  void HandleSuccessAcknowledgementAsRequestor(const std::vector<NodeId>& close_ids);
  void HandleSuccessAcknowledgementAsReponder(NodeInfo peer, bool client);
  void ValidateAndCompleteConnection(const NodeInfo& peer, bool from_requestor,
                                     const std::vector<NodeId>& close_ids, ClientNode);
  void ValidateAndCompleteConnection(const NodeInfo& peer, bool from_requestor,
                                     const std::vector<NodeId>& close_ids, VaultNode);

  mutable std::mutex mutex_;
  Connections<NodeType>& connections_;
  NetworkUtils<NodeType>& network_;
  RequestPublicKeyFunctor request_public_key_functor_;
};

template <typename NodeType>
ResponseHandler<NodeType>::ResponseHandler(Connections<NodeType>& connections,
                                           NetworkUtils<NodeType>& network)
    : mutex_(), connections_(connections), network_(network), request_public_key_functor_() {}

template <typename NodeType>
ResponseHandler<NodeType>::~ResponseHandler() {}

template <typename NodeType>
void ResponseHandler<NodeType>::Ping(protobuf::Message& message) {
  // Always direct, never pass on

  // TODO(dirvine): do we need this and where and how can I update the response
  protobuf::PingResponse ping_response;
  if (ping_response.ParseFromString(message.data(0))) {
    // do stuff here
  }
}

template <typename NodeType>
void ResponseHandler<NodeType>::Connect(protobuf::Message& message) {
  protobuf::ConnectResponse connect_response;
  protobuf::ConnectRequest connect_request;
  if (!connect_response.ParseFromString(message.data(0))) {
    LOG(kError) << "Could not parse connect response";
    return;
  }

  if (!connect_request.ParseFromString(connect_response.original_request())) {
    LOG(kError) << "Could not parse original connect request"
                << " id: " << message.id();
    return;
  }

  if (connect_response.answer() == protobuf::ConnectResponseType::kRejected) {
    LOG(kInfo) << "Peer rejected this node's connection request."
               << " id: " << message.id();
    return;
  }

  if (connect_response.answer() == protobuf::ConnectResponseType::kConnectAttemptAlreadyRunning) {
    LOG(kInfo) << "Already ongoing connection attempt with : "
               << HexSubstr(connect_response.contact().node_id());
    return;
  }

  if (NodeId(connect_response.contact().node_id()).IsZero()) {
    LOG(kError) << "Invalid contact details";
    return;
  }

  NodeInfo node_to_add;
  node_to_add.id = NodeId(connect_response.contact().node_id());
  if (connections_.routing_table.CheckNode(node_to_add) ||
      (node_to_add.id == network_.bootstrap_connection_id())) {
    rudp::EndpointPair peer_endpoint_pair;
    peer_endpoint_pair.external =
        GetEndpointFromProtobuf(connect_response.contact().public_endpoint());
    peer_endpoint_pair.local =
        GetEndpointFromProtobuf(connect_response.contact().private_endpoint());

    if (peer_endpoint_pair.external.address().is_unspecified() &&
        peer_endpoint_pair.local.address().is_unspecified()) {
      LOG(kError) << "Invalid peer endpoint details";
      return;
    }

    NodeId peer_node_id(connect_response.contact().node_id());
    NodeId peer_connection_id(connect_response.contact().connection_id());

    LOG(kVerbose) << "This node [" << connections_.kNodeId() << "] received connect response from "
                  << peer_node_id << " id: " << message.id();

    int result =
        AddToRudp(network_, connections_.kNodeId(), connections_.kConnectionId(),
                  PeerNodeId(peer_node_id), PeerConnectionId(peer_connection_id),
                  PeerEndpoint(peer_endpoint_pair), IsRequestor(true));
    if (result == kSuccess) {
      // Special case with bootstrapping peer in which kSuccess comes before connect response
      if (peer_node_id == network_.bootstrap_connection_id()) {
        LOG(kInfo) << "Special case with bootstrapping peer : " << DebugId(peer_node_id);
        const std::vector<NodeInfo> close_nodes;  // add closer ids if needed
        protobuf::Message connect_success_ack(rpcs::ConnectSuccessAcknowledgement(
            PeerNodeId(peer_node_id), connections_.kNodeId(), connections_.kConnectionId(),
            IsRequestor(true), close_nodes, IsClient(NodeType::value)));
        network_.SendToDirect(connect_success_ack, peer_node_id, peer_connection_id);
      }
    }
  } else {
    LOG(kVerbose) << "Already added node";
  }
}

template <typename NodeType>
void ResponseHandler<NodeType>::FindNodes(const protobuf::Message& message) {
  protobuf::FindNodesResponse find_nodes_response;
  protobuf::FindNodesRequest find_nodes_request;
  if (!find_nodes_response.ParseFromString(message.data(0))) {
    LOG(kError) << "Could not parse find node response";
    return;
  }
  if (!find_nodes_request.ParseFromString(find_nodes_response.original_request())) {
    LOG(kError) << "Could not parse original find node request";
    return;
  }

  if (find_nodes_request.num_nodes_requested() == 1) {  // detect collision
    if ((find_nodes_response.nodes_size() == 1) &&
        find_nodes_response.nodes(0) == connections_.kNodeId().data.string()) {
      LOG(kWarning) << "Collision detected";
      // TODO(Prakash): FIXME handle collision and return kIdCollision on join()
      return;
    }
  }
  //  if (asymm::CheckSignature(find_nodes.original_request(),
  //                            find_nodes.original_signature(),
  //                            routing_table.kKeys().public_key) != kSuccess) {
  //    LOG(kError) << " find node request was not signed by us";
  //    return;  // we never requested this
  //  }

  LOG(kVerbose) << "[" << connections_.kNodeId() << "] received FindNodes response from "
                << HexSubstr(message.source_id()) << " id: " << message.id();
  std::string find_node_result =
      "FindNodes from " + HexSubstr(message.source_id()) + " returned :\n";
  for (int i = 0; i < find_nodes_response.nodes_size(); ++i) {
    find_node_result += "[" + HexSubstr(find_nodes_response.nodes(i)) + "]\t";
  }

  LOG(kVerbose) << find_node_result;

  for (int i = 0; i < find_nodes_response.nodes_size(); ++i) {
    if (!find_nodes_response.nodes(i).empty())
      CheckAndSendConnectRequest(NodeId(find_nodes_response.nodes(i)));
  }
}

template <typename NodeType>
void ResponseHandler<NodeType>::SendConnectRequest(const NodeId peer_node_id) {
  if (network_.bootstrap_connection_id().IsZero() && (connections_.routing_table.size() == 0)) {
    LOG(kWarning) << "Need to re bootstrap !";
    return;
  }
  bool send_to_bootstrap_connection(
      (connections_.routing_table.size() < Parameters::closest_nodes_size) &&
      !network_.bootstrap_connection_id().IsZero());
  NodeInfo peer;
  peer.id = peer_node_id;

  if (peer.id == NodeId(connections_.kNodeId())) {
    //    LOG(kInfo) << "Can't send connect request to self !";
    return;
  }

  if (connections_.routing_table.CheckNode(peer)) {
    LOG(kVerbose) << "CheckNode succeeded for node " << peer.id;
    rudp::EndpointPair this_endpoint_pair, peer_endpoint_pair;
    rudp::NatType this_nat_type(rudp::NatType::kUnknown);
    int ret_val = network_.GetAvailableEndpoint(peer.id, peer_endpoint_pair, this_endpoint_pair,
                                                this_nat_type);
    if (rudp::kSuccess != ret_val && rudp::kBootstrapConnectionAlreadyExists != ret_val) {
      if (rudp::kUnvalidatedConnectionAlreadyExists != ret_val &&
          rudp::kConnectAttemptAlreadyRunning != ret_val) {
        LOG(kError) << "[" << connections_.kNodeId() << "] Response Handler"
                    << "Failed to get available endpoint for new connection to : " << peer.id
                    << "peer_endpoint_pair.external = " << peer_endpoint_pair.external
                    << ", peer_endpoint_pair.local = " << peer_endpoint_pair.local
                    << ". Rudp returned :" << ret_val;
      } else {
        LOG(kVerbose) << "Already ongoing attempt to : " << DebugId(peer.id);
      }
      return;
    }
    assert((!this_endpoint_pair.external.address().is_unspecified() ||
            !this_endpoint_pair.local.address().is_unspecified()) &&
           "Unspecified endpoint after GetAvailableEndpoint success.");
    NodeId relay_connection_id;
    bool relay_message(false);
    if (send_to_bootstrap_connection) {
      // Not in any peer's routing table, need a path back through relay IP.
      relay_connection_id = network_.this_node_relay_connection_id();
      relay_message = true;
    }
    protobuf::Message connect_rpc(rpcs::Connect(PeerNodeId(peer.id),
                                                SelfEndpoint(this_endpoint_pair),
                                                SelfNodeId(connections_.kNodeId()),
                                                SelfConnectionId(connections_.kConnectionId()),
                                                IsClient(NodeType::value),
                                                NatType(this_nat_type),
                                                IsRelayMessage(relay_message),
                                                RelayConnectionId(relay_connection_id)));
    LOG(kVerbose) << "Sending Connect RPC to " << DebugId(peer.id)
                  << " message id : " << connect_rpc.id();
    if (send_to_bootstrap_connection)
      network_.SendToDirect(connect_rpc, network_.bootstrap_connection_id(),
                            network_.bootstrap_connection_id());
    else
      network_.SendToClosestNode(connect_rpc);
  }
}

template <typename NodeType>
protobuf::Message ResponseHandler<NodeType>::HandleMessage(
    ConnectSuccessAcknowledgementRequestFromVault message_wrapper) {
  typedef ConnectSuccessAcknowledgementRequestFromVault::SourceType SourceType;
  ConnectSuccessAcknowledgement<SourceType>(message_wrapper.message);
  return message_wrapper.message;
}

template <typename NodeType>
protobuf::Message ResponseHandler<NodeType>::HandleMessage(
    ConnectSuccessAcknowledgementRequestFromClient message_wrapper) {
  typedef ConnectSuccessAcknowledgementRequestFromClient::SourceType SourceType;
  ConnectSuccessAcknowledgement<SourceType>(message_wrapper.message);
  return message_wrapper.message;
}

template <typename NodeType>
template <typename PeerNodeType>
void ResponseHandler<NodeType>::ConnectSuccessAcknowledgement(protobuf::Message& message) {
  protobuf::ConnectSuccessAcknowledgement connect_success_ack;

  if (!connect_success_ack.ParseFromString(message.data(0))) {
    LOG(kWarning) << "Unable to parse connect success ack.";
    message.Clear();
    return;
  }

  NodeInfo peer;
  if (!connect_success_ack.node_id().empty())
    peer.id = NodeId(connect_success_ack.node_id());
  if (!connect_success_ack.connection_id().empty())
    peer.connection_id = NodeId(connect_success_ack.connection_id());
  if (peer.id.IsZero()) {
    LOG(kWarning) << "Invalid node id provided";
    return;
  }
  if (peer.connection_id.IsZero()) {
    LOG(kWarning) << "Invalid peer connection_id provided";
    return;
  }

  bool from_requestor(connect_success_ack.requestor());
  std::vector<NodeId> close_ids;
  for (const auto& elem : connect_success_ack.close_ids()) {
    if (!(elem).empty()) {
      close_ids.push_back(NodeId(elem));
    }
  }
  ValidateAndCompleteConnection(peer, from_requestor, close_ids, PeerNodeType());
}

template <typename NodeType>
void ResponseHandler<NodeType>::ValidateAndCompleteConnection(
    const NodeInfo& peer, bool from_requestor, const std::vector<NodeId>& close_ids, ClientNode) {
  if (ValidateAndAddToRoutingTable(network_, connections_, peer.id, peer.connection_id,
                                   asymm::PublicKey(), ClientNode())) {
    if (from_requestor) {
      HandleSuccessAcknowledgementAsReponder(peer, true);
    } else {
      HandleSuccessAcknowledgementAsRequestor(close_ids);
    }
  }
}

template <typename NodeType>
void ResponseHandler<NodeType>::ValidateAndCompleteConnection(
    const NodeInfo& peer, bool from_requestor, const std::vector<NodeId>& close_ids, VaultNode) {
  std::weak_ptr<ResponseHandler<NodeType>> response_handler_weak_ptr = this->shared_from_this();
  if (request_public_key_functor_) {
    auto validate_node([=](const asymm::PublicKey& key) {
      LOG(kInfo) << "Validation callback called with public key for " << peer.id;
      if (std::shared_ptr<ResponseHandler> response_handler = response_handler_weak_ptr.lock()) {
        if (ValidateAndAddToRoutingTable(response_handler->network_, response_handler->connections_,
                                         peer.id, peer.connection_id, key, VaultNode())) {
          if (from_requestor) {
            response_handler->HandleSuccessAcknowledgementAsReponder(peer, false);
          } else {
            response_handler->HandleSuccessAcknowledgementAsRequestor(close_ids);
          }
        }
      }
    });
    request_public_key_functor_(peer.id, validate_node);
  }
}

template <typename NodeType>
void ResponseHandler<NodeType>::HandleSuccessAcknowledgementAsReponder(NodeInfo peer, bool client) {
  auto count =
      (client ? Parameters::max_routing_table_size_for_client : Parameters::max_routing_table_size);
  auto close_nodes_for_peer(connections_.routing_table.GetClosestNodes(peer.id, count));
  auto itr(std::find_if(std::begin(close_nodes_for_peer), std::end(close_nodes_for_peer),
                        [=](const NodeInfo & info)->bool { return (peer.id == info.id); }));
  if (itr != std::end(close_nodes_for_peer))
    close_nodes_for_peer.erase(itr);

  protobuf::Message connect_success_ack(rpcs::ConnectSuccessAcknowledgement(
      PeerNodeId(peer.id), connections_.kNodeId(), connections_.kConnectionId(), IsRequestor(false),
      close_nodes_for_peer, IsClient(NodeType::value)));
  network_.SendToDirect(connect_success_ack, peer.id, peer.connection_id);
}

template <typename NodeType>
void ResponseHandler<NodeType>::HandleSuccessAcknowledgementAsRequestor(
    const std::vector<NodeId>& close_ids) {
  for (const auto& i : close_ids) {
    if (!i.IsZero()) {
      CheckAndSendConnectRequest(i);
    }
  }
}

template <typename NodeType>
void ResponseHandler<NodeType>::CheckAndSendConnectRequest(const NodeId& node_id) {
  if ((connections_.routing_table.size() < Params<NodeType>::max_routing_table_size) ||
      NodeId::CloserToTarget(
          node_id,
          connections_.routing_table.GetNthClosestNode(connections_.kNodeId(),
                                                       Params<NodeType>::closest_nodes_size).id,
          connections_.kNodeId()))
    SendConnectRequest(node_id);
}

template <typename NodeType>
void ResponseHandler<NodeType>::CloseNodeUpdateForClient(protobuf::Message& message) {
  assert(NodeType::value);
  if (message.destination_id() != connections_.kNodeId().data.string()) {
    // Message not for this node and we should not pass it on.
    LOG(kError) << "Message not for this node.";
    message.Clear();
    return;
  }
  protobuf::ClosestNodesUpdate closest_node_update;
  if (!closest_node_update.ParseFromString(message.data(0))) {
    LOG(kError) << "No Data.";
    return;
  }

  if (closest_node_update.node().empty() || !CheckId(closest_node_update.node())) {
    LOG(kError) << "Invalid node id provided.";
    return;
  }

  std::vector<NodeId> closest_nodes;
  for (const auto& basic_info : closest_node_update.nodes_info()) {
    if (CheckId(basic_info.node_id())) {
      closest_nodes.push_back(NodeId(basic_info.node_id()));
    }
  }
  assert(!closest_nodes.empty());
  HandleSuccessAcknowledgementAsRequestor(closest_nodes);
  message.Clear();
}

template <typename NodeType>
void ResponseHandler<NodeType>::InformClientOfNewCloseNode(protobuf::Message& message) {
  assert(NodeType::value && "Handler must be client");
  if (message.destination_id() != connections_.kNodeId().data.string()) {
    // Message not for this node and we should not pass it on.
    LOG(kError) << "Message not for this node.";
    message.Clear();
    return;
  }
  protobuf::InformClientOfhNewCloseNode inform_client_of_new_close_node;
  if (!inform_client_of_new_close_node.ParseFromString(message.data(0))) {
    LOG(kError) << "Failure to parse";
    return;
  }

  if (inform_client_of_new_close_node.node_id().empty() ||
      !CheckId(inform_client_of_new_close_node.node_id())) {
    LOG(kError) << "Invalid node id provided.";
    return;
  }
  CheckAndSendConnectRequest(NodeId(inform_client_of_new_close_node.node_id()));
  message.Clear();
}

template <typename NodeType>
void ResponseHandler<NodeType>::GetGroup(Timer<std::string>& timer, protobuf::Message& message) {
  try {
    if (!message.has_id() || message.data_size() != 1)
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
    timer.AddResponse(message.id(), message.data(0));
  }
  catch (const maidsafe_error& e) {
    LOG(kError) << e.what();
    return;
  }
}

template <typename NodeType>
void ResponseHandler<NodeType>::set_request_public_key_functor(
    RequestPublicKeyFunctor request_public_key) {
  request_public_key_functor_ = request_public_key;
}

template <typename NodeType>
RequestPublicKeyFunctor ResponseHandler<NodeType>::request_public_key_functor() const {
  return request_public_key_functor_;
}

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_RESPONSE_HANDLER_H_
