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
  // OK we will try to connect to all the endpoints supplied
//   connect_request.contact()

  if (connect_request.bootstrap()) {
    return;  //  will be cleared by by Routing after timeout
  }
//   if (connect_request.client())
//
//   if (routing_table_->CheckNode(NodeId(connect_request.contact().node_id())))
//
}

void Service::FindNodes(protobuf::Message &message) {
  protobuf::FindNodesResponse find_nodes;
  if (!find_nodes.ParseFromString(message.data()))
    return;
  for (int i = 0; i < find_nodes.nodes().size(); ++i) {
    NodeInfo node;
    node.node_id = NodeId(find_nodes.nodes(i));
    routing_table_->CheckNode(node);
  }
}

}  // namespace routing

}  // namespace maidsafe
