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
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/log.h"

namespace maidsafe {

namespace routing {

Service::Service(std::shared_ptr< Rpcs > rpc_ptr,
                 std::shared_ptr< RoutingTable > routing_table) :
                 rpc_ptr_(rpc_ptr),
                 routing_table_(routing_table) {}


void Service::Ping(protobuf::Message &message) {
  protobuf::PingResponse ping_response;
  protobuf::PingRequest ping_request;

  if (!ping_request.ParseFromString(message.data()))
    return;
  ping_response.set_pong(true);
  message.set_data(ping_response.SerializeAsString());
  rpc_ptr_->SendOn(message);
}

void Service::Connect(protobuf::Message &message) {
// send message back  wait on his connect
// add him to a pending endpoint queue
// and when transport asks us to accept him we will
  if (message.has_source_id())
    DLOG(INFO) << " have source ID";
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
