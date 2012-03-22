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

#include "maidsafe/transport/managed_connection.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/rpcs.h"


namespace maidsafe {

namespace routing {

Rpcs::Rpcs(std::shared_ptr< RoutingTable > routing_table,
           std::shared_ptr< transport::ManagedConnection > transport) :
           routing_table_(routing_table),
           transport_(transport) { }

void Rpcs::SendOn(protobuf::Message& message) {
  NodeInfo next_node(routing_table_->
                     GetClosestNode(NodeId(message.destination_id()), 0));
// FIXME SEND transport_->Send(next_node.endpoint, message.SerializeAsString());
}


void Rpcs::Ping(protobuf::Message &message) {
  protobuf::PingRequest ping_request;
  ping_request.set_ping(true);
  message.set_destination_id(message.source_id());
  message.set_source_id(routing_table_->kNodeId().String());
  message.set_data(ping_request.SerializeAsString());
  message.set_direct(true);
  message.set_response(true);
  message.set_replication(1);
  message.set_type(0);
  SendOn(message);
}

void Rpcs::Connect(protobuf::Message &message) {
    // create a connect message to send direct.
  protobuf::ConnectRequest protobuf_connect_request;
  protobuf::Endpoint protobuf_endpoint;
  maidsafe::transport::Endpoint peer_endpoint;
  peer_endpoint.ip.from_string(protobuf_endpoint.ip());
  peer_endpoint.port = protobuf_endpoint.port();

}

void Rpcs::FindNodes(protobuf::Message &message) {
  protobuf::FindNodesRequest find_nodes;
  protobuf::FindNodesResponse found_nodes;
  std::vector<NodeId>
        nodes(routing_table_->GetClosestNodes(NodeId(message.destination_id()),
                 static_cast<uint16_t>(find_nodes.num_nodes_requested())));

  for (auto it = nodes.begin(); it != nodes.end(); ++it)
    found_nodes.add_nodes((*it).String());
  message.set_destination_id(message.source_id());
  message.set_source_id(routing_table_->kNodeId().String());
  message.set_data(found_nodes.SerializeAsString());
  message.set_direct(true);
  message.set_response(true);
  message.set_replication(1);
  message.set_type(1);
  SendOn(message);
}


}  // namespace routing

}  // namespace maidsafe
