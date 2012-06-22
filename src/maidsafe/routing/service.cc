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
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

namespace service {

void Ping(RoutingTable &routing_table, protobuf::Message &message) {
//   if (message.destination_id() != NodeId::kKeySizeBytes) {
//         LOG(kError) << "Invalid destination ID";
//     return;
//   }
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
             rudp::ManagedConnections &rudp,
             protobuf::Message &message,
             NodeValidationFunctor node_validation_functor,
             std::shared_ptr<AsioService> asio_service) {
  if (message.destination_id() != routing_table.kKeys().identity) {
    LOG(kVerbose) << "Connect -- not for us and we should not pass it on.";
    return;  // not for us and we should not pass it on.
  }
  protobuf::ConnectRequest connect_request;
  protobuf::ConnectResponse connect_response;
  if (!connect_request.ParseFromString(message.data())) {
    LOG(kVerbose) << "Unable to parse connect request";
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
  Endpoint their_public_endpoint;
  Endpoint their_private_endpoint;
  their_public_endpoint.address(
      boost::asio::ip::address::from_string(connect_request.contact().public_endpoint().ip()));
  their_public_endpoint.port(
      static_cast<unsigned short>(connect_request.contact().public_endpoint().port()));
  their_private_endpoint.address(
      boost::asio::ip::address::from_string(connect_request.contact().private_endpoint().ip()));
  their_private_endpoint.port(
      static_cast<unsigned short>(connect_request.contact().private_endpoint().port()));
  their_endpoint_pair.external = their_public_endpoint;
  their_endpoint_pair.local = their_private_endpoint;
  LOG(kVerbose) << "Calling GetAvailableEndpoint with peer ep - " << their_public_endpoint;
  if((rudp.GetAvailableEndpoint(their_public_endpoint, our_endpoint_pair)) != 0) {
    LOG(kVerbose) << "Unable to get available endpoint to connect to" << their_public_endpoint;
    return;
  }
  LOG(kWarning) << " GetAvailableEndpoint for peer - " << their_public_endpoint << " my endpoint - " << our_endpoint_pair.external;
  // TODO(dirvine) try both connections
  if (message.client_node()) {
    connect_response.set_answer(true);
    // TODO(dirvine): get the routing pointer back again

    //if (node_validation_functor)  // never add any node to routing table
    //node_validation_functor(NodeId(connect_response.contact().node_id()),
    //                        their_endpoint_pair,
    //                        our_endpoint_pair,
    //                        message.client_node());
  }
  if ((routing_table.CheckNode(node)) && (!message.client_node())) {
    connect_response.set_answer(true);
    LOG(kVerbose) << "CheckNode(node) successfull!";

    asio_service->service().post(std::bind(node_validation_functor,
                                           NodeId(connect_request.contact().node_id()),
                                           their_endpoint_pair,
                                           our_endpoint_pair,
                                           message.client_node()));
  }

  protobuf::Contact *contact;
  protobuf::Endpoint *private_endpoint;
  protobuf::Endpoint *public_endpoint;
  contact = connect_response.mutable_contact();
  private_endpoint = contact->mutable_private_endpoint();
  private_endpoint->set_ip(our_endpoint_pair.local.address().to_string());
  private_endpoint->set_port(our_endpoint_pair.local.port());
  public_endpoint = contact->mutable_public_endpoint();
  public_endpoint->set_ip(our_endpoint_pair.local.address().to_string());
  public_endpoint->set_port(our_endpoint_pair.local.port());
  contact->set_node_id(routing_table.kKeys().identity);
  connect_response.set_timestamp(GetTimeStamp());
  connect_response.set_original_request(message.data());
  connect_response.set_original_signature(message.signature());
  message.set_destination_id(message.source_id());
  message.set_source_id(routing_table.kKeys().identity);
  message.set_data(connect_response.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_type(-2);
  if (!message.has_relay()) {
    message.set_relay_id(connect_request.contact().node_id());
  } else {
    protobuf::Endpoint *pbendpoint;
    pbendpoint = message.mutable_relay();
    pbendpoint->set_ip(their_public_endpoint.address().to_string().c_str());
    pbendpoint->set_port(their_public_endpoint.port());
  }
  assert(message.IsInitialized() && "unintialised message");
}

void FindNodes(RoutingTable &routing_table, protobuf::Message &message) {
  protobuf::FindNodesRequest find_nodes;
  protobuf::FindNodesResponse found_nodes;
  std::vector<NodeId>
      nodes(routing_table.GetClosestNodes(NodeId(message.destination_id()),
            static_cast<uint16_t>(find_nodes.num_nodes_requested())));
  for (auto it = nodes.begin(); it != nodes.end(); ++it)
    found_nodes.add_nodes((*it).String());
  if (routing_table.Size() < Parameters::closest_nodes_size)
    found_nodes.add_nodes(routing_table.kKeys().identity);  // small network send our ID
  found_nodes.set_original_request(message.data());
  found_nodes.set_original_signature(message.signature());
  found_nodes.set_timestamp(GetTimeStamp());
  assert(found_nodes.IsInitialized() && "unintialised found_nodes response");
  message.set_destination_id(message.source_id());
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

  Endpoint endpoint(boost::asio::ip::address::from_string(proxy_connect_request.endpoint().ip()),
                    static_cast<uint16_t> (proxy_connect_request.endpoint().port()));
  if (routing_table.AmIConnectedToEndpoint(endpoint)) {  // If endpoint already in routing table
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
