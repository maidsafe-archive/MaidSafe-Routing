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

#include "maidsafe/routing/service.h"

#include <vector>

#include "maidsafe/common/utils.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/log.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

namespace service {

void Ping(RoutingTable &routing_table, protobuf::Message &message) {
  if (message.destination_id() != routing_table.kKeys().identity) {
    LOG(kError) << "Message not for us";
    return;  // not for us and we should not pass it on.
  }
  protobuf::PingResponse ping_response;
  protobuf::PingRequest ping_request;

  if (!ping_request.ParseFromString(message.data())) {
    LOG(kError) << "No Data";
    return;
  }
  ping_response.set_pong(true);
  ping_response.set_original_request(message.data());
  ping_response.set_original_signature(message.signature());
  ping_response.set_timestamp(GetTimeStamp());
  message.set_type(-1);
  message.set_data(ping_response.SerializeAsString());
  message.set_destination_id(message.source_id());
  message.set_source_id(routing_table.kKeys().identity);
  assert(message.IsInitialized() && "unintialised message");
}

void Connect(RoutingTable &routing_table,
             NonRoutingTable &non_routing_table,
             rudp::ManagedConnections &rudp,
             protobuf::Message &message,
             RequestPublicKeyFunctor node_validation_functor) {
  if (message.destination_id() != routing_table.kKeys().identity) {
    LOG(kWarning) << "Connect -- not for us and we should not pass it on."
                  << "Message destination id : "
                  << HexSubstr(message.destination_id())
                  << " . I am"
                  << HexSubstr(routing_table.kKeys().identity);
    message.Clear();
    return;  // not for us and we should not pass it on.
  }
  protobuf::ConnectRequest connect_request;
  protobuf::ConnectResponse connect_response;
  if (!connect_request.ParseFromString(message.data())) {
    LOG(kVerbose) << "Unable to parse connect request";
    message.Clear();
    return;  // no need to reply
  }
  NodeInfo node;
  node.node_id = NodeId(connect_request.contact().node_id());
  if (connect_request.bootstrap()) {
             // Already connected
             return;  // FIXME
  }
  connect_response.set_answer(false);
  rudp::EndpointPair our_endpoint_pair;
  rudp::EndpointPair their_endpoint_pair;
  their_endpoint_pair.external = GetEndpointFromProtobuf(connect_request.contact().
                                                           public_endpoint());
  their_endpoint_pair.local = GetEndpointFromProtobuf(connect_request.contact().
                                                        private_endpoint());
  // TODO(dirvine) try both connections
  if ((rudp.GetAvailableEndpoint(their_endpoint_pair.external, our_endpoint_pair)) != 0) {
    LOG(kError) << "Unable to get available endpoint to connect to"
                << their_endpoint_pair.external;
    return;
  }

  LOG(kVerbose) << " GetAvailableEndpoint for peer - " << their_endpoint_pair.external
                << " my endpoint - " << our_endpoint_pair.external;

  bool check_node_succeeded(false);
  if (message.client_node()) {  // Client node, check NRT
    LOG(kVerbose) << " client connect request - will check NRT";
    NodeId furthest_close_node_id =
        routing_table.GetClosestNode(NodeId(routing_table.kKeys().identity),
                                     Parameters::closest_nodes_size).node_id;
    check_node_succeeded = non_routing_table.CheckNode(node, furthest_close_node_id);
  } else {
    LOG(kVerbose) << " server connect request - will check RT";
    check_node_succeeded = routing_table.CheckNode(node);
  }

  if (check_node_succeeded) {
    LOG(kVerbose) << "CheckNode(node) for " << (message.client_node()?" client" : "server")
                  << " node succeeded !!";
    if (node_validation_functor) {
      auto validate_node =
          [=, &routing_table, &non_routing_table, &rudp] (const asymm::PublicKey &key)->void {
            LOG(kInfo) << "NEED TO VALIDATE THE NODE HERE";
            ValidateThisNode(rudp,
                             routing_table,
                             non_routing_table,
                             NodeId(connect_request.contact().node_id()),
                             key,
                             their_endpoint_pair,
                             our_endpoint_pair,
                             message.client_node());
          };
      node_validation_functor(NodeId(connect_request.contact().node_id()), validate_node);
      connect_response.set_answer(true);
      connect_response.mutable_contact()->set_node_id(routing_table.kKeys().identity);
      SetProtobufEndpoint(our_endpoint_pair.local,
                          connect_response.mutable_contact()->mutable_private_endpoint());
      SetProtobufEndpoint(our_endpoint_pair.external,
                          connect_response.mutable_contact()->mutable_public_endpoint());
    }
  }

  connect_response.set_timestamp(GetTimeStamp());
  connect_response.set_original_request(message.data());
  connect_response.set_original_signature(message.signature());
  message.set_data(connect_response.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_type(-2);
  if (message.has_source_id())
    message.set_destination_id(message.source_id());
  else
    message.clear_destination_id();
  message.set_source_id(routing_table.kKeys().identity);
  assert(message.IsInitialized() && "unintialised message");
}

void FindNodes(RoutingTable &routing_table, protobuf::Message &message) {
  LOG(kVerbose) << "FindNodes -- service()";
  protobuf::FindNodesRequest find_nodes;
  if (!find_nodes.ParseFromString(message.data())) {
    LOG(kWarning) << "Unable to parse find node request";
    message.Clear();
    return;  // no need to reply
  }
  if (0 == find_nodes.num_nodes_requested()) {
    LOG(kWarning) << "Invalid find node request";
    message.Clear();
    return;  // no need to reply
  }
  LOG(kVerbose) << "Parsed find node request -- " << HexSubstr(find_nodes.target_node());
  protobuf::FindNodesResponse found_nodes;
  std::vector<NodeId> nodes(routing_table.GetClosestNodes(NodeId(find_nodes.target_node()),
                              static_cast<uint16_t>(find_nodes.num_nodes_requested() - 1)));

  found_nodes.add_nodes(routing_table.kKeys().identity);

  for (auto node : nodes)
    found_nodes.add_nodes(node.String());

  LOG(kVerbose) << "Responding Find node with " << found_nodes.nodes_size()  << " contacts";

  found_nodes.set_original_request(message.data());
  found_nodes.set_original_signature(message.signature());
  found_nodes.set_timestamp(GetTimeStamp());
  assert(found_nodes.IsInitialized() && "unintialised found_nodes response");
  if (message.has_source_id()) {
    message.set_destination_id(message.source_id());
  } else {
    message.clear_destination_id();
    LOG(kVerbose) << "Relay message, so not setting dst id";
  }
  message.set_source_id(routing_table.kKeys().identity);
  message.set_data(found_nodes.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_type(-3);
  assert(message.IsInitialized() && "unintialised message");
}

void ProxyConnect(RoutingTable &routing_table, rudp::ManagedConnections &/*rudp*/,
                  protobuf::Message &message) {
  if (message.destination_id() != routing_table.kKeys().identity) {
    LOG(kError) << "Message not for us";
    return;  // not for us and we should not pass it on.
  }
  protobuf::ProxyConnectResponse proxy_connect_response;
  protobuf::ProxyConnectRequest proxy_connect_request;

  if (!proxy_connect_request.ParseFromString(message.data())) {
    LOG(kError) << "No Data";
    return;
  }

  // TODO(Prakash): any validation needed?
  rudp::EndpointPair endpoint_pair;
  endpoint_pair.external = GetEndpointFromProtobuf(proxy_connect_request.external_endpoint());
  endpoint_pair.local = GetEndpointFromProtobuf(proxy_connect_request.local_endpoint());

  // TODO(Prakash): Also check NRT and if its my bootstrap endpoint.
  if (routing_table.AmIConnectedToEndpoint(endpoint_pair.external)) {  // If already in RT
    proxy_connect_response.set_result(protobuf::kAlreadyConnected);
  } else {
    bool connect_result(false);
    // TODO(Prakash):  connect_result = rudp.TryConnect(endpoint);
    if (connect_result)
      proxy_connect_response.set_result(protobuf::kSuccess);
    else
      proxy_connect_response.set_result(protobuf::kFailure);
  }
  message.set_type(-4);
  message.set_data(proxy_connect_response.SerializeAsString());
  message.set_destination_id(message.source_id());
  message.set_source_id(routing_table.kKeys().identity);
  assert(message.IsInitialized() && "unintialised message");
}

}  // namespace service

}  // namespace routing

}  // namespace maidsafe
