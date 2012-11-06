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

#include "maidsafe/routing/response_handler.h"

#include<memory>
#include<vector>
#include<string>
#include <algorithm>

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/utils.h"

namespace bptime = boost::posix_time;

namespace maidsafe {

namespace routing {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // unnamed namespace

ResponseHandler::ResponseHandler(RoutingTable& routing_table,
                                 NonRoutingTable& non_routing_table,
                                 NetworkUtils& network)
    : mutex_(),
      routing_table_(routing_table),
      non_routing_table_(non_routing_table),
      network_(network),
      request_public_key_functor_() {
}

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
    LOG(kError) << "Could not parse original connect request" << " id: " << message.id();
    return;
  }

  if (connect_response.answer() == protobuf::ConnectResponseType::kRejected) {
    LOG(kInfo) << "Peer rejected this node's connection request." << " id: " << message.id();
    return;
  }

  if (connect_response.answer() ==
        protobuf::ConnectResponseType::kConnectAttemptAlreadyRunning) {
    LOG(kInfo) << "Already ongoing connection attempt with : "
               << HexSubstr(connect_response.contact().node_id());
    return;
  }

  if (NodeId(connect_response.contact().node_id()).IsZero()) {
    LOG(kError) << "Invalid contact details";
    return;
  }

  NodeInfo node_to_add;
  node_to_add.node_id = NodeId(connect_response.contact().node_id());
  if (routing_table_.CheckNode(node_to_add) ||
      (node_to_add.node_id == network_.bootstrap_connection_id())) {
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
    // Updating bootstrap only for test local network
    if (!peer_endpoint_pair.external.address().is_unspecified())
      network_.AddToBootstrapFile(peer_endpoint_pair.external);
    else
      network_.AddToBootstrapFile(peer_endpoint_pair.local);

    NodeId peer_node_id(connect_response.contact().node_id());
    NodeId peer_connection_id(connect_response.contact().connection_id());

    LOG(kVerbose) << "This node [" << DebugId(routing_table_.kNodeId())
                  << "] received connect response from "
                  << DebugId(peer_node_id)
                  << " id: " << message.id();

    int result = AddToRudp(network_, routing_table_.kNodeId(),
                           routing_table_.kConnectionId(),
                           peer_node_id,
                           peer_connection_id,
                           peer_endpoint_pair,
                           true,  // requestor
                           routing_table_.client_mode());
    if (result == kSuccess) {
      // Special case with bootstrapping peer in which kSuccess comes before connect respons
      if (peer_node_id == network_.bootstrap_connection_id()) {
        LOG(kInfo) << "Special case with bootstrapping peer : "  << DebugId(peer_node_id);
        const std::vector<NodeId> close_ids;  // add closer ids if needed
        protobuf::Message connect_success_ack(
            rpcs::ConnectSuccessAcknowledgement(peer_node_id,
            routing_table_.kNodeId(),
            routing_table_.kConnectionId(),
            true,  // this node is requestor
            close_ids,
            routing_table_.client_mode()));
        network_.SendToDirect(connect_success_ack, peer_node_id, peer_connection_id);
      }
    }
  } else {
    LOG(kVerbose) << "Already added node";
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

  LOG(kVerbose) << "[" << DebugId(routing_table_.kNodeId())
                << "] received FindNodes response from " << HexSubstr(message.source_id())
                << " id: " << message.id();
  std::string find_node_result = "FindNodes from " + HexSubstr(message.source_id()) +
          " returned :\n";
  for (int i = 0; i < find_nodes_response.nodes_size(); ++i) {
    find_node_result += "[" + HexSubstr(find_nodes_response.nodes(i)) + "]\t";
  }

  LOG(kVerbose) << find_node_result;

  for (int i = 0; i < find_nodes_response.nodes_size(); ++i) {
    if (!find_nodes_response.nodes(i).empty())
      SendConnectRequest(NodeId(find_nodes_response.nodes(i)));
  }
}

void ResponseHandler::SendConnectRequest(const NodeId peer_node_id) {
  if (network_.bootstrap_connection_id().IsZero() && (routing_table_.size() == 0)) {
      LOG(kWarning) << "Need to re bootstrap !";
    return;
  }
  bool send_to_bootstrap_connection((routing_table_.size() < Parameters::closest_nodes_size) &&
                                    !network_.bootstrap_connection_id().IsZero());
  NodeInfo peer;
  peer.node_id = peer_node_id;

  if (peer.node_id == NodeId(routing_table_.kFob().identity)) {
    return;
  }

  if (routing_table_.CheckNode(peer)) {
    LOG(kVerbose) << "CheckNode succeeded for node " << DebugId(peer.node_id);
    rudp::EndpointPair this_endpoint_pair, peer_endpoint_pair;
    rudp::NatType this_nat_type(rudp::NatType::kUnknown);
    int ret_val = network_.GetAvailableEndpoint(peer.node_id,
                                                peer_endpoint_pair,
                                                this_endpoint_pair,
                                                this_nat_type);
    if (rudp::kSuccess != ret_val && rudp::kBootstrapConnectionAlreadyExists != ret_val) {
      if (rudp::kUnvalidatedConnectionAlreadyExists != ret_val &&
            rudp::kConnectAttemptAlreadyRunning != ret_val) {
        LOG(kError) << "[" << DebugId(routing_table_.kNodeId()) << "] Response Handler"
                    << "Failed to get available endpoint for new connection to : "
                    << DebugId(peer.node_id)
                    << "peer_endpoint_pair.external = "
                    << peer_endpoint_pair.external
                    << ", peer_endpoint_pair.local = "
                    << peer_endpoint_pair.local
                    << ". Rudp returned :"
                    << ret_val;
      } else {
        LOG(kVerbose) << "Already ongoing attempt to : " << DebugId(peer.node_id);
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
    protobuf::Message connect_rpc(
        rpcs::Connect(peer.node_id,
        this_endpoint_pair,
        routing_table_.kNodeId(),
        routing_table_.kConnectionId(),
        routing_table_.client_mode(),
        this_nat_type,
        relay_message,
        relay_connection_id));
    LOG(kVerbose) << "Sending Connect RPC to " << DebugId(peer.node_id)
                  << " message id : " << connect_rpc.id();
    if (send_to_bootstrap_connection)
      network_.SendToDirect(connect_rpc, network_.bootstrap_connection_id(),
                            network_.bootstrap_connection_id());
    else
      network_.SendToClosestNode(connect_rpc);
  }
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
    peer.node_id = NodeId(connect_success_ack.node_id());
  if (!connect_success_ack.connection_id().empty())
    peer.connection_id = NodeId(connect_success_ack.connection_id());
  if (peer.node_id.IsZero()) {
    LOG(kWarning) << "Invalid node id provided";
    return;
  }
  if (peer.connection_id.IsZero()) {
    LOG(kWarning) << "Invalid peer connection_id provided";
    return;
  }

  bool from_requestor(connect_success_ack.requestor());
  bool client_node(message.client_node());
  std::vector<NodeId> close_ids;
  for (auto itr = connect_success_ack.close_ids().begin();
           itr != connect_success_ack.close_ids().end(); ++itr) {
    if (!(*itr).empty()) {
      close_ids.push_back(NodeId(*itr));
    }
  }

  std::weak_ptr<ResponseHandler> response_handler_weak_ptr = shared_from_this();
  if (request_public_key_functor_) {
    auto validate_node([=] (const asymm::PublicKey& key) {
                           LOG(kInfo) << "Validation callback called with public key for "
                                      << DebugId(peer.node_id);
                           if (std::shared_ptr<ResponseHandler> response_handler =
                               response_handler_weak_ptr.lock()) {
                             if (ValidateAndAddToRoutingTable(
                                 response_handler->network_,
                                 response_handler->routing_table_,
                                 response_handler->non_routing_table_,
                                 peer.node_id,
                                 peer.connection_id,
                                 key,
                                 client_node)) {
                               if (from_requestor) {
                                 response_handler->HandleSuccessAcknowledgementAsReponder(
                                       peer, client_node, close_ids);
                               } else {
                                 response_handler->HandleSuccessAcknowledgementAsRequestor(
                                       close_ids);
                               }
                             }
                           }
                         });
      request_public_key_functor_(peer.node_id, validate_node);
  }
}

// FIXME life time issue with weak pointers
void ResponseHandler::HandleSuccessAcknowledgementAsReponder(NodeInfo peer,
                                                             const bool &client,
                                                             std::vector<NodeId> /*close_ids*/) {
  auto count =
      (client ? Parameters::max_client_routing_table_size: Parameters::max_routing_table_size);
  std::vector<NodeId> close_ids_for_peer(
      routing_table_.GetClosestNodes(peer.node_id, count));
  auto itr(std::find_if(close_ids_for_peer.begin(), close_ids_for_peer.end(),
                        [=](const NodeId& node_id)->bool {
                            return (peer.node_id == node_id);
                          }));
  if (itr != close_ids_for_peer.end())
    close_ids_for_peer.erase(itr);

  protobuf::Message connect_success_ack(
      rpcs::ConnectSuccessAcknowledgement(peer.node_id,
                                          routing_table_.kNodeId(),
                                          routing_table_.kConnectionId(),
                                          false,  // this node is responder
                                          close_ids_for_peer,
                                          routing_table_.client_mode()));
  network_.SendToDirect(connect_success_ack, peer.node_id, peer.connection_id);

// Connect to close ids provided by peer
// TODO(Prakash) : uncomment below once GetavailableEndpoint returns alreadyConnected
//  for (auto i : close_ids) {
//    LOG(kVerbose) << "HandleSuccessAcknowledgementAsReponder: connecting " <<DebugId(i);
//    if (!i.Empty()) {
//      SendConnectRequest(i);
//    }
//  }
}

void ResponseHandler::HandleSuccessAcknowledgementAsRequestor(std::vector<NodeId> close_ids) {
  for (auto i : close_ids) {
    if (!i.IsZero()) {
      if ((routing_table_.size() < Parameters::greedy_fraction) ||
          NodeId::CloserToTarget(i,
                                 routing_table_.GetNthClosestNode(routing_table_.kNodeId(),
                                     Parameters::greedy_fraction).node_id,
                                 routing_table_.kNodeId()))
        SendConnectRequest(i);
    }
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
