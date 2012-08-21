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

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/utils.h"


namespace maidsafe {

namespace routing {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // unnamed namespace

ResponseHandler::ResponseHandler(RoutingTable& routing_table,
                                 NonRoutingTable& non_routing_table,
                                 NetworkUtils& network)
    : routing_table_(routing_table),
      non_routing_table_(non_routing_table),
      network_(network),
      request_public_key_functor_() {}

void ResponseHandler::Ping(protobuf::Message& message) {
  // Always direct, never pass on

  // TODO(dirvine): do we need this and where and how can I update the response
  protobuf::PingResponse ping_response;
  if (ping_response.ParseFromString(message.data(0))) {
    // do stuff here
  }
}

void ResponseHandler::Connect(protobuf::Message& message) {
  // The peer agreed to connect
  protobuf::ConnectResponse connect_response;
  protobuf::ConnectRequest connect_request;
  if (!connect_response.ParseFromString(message.data(0))) {
    LOG(kError) << "Could not parse connect response";
    return;
  }

  if (!connect_response.answer()) {
    LOG(kVerbose) << "Peer rejected this node's connection request." << " id: " << message.id();
    return;
  }

  if (!connect_request.ParseFromString(connect_response.original_request())) {
    LOG(kError) << "Could not parse original connect request" << " id: " << message.id();
    return;
  }

  if (!NodeId(connect_response.contact().node_id()).IsValid()) {
    LOG(kError) << "Invalid contact details";
    return;
  }

  LOG(kVerbose) << "This node [" << HexSubstr(routing_table_.kKeys().identity)
                << "] received connect response from "
                << HexSubstr(connect_request.contact().node_id())
                << " id: " << message.id();
  rudp::EndpointPair this_endpoint_pair;
  this_endpoint_pair.external =
      GetEndpointFromProtobuf(connect_request.contact().public_endpoint());
  this_endpoint_pair.local = GetEndpointFromProtobuf(connect_request.contact().private_endpoint());

  rudp::EndpointPair peer_endpoint_pair;
  peer_endpoint_pair.external =
      GetEndpointFromProtobuf(connect_response.contact().public_endpoint());
  peer_endpoint_pair.local =
      GetEndpointFromProtobuf(connect_response.contact().private_endpoint());
  std::weak_ptr<ResponseHandler> response_handler_weak_ptr = shared_from_this();
  if (request_public_key_functor_) {
    auto validate_node([=] (const asymm::PublicKey& key) {
                           LOG(kInfo) << "NEED TO VALIDATE THE NODE HERE";
                           if (std::shared_ptr<ResponseHandler> response_handler =
                               response_handler_weak_ptr.lock()) {
                             ValidatePeer(response_handler->network_,
                                          response_handler->routing_table_,
                                          response_handler->non_routing_table_,
                                          NodeId(connect_response.contact().node_id()),
                                          key,
                                          peer_endpoint_pair,
                                          this_endpoint_pair,
                                          false);
                           }
                         });
    request_public_key_functor_(NodeId(connect_response.contact().node_id()), validate_node);
  }
}

void ResponseHandler::FindNodes(const protobuf::Message& message) {
  protobuf::FindNodesResponse find_nodes;
  if (!find_nodes.ParseFromString(message.data(0))) {
    LOG(kError) << "Could not parse find node response";
    return;
  }
//  if (asymm::CheckSignature(find_nodes.original_request(),
//                            find_nodes.original_signature(),
//                            routing_table.kKeys().public_key) != kSuccess) {
//    LOG(kError) << " find node request was not signed by us";
//    return;  // we never requested this
//  }

  LOG(kVerbose) << "This node [" << HexSubstr(routing_table_.kKeys().identity)
                << "] received FindNodes response from " << HexSubstr(message.source_id());
#ifndef NDEBUG
  for (int i = 0; i < find_nodes.nodes_size(); ++i) {
    LOG(kVerbose) << "FindNodes from " << HexSubstr(message.source_id())
                  << " returned " << HexSubstr(find_nodes.nodes(i));
  }
#endif

  for (int i = 0; i < find_nodes.nodes_size(); ++i) {
    NodeInfo node_to_add;
    node_to_add.node_id = NodeId(find_nodes.nodes(i));
    if (node_to_add.node_id == NodeId(routing_table_.kKeys().identity))
      continue;  // TODO(Prakash): FIXME handle collision and return kIdCollision on join()
    if (routing_table_.CheckNode(node_to_add)) {
      LOG(kVerbose) << "CheckNode succeeded for node " << HexSubstr(node_to_add.node_id.String());
      Endpoint direct_endpoint;
      bool routing_table_empty(routing_table_.Size() == 0);
      if (routing_table_empty)  // Joining the network, and may connect to bootstrapping node.
        direct_endpoint = network_.bootstrap_endpoint();
      rudp::EndpointPair endpoint;
      if (kSuccess != network_.GetAvailableEndpoint(direct_endpoint, endpoint)) {
        LOG(kWarning) << "Failed to get available endpoint for new connections";
        return;
      }
      Endpoint relay_endpoint;
      bool relay_message(false);
      if (routing_table_empty) {
        // Not in any peer's routing table, need a path back through relay IP.
        relay_endpoint = network_.this_node_relay_endpoint();
        relay_message = true;
      }
      LOG(kVerbose) << "Sending Connect RPC to " << HexSubstr(find_nodes.nodes(i));
      protobuf::Message connect_rpc(rpcs::Connect(NodeId(find_nodes.nodes(i)),
                                    endpoint,
                                    NodeId(routing_table_.kKeys().identity),
                                    routing_table_.client_mode(),
                                    relay_message,
                                    relay_endpoint));
      if (routing_table_empty)
        network_.SendToDirectEndpoint(connect_rpc, network_.bootstrap_endpoint());
      else
        network_.SendToClosestNode(connect_rpc);
    }
  }
}

void ResponseHandler::ProxyConnect(protobuf::Message& message) {
  protobuf::ProxyConnectResponse proxy_connect_response;
  if (proxy_connect_response.ParseFromString(message.data(0))) {
    // do stuff here
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
