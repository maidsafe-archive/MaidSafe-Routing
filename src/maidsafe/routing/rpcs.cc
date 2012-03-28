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

#include "maidsafe/common/utils.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

namespace rpcs {

// this is maybe not required and might be removed
const protobuf::Message Ping(const NodeId &node_id, std::string &,
                       const std::string &identity) {
  protobuf::Message message;
  protobuf::PingRequest ping_request;
  ping_request.set_ping(true);
  ping_request.set_timestamp(GetTimeStamp());
  message.set_destination_id(node_id.String());
  message.set_source_id(identity);
  message.set_data(ping_request.SerializeAsString());
  message.set_direct(true);
  message.set_response(false);
  message.set_replication(1);
  message.set_type(0);
  BOOST_ASSERT_MSG(message.IsInitialized(), "unintialised message");
  return message;
}

const protobuf::Message Connect(const NodeId &node_id,
                   const transport::Endpoint &our_endpoint,
                   const std::string &identity) {
  protobuf::Message message;
  protobuf::Contact *contact;
  protobuf::Endpoint *endpoint;
  protobuf::ConnectRequest protobuf_connect_request;
  contact = protobuf_connect_request.mutable_contact();
  endpoint = contact->mutable_endpoint();
  endpoint->set_ip(our_endpoint.ip.to_string());
  endpoint->set_port(our_endpoint.port);
  contact->set_node_id(identity);
  protobuf_connect_request.set_timestamp(GetTimeStamp());
  message.set_destination_id(node_id.String());
  message.set_source_id(identity);
  message.set_data(protobuf_connect_request.SerializeAsString());
  message.set_direct(true);
  message.set_response(false);
  message.set_replication(1);
  message.set_type(1);
  BOOST_ASSERT_MSG(message.IsInitialized(), "unintialised message");
  return message;
}

const protobuf::Message FindNodes(const NodeId &node_id) {
  protobuf::Message message;
  protobuf::FindNodesRequest find_nodes;
  find_nodes.set_num_nodes_requested(Parameters::closest_nodes_size);
  find_nodes.set_target_node(node_id.String());
  find_nodes.set_timestamp(GetTimeStamp());
  message.set_destination_id(node_id.String());
  message.set_source_id(node_id.String());
  message.set_data(find_nodes.SerializeAsString());
  message.set_direct(true);
  message.set_response(false);
  message.set_replication(1);
  message.set_type(2);
  BOOST_ASSERT_MSG(message.IsInitialized(), "unintialised message");
  return message;
}

} // namespace rpcs

}  // namespace routing

}  // namespace maidsafe
