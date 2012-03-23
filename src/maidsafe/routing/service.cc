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

#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/log.h"

namespace maidsafe {

namespace routing {

Service::Service(std::shared_ptr<RoutingTable> routing_table)
    : routing_table_(routing_table) {}

void Service::Ping(protobuf::Message &message) {
  protobuf::PingResponse ping_response;
  protobuf::PingRequest ping_request;

  if (!ping_request.ParseFromString(message.data()))
    return;
  ping_response.set_pong(true);
  message.set_data(ping_response.SerializeAsString());
  routing_table_->SendOn(message);
}

void Service::Connect(protobuf::Message &message) {
  protobuf::ConnectRequest connect_request;
  if (!connect_request.ParseFromString(message.data()))
    return;  // no need to reply
  if (connect_request.bootstrap()) {
             // connect here !!
     //  will be cleared by by Routing after timeout
  }
  if (connect_request.client()) {
                 // connect here !!
                 // add to client bucket
     //  will be cleared by by Routing after timeout
  }
  NodeInfo node;
  node.node_id = NodeId(connect_request.contact().node_id());
  if (routing_table_->CheckNode(node))
    return; // no need to reply
  // OK we will try to connect to all the endpoints supplied
 // for (int i = connect_request.contact()
  //transport_->AddConnection(transport::Endpoint(connect_request.contact().endpoint().ip(), connect_request.contact().endpoint().port()));

}

void Service::FindNodes(protobuf::Message &message) {
  protobuf::FindNodesRequest find_nodes;
  protobuf::FindNodesResponse found_nodes;
  std::vector<NodeId>
        nodes(routing_table_->GetClosestNodes(NodeId(message.destination_id()),
                 static_cast<uint16_t>(find_nodes.num_nodes_requested())));

  for (auto it = nodes.begin(); it != nodes.end(); ++it)
    found_nodes.add_nodes((*it).String());
  if (routing_table_->Size() < routing_table_->ClosestNodesSize())
    found_nodes.add_nodes(routing_table_->kNodeId().String()); // small network send our ID
  message.set_destination_id(message.source_id());
  message.set_source_id(routing_table_->kNodeId().String());
  message.set_data(found_nodes.SerializeAsString());
  message.set_direct(true);
  message.set_response(true);
  message.set_replication(0);
  message.set_type(1);
  routing_table_->SendOn(message);
}

}  // namespace routing

}  // namespace maidsafe
