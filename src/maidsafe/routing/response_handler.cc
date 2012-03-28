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

#include "boost/thread/shared_mutex.hpp"
#include "boost/thread/mutex.hpp"
#include "maidsafe/common/rsa.h"
#include "maidsafe/transport/managed_connections.h"
#include "maidsafe/routing/response_handler.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/utils.h"
#include "maidsafe/routing/log.h"


namespace maidsafe {

namespace routing {

ResponseHandler::ResponseHandler(const NodeValidationFunctor& node_Validation_functor,
                RoutingTable &routing_table,
                transport::ManagedConnections &transport) :
                node_validation_functor_(node_Validation_functor),
                routing_table_(routing_table),
                transport_(transport) {}

// always direct !! never pass on
void ResponseHandler::ProcessPingResponse(protobuf::Message& message) {
  // TODO , do we need this and where and how can I update the response
  protobuf::PingResponse ping_response;
  if (ping_response.ParseFromString(message.data())) {
    //  do stuff here
    }
}

// the other node agreed to connect - he has accepted our connection
void ResponseHandler::ProcessConnectResponse(protobuf::Message& message) {
  protobuf::ConnectResponse connect_response;
  if (!connect_response.ParseFromString(message.data())) {

    DLOG(ERROR) << "Could not parse connect response";
    return;
  }
  if (!connect_response.answer()) {
    return;  // they don't want us
  }
  transport::Endpoint endpoint;
  endpoint.ip.from_string(connect_response.contact().endpoint().ip());
  endpoint.port = connect_response.contact().endpoint().port();
  node_validation_functor_(connect_response.contact().node_id(),
                           endpoint, message.client_node());
}

void ResponseHandler::ProcessFindNodeResponse(protobuf::Message& message) {
  protobuf::FindNodesResponse find_nodes;
  if (!find_nodes.ParseFromString(message.data())) {
    DLOG(ERROR) << "Could not parse find node response";
    return;
  }
  if (asymm::CheckSignature(find_nodes.original_request(),
                            find_nodes.original_signature(),
                            routing_table_.kKeys().public_key) != kSuccess) {
    DLOG(ERROR) << " find node request was not signed by us";
    return;  // we never requested this
  }
  for(int i = 0; i < find_nodes.nodes_size() ; ++i) {
    NodeInfo node_to_add;
    node_to_add.node_id = NodeId(find_nodes.nodes(i));
    if (routing_table_.CheckNode(node_to_add)) {
      SendOn(rpcs::Connect(NodeId(find_nodes.nodes(i)),
                               transport_.GetAvailableEndpoint(),
                               routing_table_.kKeys().identity),
             transport_,
             routing_table_);
    }
  }
}


}  // namespace routing

}  // namespace maidsafe