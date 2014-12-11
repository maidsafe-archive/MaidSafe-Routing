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

#include "maidsafe/routing/response_handler.h"

#include <memory>
#include <vector>
#include <string>
#include <algorithm>

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/client_routing_table.h"
#include "maidsafe/routing/network.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/utils.h"

namespace bptime = boost::posix_time;

namespace maidsafe {

namespace routing {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // unnamed namespace

ResponseHandler::ResponseHandler(
    RoutingTable& routing_table, ClientRoutingTable& client_routing_table, Network& network,
    PublicKeyHolder& public_key_holder)
    : mutex_(), routing_table_(routing_table), client_routing_table_(client_routing_table),
      network_(network), request_public_key_functor_(), public_key_holder_(public_key_holder)  {}

ResponseHandler::~ResponseHandler() {}

void ResponseHandler::Ping(protobuf::Message& message) {
  // Always direct, never pass on

  // TODO(dirvine): do we need this and where and how can I update the response
  protobuf::PingResponse ping_response;
  if (ping_response.ParseFromString(message.data(0))) {
    // do stuff here
  }
}

void ResponseHandler::Connect(protobuf::Message& message) {
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
    return;
  }

  if (connect_response.answer() == protobuf::ConnectResponseType::kConnectAttemptAlreadyRunning) {
    return;
  }

  if (!NodeId(connect_response.contact().node_id()).IsValid()) {
    LOG(kError) << "Invalid contact details";
    return;
  }

  NodeInfo node_to_add;
  node_to_add.id = NodeId(connect_response.contact().node_id());
  if (routing_table_.CheckNode(node_to_add) ||
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

    if (!public_key_holder_.Find(peer_node_id)) {
      LOG(kError)  << "missing public key ";
      message.Clear();
      return;
    }

    auto result(AddToRudp(network_, routing_table_.kNodeId(), routing_table_.kConnectionId(),
                          peer_node_id, peer_connection_id, peer_endpoint_pair, true,  // requestor
                          routing_table_.client_mode()));
    if (result != kSuccess)
      LOG(kWarning) << "Already added node";
  }
}

void ResponseHandler::FindNodes(const protobuf::Message& message) {
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
        find_nodes_response.nodes(0) == routing_table_.kNodeId().string()) {
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

  for (int i = 0; i < find_nodes_response.nodes_size(); ++i) {
    if (!find_nodes_response.nodes(i).empty())
      CheckAndSendConnectRequest(NodeId(find_nodes_response.nodes(i)));
  }
}

void ResponseHandler::SendConnectRequest(const NodeId peer_node_id) {
  if (!network_.bootstrap_connection_id().IsValid() && (routing_table_.size() == 0)) {
    LOG(kWarning) << "Need to re bootstrap !";
    return;
  }
  bool send_to_bootstrap_connection((routing_table_.size() < Parameters::closest_nodes_size) &&
                                    network_.bootstrap_connection_id().IsValid());
  NodeInfo peer;
  peer.id = peer_node_id;

  if (peer.id == NodeId(routing_table_.kNodeId())) {
    return;
  }

  if (routing_table_.CheckNode(peer)) {
    rudp::EndpointPair this_endpoint_pair, peer_endpoint_pair;
    rudp::NatType this_nat_type(rudp::NatType::kUnknown);
    int ret_val = network_.GetAvailableEndpoint(peer.id, peer_endpoint_pair,
                                                this_endpoint_pair, this_nat_type);
    if (rudp::kSuccess != ret_val && rudp::kBootstrapConnectionAlreadyExists != ret_val) {
      if (rudp::kUnvalidatedConnectionAlreadyExists != ret_val &&
          rudp::kConnectAttemptAlreadyRunning != ret_val) {
        LOG(kError) << "[" << DebugId(routing_table_.kNodeId()) << "] Response Handler"
                    << "Failed to get available endpoint for new connection to : " << peer.id
                    << "peer_endpoint_pair.external = " << peer_endpoint_pair.external
                    << ", peer_endpoint_pair.local = " << peer_endpoint_pair.local
                    << ". Rudp returned :" << ret_val;
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
    protobuf::Message connect_rpc(rpcs::Connect(
        peer.id, this_endpoint_pair, routing_table_.kNodeId(), routing_table_.kConnectionId(),
        routing_table_.client_mode(), this_nat_type, relay_message, relay_connection_id));
    if (send_to_bootstrap_connection)
      network_.SendToDirect(connect_rpc, network_.bootstrap_connection_id(),
                            network_.bootstrap_connection_id());
    else
      network_.SendToClosestNode(connect_rpc);
  }
}

void ResponseHandler::ConnectSuccess(protobuf::Message& message) {
  message.Clear();  // message is sent directly to the peer
}

void ResponseHandler::ConnectSuccessAcknowledgement(protobuf::Message& message) {
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
  if (!peer.id.IsValid()) {
    LOG(kWarning) << "Invalid node id provided";
    return;
  }
  if (!peer.connection_id.IsValid()) {
    LOG(kWarning) << "Invalid peer connection_id provided";
    return;
  }

  bool from_requestor(connect_success_ack.requestor());
  bool client_node(message.client_node());
  std::vector<NodeId> close_ids;
  for (const auto& elem : connect_success_ack.close_ids()) {
    if (!(elem).empty()) {
      close_ids.push_back(NodeId(elem));
    }
  }
  if (!client_node) {
    ValidateAndCompleteConnectionToNonClient(peer, from_requestor, close_ids);
  } else {
    ValidateAndCompleteConnectionToClient(peer, from_requestor, close_ids);
  }
  message.Clear();
}

void ResponseHandler::ValidateAndCompleteConnectionToClient(const NodeInfo& peer,
                                                            bool from_requestor,
                                                            const std::vector<NodeId>& close_ids) {
  if (ValidateAndAddToRoutingTable(network_, routing_table_, client_routing_table_, peer.id,
                                   peer.connection_id, asymm::PublicKey(), true)) {
    if (from_requestor) {
      HandleSuccessAcknowledgementAsReponder(peer, true);
    } else {
      HandleSuccessAcknowledgementAsRequestor(close_ids);
    }
  }
}

void ResponseHandler::ValidateAndCompleteConnectionToNonClient(
    const NodeInfo& peer, bool from_requestor, const std::vector<NodeId>& close_ids) {
  auto peer_public_key(public_key_holder_.Find(peer.id));
  if (!peer_public_key) {
    return;
  }
  public_key_holder_.Remove(peer.id);
  if (ValidateAndAddToRoutingTable(network_, routing_table_, client_routing_table_, peer.id,
                                   peer.connection_id, *peer_public_key, false)) {
    if (from_requestor) {
      HandleSuccessAcknowledgementAsReponder(peer, false);
    } else {
      HandleSuccessAcknowledgementAsRequestor(close_ids);
    }
  }
}

void ResponseHandler::HandleSuccessAcknowledgementAsReponder(NodeInfo peer, bool client) {
  auto count =
      (client ? Parameters::max_routing_table_size_for_client : Parameters::max_routing_table_size);
  auto close_nodes_for_peer(routing_table_.GetClosestNodes(peer.id, count));
  auto itr(std::find_if(std::begin(close_nodes_for_peer), std::end(close_nodes_for_peer),
                        [=](const NodeInfo&  info)->bool {
                          return (peer.id == info.id);
                        }));
  if (itr != std::end(close_nodes_for_peer))
    close_nodes_for_peer.erase(itr);

  protobuf::Message connect_success_ack(rpcs::ConnectSuccessAcknowledgement(
      peer.id, routing_table_.kNodeId(), routing_table_.kConnectionId(),
      false,  // this node is responder
      close_nodes_for_peer, routing_table_.client_mode()));
  network_.SendToDirect(connect_success_ack, peer.id, peer.connection_id);
}

void ResponseHandler::HandleSuccessAcknowledgementAsRequestor(
    const std::vector<NodeId>& close_ids) {
  for (const auto& i : close_ids) {
    if (i.IsValid()) {
      CheckAndSendConnectRequest(i);
    }
  }
}

void ResponseHandler::CheckAndSendConnectRequest(const NodeId& node_id) {
  if (node_id == routing_table_.kNodeId())
    return;
  if (routing_table_.Contains(node_id))
    return;
  if (public_key_holder_.Find(node_id))
    return;
  unsigned int limit(routing_table_.client_mode() ? Parameters::max_routing_table_size_for_client
                                                  : Parameters::closest_nodes_size);
  if ((routing_table_.size() < routing_table_.kMaxSize()) ||
      NodeId::CloserToTarget(
          node_id, routing_table_.GetNthClosestNode(routing_table_.kNodeId(), limit).id,
          routing_table_.kNodeId()))
    ValidateAndSendConnectRequest(node_id);
}

void ResponseHandler::ValidateAndSendConnectRequest(const NodeId& peer_id) {
  std::weak_ptr<ResponseHandler> response_handler_weak_ptr = shared_from_this();
  if (request_public_key_functor_) {
    auto validate_node([=](boost::optional<asymm::PublicKey> public_key) {
      if (!public_key) {
        LOG(kError) << "Failed to retrieve public key for: " << peer_id;
        return;
      }
      if (std::shared_ptr<ResponseHandler> response_handler = response_handler_weak_ptr.lock()) {
        response_handler->public_key_holder_.Add(peer_id, *public_key);
        response_handler->SendConnectRequest(peer_id);
      }
    });
    request_public_key_functor_(peer_id, validate_node);
  }
}

void ResponseHandler::CloseNodeUpdateForClient(protobuf::Message& message) {
  assert(routing_table_.client_mode());
  if (message.destination_id() != routing_table_.kNodeId().string()) {
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

void ResponseHandler::InformClientOfNewCloseNode(protobuf::Message& message) {
  assert(routing_table_.client_mode() && "Handler must be client");
  if (message.destination_id() != routing_table_.kNodeId().string()) {
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

void ResponseHandler::GetGroup(Timer<std::string>& timer, protobuf::Message& message) {
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

void ResponseHandler::set_request_public_key_functor(RequestPublicKeyFunctor request_public_key) {
  request_public_key_functor_ = request_public_key;
}

RequestPublicKeyFunctor ResponseHandler::request_public_key_functor() const {
  return request_public_key_functor_;
}

}  // namespace routing

}  // namespace maidsafe
