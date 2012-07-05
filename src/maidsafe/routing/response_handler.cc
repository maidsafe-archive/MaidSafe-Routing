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

#include "boost/thread/shared_mutex.hpp"
#include "boost/thread/mutex.hpp"

#include "maidsafe/common/rsa.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/log.h"
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

namespace response {

// always direct !! never pass on
void Ping(protobuf::Message& message) {
  // TODO(dirvine): do we need this and where and how can I update the response
  protobuf::PingResponse ping_response;
  if (ping_response.ParseFromString(message.data())) {
    //  do stuff here
  }
}

// the other node agreed to connect - he has accepted our connection
void Connect(RoutingTable &routing_table,
             NonRoutingTable &non_routing_table,
             rudp::ManagedConnections &rudp,
             protobuf::Message& message,
             RequestPublicKeyFunctor node_validation_functor) {
  protobuf::ConnectResponse connect_response;
  protobuf::ConnectRequest connect_request;
  if (!connect_response.ParseFromString(message.data())) {
    LOG(kError) << "Could not parse connect response";
    return;
  }
  LOG(kVerbose) << "ResponseHandler::Connect() Parsed connect node response";
  if (!connect_response.answer()) {
    LOG(kVerbose) << "ResponseHandler::Connect() they don't want us";
    return;  // they don't want us
  }
  if (!connect_request.ParseFromString(connect_response.original_request()))
    return;  // invalid response

  rudp::EndpointPair our_endpoint_pair;
  our_endpoint_pair.external = GetEndpointFromProtobuf(connect_request.contact().public_endpoint());
  our_endpoint_pair.local = GetEndpointFromProtobuf(connect_request.contact().private_endpoint());

  rudp::EndpointPair their_endpoint_pair;
  their_endpoint_pair.external =
      GetEndpointFromProtobuf(connect_response.contact().public_endpoint());
  their_endpoint_pair.local =
      GetEndpointFromProtobuf(connect_response.contact().private_endpoint());

  if (node_validation_functor) {
    auto validate_node =
      [=, &routing_table, &non_routing_table, &rudp] (const asymm::PublicKey &key) {
          LOG(kInfo) << "NEED TO VALIDATE THE NODE HERE";
          ValidateThisNode(rudp,
                           routing_table,
                           non_routing_table,
                           NodeId(connect_response.contact().node_id()),
                           key,
                           their_endpoint_pair,
                           our_endpoint_pair,
                           false);
        };
    node_validation_functor(NodeId(connect_response.contact().node_id()), validate_node);
  }
}

void FindNode(RoutingTable &routing_table,
              NonRoutingTable &non_routing_table,
              rudp::ManagedConnections &rudp,
              const protobuf::Message& message,
              const Endpoint &bootstrap_endpoint) {
  LOG(kVerbose) << "ResponseHandler::FindNode()";
  protobuf::FindNodesResponse find_nodes;
  if (!find_nodes.ParseFromString(message.data())) {
    LOG(kError) << "Could not parse find node response";
    return;
  }
  LOG(kVerbose) << "Parsed find node response";
//  if (asymm::CheckSignature(find_nodes.original_request(),
//                            find_nodes.original_signature(),
//                            routing_table.kKeys().public_key) != kSuccess) {
//    LOG(kError) << " find node request was not signed by us";
//    return;  // we never requested this
//  }
  for (int i = 0; i < find_nodes.nodes_size(); ++i) {
    LOG(kVerbose) << " Find node returned - "  << HexSubstr(find_nodes.nodes(i));
  }

  for (int i = 0; i < find_nodes.nodes_size(); ++i) {
    NodeInfo node_to_add;
    node_to_add.node_id = NodeId(find_nodes.nodes(i));
    if (node_to_add.node_id == NodeId(routing_table.kKeys().identity))
      continue;  // TODO(Prakash): FIXME handle collision and return kIdCollision on join()
    if (routing_table.CheckNode(node_to_add)) {
      LOG(kVerbose) << " CheckNode succeeded for node "
                    << HexSubstr(node_to_add.node_id.String());
      Endpoint direct_endpoint;
      if (routing_table.Size() == 0)  // Joining the network, and may connect to bootstrapping node.
        direct_endpoint = bootstrap_endpoint;
      rudp::EndpointPair endpoint;
      if (kSuccess != rudp.GetAvailableEndpoint(direct_endpoint, endpoint)) {
        LOG(kWarning) << " Failed to get available endpoint for new connections";
        return;
      }
      LOG(kVerbose) << " GetAvailableEndpoint for peer - " << direct_endpoint
                    << " my endpoint - " << endpoint.external;
      Endpoint relay_endpoint;
      bool relay_message(false);
      if (routing_table.Size() == 0) {  //  Not in anyones RT, need a path back through relay ip.
        relay_endpoint = endpoint.external;
        relay_message = true;
      }
      LOG(kVerbose) << " Sending Connect rpc to - " << HexSubstr(find_nodes.nodes(i));
      ProcessSend(rpcs::Connect(NodeId(find_nodes.nodes(i)),
                                endpoint,
                                NodeId(routing_table.kKeys().identity),
                                routing_table.client_mode(),
                                relay_message,
                                relay_endpoint),
                  rudp,
                  routing_table,
                  non_routing_table,
                  direct_endpoint);
    }
  }
}

void ProxyConnect(protobuf::Message& message) {
  protobuf::ProxyConnectResponse proxy_connect_response;
  if (proxy_connect_response.ParseFromString(message.data())) {
    //  do stuff here
    }
}

}  // namespace response

}  // namespace routing

}  // namespace maidsafe
