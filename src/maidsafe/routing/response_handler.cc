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
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/response_handler.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/utils.h"
#include "maidsafe/routing/log.h"


namespace maidsafe {

namespace routing {

namespace response {

// always direct !! never pass on
void Ping(protobuf::Message& message) {
  // TODO , do we need this and where and how can I update the response
  protobuf::PingResponse ping_response;
  if (ping_response.ParseFromString(message.data())) {
    //  do stuff here
    }
}

// the other node agreed to connect - he has accepted our connection
void Connect(protobuf::Message& message,
             NodeValidationFunctor node_validation_functor) {
  protobuf::ConnectResponse connect_response;
  protobuf::ConnectRequest connect_request;
  if (!connect_response.ParseFromString(message.data())) {
    DLOG(ERROR) << "Could not parse connect response";
    return;
  }
  if (!connect_response.answer()) {
    return;  // they don't want us
  }
  if (!connect_request.ParseFromString(connect_response.original_request()))
    return;  // invalid response

  boost::asio::ip::udp::endpoint our_endpoint;
  our_endpoint.address().from_string(connect_request.contact().endpoint().ip());
  our_endpoint.port(connect_request.contact().endpoint().port());

  boost::asio::ip::udp::endpoint their_endpoint;
  their_endpoint.address().from_string(connect_response.contact().endpoint().ip());
  their_endpoint.port(connect_response.contact().endpoint().port());
  // TODO(dirvine) FIXME
  if (node_validation_functor)  // never add any node to routing table
    node_validation_functor(connect_response.contact().node_id(),
                            their_endpoint,
                            message.client_node(),
                            our_endpoint);
}

void FindNode(RoutingTable &routing_table,
              rudp::ManagedConnections &rudp,
              const protobuf::Message& message) {
  protobuf::FindNodesResponse find_nodes;
  if (!find_nodes.ParseFromString(message.data())) {
    DLOG(ERROR) << "Could not parse find node response";
    return;
  }
  if (asymm::CheckSignature(find_nodes.original_request(),
                            find_nodes.original_signature(),
                            routing_table.kKeys().public_key) != kSuccess) {
    DLOG(ERROR) << " find node request was not signed by us";
    return;  // we never requested this
  }
  for(int i = 0; i < find_nodes.nodes_size() ; ++i) {
    NodeInfo node_to_add;
    node_to_add.node_id = NodeId(find_nodes.nodes(i));
    if (routing_table.CheckNode(node_to_add)) {
      DLOG(INFO) << " size of find nodes " << find_nodes.nodes_size();
      boost::asio::ip::udp::endpoint endpoint;
      rudp.GetAvailableEndpoint(&endpoint);
      SendOn(rpcs::Connect(NodeId(find_nodes.nodes(i)),
                           endpoint,
                           routing_table.kKeys().identity),
             rudp,
             routing_table);
    }
  }
}

}  // namespace response 

}  // namespace routing

}  // namespace maidsafe