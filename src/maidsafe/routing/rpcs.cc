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

#include "maidsafe/routing/rpcs.h"

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/utils.h"


namespace maidsafe {

namespace routing {

namespace rpcs {

// This is maybe not required and might be removed
protobuf::Message Ping(const NodeId& node_id, const std::string& identity) {
  assert(node_id.IsValid() && "Invalid node_id");
  assert(!identity.empty() && "Invalid identity");
  protobuf::Message message;
  protobuf::PingRequest ping_request;
  ping_request.set_ping(true);
  ping_request.set_timestamp(GetTimeStamp());
  message.set_destination_id(node_id.String());
  message.set_source_id(identity);
  message.set_routing_message(true);
  message.add_data(ping_request.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kPing));
  message.set_request(true);
  message.set_client_node(false);
  message.set_hops_to_live(Parameters::hops_to_live);
  assert(message.IsInitialized() && "Uninitialised message");
  return message;
}

protobuf::Message Connect(const NodeId& node_id,
                          const rudp::EndpointPair& our_endpoint,
                          const NodeId& my_node_id,
                          const std::vector<std::string>& closest_ids,
                          bool client_node,
                          rudp::NatType nat_type,
                          bool relay_message,
                          NodeId relay_connection_id) {
  assert(node_id.IsValid() && "Invalid node_id");
  assert(my_node_id.IsValid() && "Invalid my node_id");
  assert((!our_endpoint.external.address().is_unspecified() ||
          !our_endpoint.local.address().is_unspecified()) && "Unspecified endpoint");
  protobuf::Message message;
  protobuf::ConnectRequest protobuf_connect_request;
  protobuf::Contact* contact = protobuf_connect_request.mutable_contact();
  SetProtobufEndpoint(our_endpoint.external, contact->mutable_public_endpoint());
  SetProtobufEndpoint(our_endpoint.local, contact->mutable_private_endpoint());
  contact->set_node_id(my_node_id.String());
  contact->set_nat_type(NatTypeProtobuf(nat_type));
  for (auto node_id : closest_ids)
    protobuf_connect_request.add_closest_id(node_id);
  protobuf_connect_request.set_timestamp(GetTimeStamp());
//  message.set_id(RandomUint32());
  message.set_destination_id(node_id.String());
  message.set_routing_message(true);
  message.add_data(protobuf_connect_request.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kConnect));
  message.set_request(true);
  message.set_client_node(client_node);
  message.set_hops_to_live(Parameters::hops_to_live);

  if (!relay_message) {
    message.set_source_id(my_node_id.String());
  } else {
    message.set_relay_id(my_node_id.String());
    // This node is not in any peer's routing table yet
    LOG(kInfo) << "Connect RPC has relay connection id " << DebugId(relay_connection_id);
    message.set_relay_connection_id(relay_connection_id.String());
  }

  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

protobuf::Message FindNodes(const NodeId& node_id,
                            const NodeId& my_node_id,
                            const int& num_nodes_requested,
                            bool relay_message,
                            NodeId relay_connection_id) {
  assert(node_id.IsValid() && "Invalid node_id");
  protobuf::Message message;
  protobuf::FindNodesRequest find_nodes;
  find_nodes.set_num_nodes_requested(num_nodes_requested);
  find_nodes.set_target_node(node_id.String());
  find_nodes.set_timestamp(GetTimeStamp());
//  message.set_id(RandomUint32());
  message.set_last_id(my_node_id.String());
  message.set_destination_id(node_id.String());
  message.set_routing_message(true);
  message.add_data(find_nodes.SerializeAsString());
  message.set_direct(false);
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kFindNodes));
  message.set_request(true);
  message.add_route_history(my_node_id.String());
  message.set_client_node(false);
  if (!relay_message) {
    message.set_source_id(my_node_id.String());
  } else {
    message.set_relay_id(my_node_id.String());
    // This node is not in any peer's routing table yet
    LOG(kInfo) << "FindNodes RPC has relay connection id " << DebugId(relay_connection_id);
    message.set_relay_connection_id(relay_connection_id.String());
  }
  message.set_hops_to_live(Parameters::hops_to_live);
  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

protobuf::Message ProxyConnect(const NodeId& node_id,
                               const NodeId& my_node_id,
                               const rudp::EndpointPair& endpoint_pair,
                               bool relay_message,
                               NodeId relay_connection_id) {
  assert(node_id.IsValid() && "Invalid node_id");
  assert(my_node_id.IsValid() && "Invalid my node_id");
  assert(!endpoint_pair.external.address().is_unspecified() && "Unspecified external endpoint");
  assert(!endpoint_pair.local.address().is_unspecified() && "Unspecified local endpoint");
  protobuf::Message message;
  protobuf::ProxyConnectRequest proxy_connect_request;
  SetProtobufEndpoint(endpoint_pair.local, proxy_connect_request.mutable_local_endpoint());
  SetProtobufEndpoint(endpoint_pair.external, proxy_connect_request.mutable_external_endpoint());
  message.set_destination_id(node_id.String());
  message.set_routing_message(true);
  message.add_data(proxy_connect_request.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kProxyConnect));
  message.set_request(true);
  message.set_client_node(false);
  message.set_hops_to_live(Parameters::hops_to_live);
  if (!relay_message) {
    message.set_source_id(my_node_id.String());
  } else {
    message.set_relay_id(my_node_id.String());
    // This node is not in any peer's routing table yet
    LOG(kInfo) << "ProxyConnect RPC has relay connection id " << DebugId(relay_connection_id);
    message.set_relay_connection_id(relay_connection_id.String());
  }
  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

protobuf::Message ConnectSuccess(const NodeId& node_id,
                                 const NodeId& this_node_seen_connection_id,
                                 const NodeId& my_node_id,
                                 bool client_node) {
  assert(node_id.IsValid() && "Invalid node_id");
  assert(my_node_id.IsValid() && "Invalid my node_id");
  protobuf::Message message;
  protobuf::ConnectSuccess protobuf_connect_success;
  protobuf_connect_success.set_node_id(my_node_id.String());
  protobuf_connect_success.set_connection_id(this_node_seen_connection_id.String());
  message.set_destination_id(node_id.String());
  message.set_routing_message(true);
  message.add_data(protobuf_connect_success.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kConnectSuccess));
  message.set_id(0);
  message.set_client_node(client_node);
  message.set_hops_to_live(Parameters::hops_to_live);
  message.set_source_id(my_node_id.String());
  message.set_request(false);
  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

}  // namespace rpcs

}  // namespace routing

}  // namespace maidsafe
