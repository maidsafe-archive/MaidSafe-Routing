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

#include "maidsafe/common/log.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/utils.h"


namespace maidsafe {

namespace routing {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // unnamed namespace

ResponseHandler::ResponseHandler(AsioService& io_service,
                                 RoutingTable& routing_table,
                                 NonRoutingTable& non_routing_table,
                                 NetworkUtils& network)
    : io_service_(io_service),
      routing_table_(routing_table),
      non_routing_table_(non_routing_table),
      network_(network),
      request_public_key_functor_() {}


void ResponseHandler::set_request_public_key_functor(RequestPublicKeyFunctor request_public_key) {
  request_public_key_functor_ = request_public_key;
}
// always direct !! never pass on
void ResponseHandler::Ping(protobuf::Message& message) {
  // TODO(dirvine): do we need this and where and how can I update the response
  protobuf::PingResponse ping_response;
  if (ping_response.ParseFromString(message.data(0))) {
    //  do stuff here
  }
}

// the other node agreed to connect - he has accepted our connection
void ResponseHandler::Connect(protobuf::Message& message) {
  protobuf::ConnectResponse connect_response;
  protobuf::ConnectRequest connect_request;
  if (!connect_response.ParseFromString(message.data(0))) {
    LOG(kError) << "Could not parse connect response";
    return;
  }

  if (!connect_response.answer()) {
    LOG(kVerbose) << "ResponseHandler::Connect() they don't want us";
    return;  // they don't want us
  }
  if (!connect_request.ParseFromString(connect_response.original_request()))
    return;  // invalid response

  LOG(kVerbose) << "Received connect response from "
                << HexSubstr(connect_request.contact().node_id())
                << ", I am " << HexSubstr(routing_table_.kKeys().identity);
  rudp::EndpointPair our_endpoint_pair;
  our_endpoint_pair.external = GetEndpointFromProtobuf(connect_request.contact().public_endpoint());
  our_endpoint_pair.local = GetEndpointFromProtobuf(connect_request.contact().private_endpoint());

  rudp::EndpointPair their_endpoint_pair;
  their_endpoint_pair.external =
      GetEndpointFromProtobuf(connect_response.contact().public_endpoint());
  their_endpoint_pair.local =
      GetEndpointFromProtobuf(connect_response.contact().private_endpoint());

  if (request_public_key_functor_) {
    auto validate_node =
      [=] (const asymm::PublicKey& key) {
          LOG(kInfo) << "NEED TO VALIDATE THE NODE HERE";
          ValidateThisNode(this->network_,
                           this->routing_table_,
                           this->non_routing_table_,
                           NodeId(connect_response.contact().node_id()),
                           key,
                           their_endpoint_pair,
                           our_endpoint_pair,
                           false);
      };
    request_public_key_functor_(NodeId(connect_response.contact().node_id()), validate_node);
  }
}

void ResponseHandler::FindNode(const protobuf::Message& message) {
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

  LOG(kVerbose) << "Find node from "  << HexSubstr(message.source_id())
                << ", returned " << find_nodes.nodes_size()
                << ". I am [" << HexSubstr(routing_table_.kKeys().identity) << "]";
  for (int i = 0; i < find_nodes.nodes_size(); ++i) {
    LOG(kVerbose) << " Find node from "  << HexSubstr(message.source_id())
                  << " returned - "  << HexSubstr(find_nodes.nodes(i))
                  << ". nodes, I am ["
                  << HexSubstr(routing_table_.kKeys().identity) << "]";
  }

  for (int i = 0; i < find_nodes.nodes_size(); ++i) {
    NodeInfo node_to_add;
    node_to_add.node_id = NodeId(find_nodes.nodes(i));
    if (node_to_add.node_id == NodeId(routing_table_.kKeys().identity))
      continue;  // TODO(Prakash): FIXME handle collision and return kIdCollision on join()
    if (routing_table_.CheckNode(node_to_add)) {
      LOG(kVerbose) << " CheckNode succeeded for node "
                    << HexSubstr(node_to_add.node_id.String());
      Endpoint direct_endpoint;
      bool routing_table_empty(routing_table_.Size() == 0);
      if (routing_table_empty)  // Joining the network, and may connect to bootstrapping node.
        direct_endpoint = network_.bootstrap_endpoint();
      rudp::EndpointPair endpoint;
      if (kSuccess != network_.GetAvailableEndpoint(direct_endpoint, endpoint)) {
        LOG(kWarning) << " Failed to get available endpoint for new connections";
        return;
      }
      Endpoint relay_endpoint;
      bool relay_message(false);
      if (routing_table_empty) {  //  Not in anyones RT, need a path back through relay ip.
        relay_endpoint = network_.my_relay_endpoint();
        relay_message = true;
      }
      LOG(kVerbose) << " Sending Connect rpc to - " << HexSubstr(find_nodes.nodes(i));
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
// post it to timer
  if (routing_table_.Size() < Parameters::closest_nodes_size) {
  LOG(kVerbose) << " Routing table smaller than " << Parameters::closest_nodes_size
                << "Nodes. Sending another FindNode...";
    // ReSendFindNodeRequest();
  }
}

void ResponseHandler::ReSendFindNodeRequest() {
  bool relay_message(false);
  Endpoint relay_endpoint;
  bool routing_table_empty(routing_table_.Size() == 0);
  if (routing_table_empty) {  //  Not in anyones RT, need a path back through relay ip.
    relay_endpoint = network_.my_relay_endpoint();
    relay_message = true;
  }
  protobuf::Message find_node_rpc(rpcs::FindNodes(NodeId(routing_table_.kKeys().identity),
                                                  NodeId(routing_table_.kKeys().identity),
                                                  relay_message,
                                                  relay_endpoint));
  if (routing_table_empty)
    network_.SendToDirectEndpoint(find_node_rpc, network_.bootstrap_endpoint());
  else
    network_.SendToClosestNode(find_node_rpc);
}

void ResponseHandler::ProxyConnect(protobuf::Message& message) {
  protobuf::ProxyConnectResponse proxy_connect_response;
  if (proxy_connect_response.ParseFromString(message.data(0))) {
    //  do stuff here
  }
}

}  // namespace routing

}  // namespace maidsafe
