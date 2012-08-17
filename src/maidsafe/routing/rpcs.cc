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
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/node_id.h"
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
  message.add_data(ping_request.SerializeAsString());
  message.set_direct(static_cast<int32_t>(ConnectType::kSingle));
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kPingRequest));
  message.set_id(0);
  message.set_client_node(false);
  assert(message.IsInitialized() && "Uninitialised message");
  return message;
}

protobuf::Message Connect(const NodeId& node_id,
                          const rudp::EndpointPair& our_endpoint,
                          const NodeId& my_node_id,
                          const std::vector<std::string>& closest_ids,
                          bool client_node,
                          bool relay_message,
                          boost::asio::ip::udp::endpoint local_endpoint) {
  assert(node_id.IsValid() && "Invalid node_id");
  assert(my_node_id.IsValid() && "Invalid my node_id");
//  BOOST_ASSERT_MSG(!our_endpoint.external.address().is_unspecified(), "Unspecified endpoint");
//  BOOST_ASSERT_MSG(!our_endpoint.local.address().is_unspecified(), "Unspecified endpoint");
  protobuf::Message message;
  protobuf::ConnectRequest protobuf_connect_request;
  protobuf::Contact* contact = protobuf_connect_request.mutable_contact();
  protobuf::Endpoint* public_endpoint = contact->mutable_public_endpoint();
  public_endpoint->set_ip(our_endpoint.external.address().to_string());
  public_endpoint->set_port(our_endpoint.external.port());
  protobuf::Endpoint* private_endpoint = contact->mutable_private_endpoint();
  private_endpoint->set_ip(our_endpoint.local.address().to_string());
  private_endpoint->set_port(our_endpoint.local.port());
  contact->set_node_id(my_node_id.String());
  for (auto node_id : closest_ids)
    protobuf_connect_request.add_closest_id(node_id);
  protobuf_connect_request.set_timestamp(GetTimeStamp());
  message.set_destination_id(node_id.String());
  message.add_data(protobuf_connect_request.SerializeAsString());
  message.set_direct(static_cast<int32_t>(ConnectType::kSingle));
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kConnectRequest));
  message.set_id(0);
  message.set_id(RandomUint32() % 10000);
  message.set_client_node(client_node);

  if (!relay_message) {
    message.set_source_id(my_node_id.String());
  } else {
    message.set_relay_id(my_node_id.String());
    if (!local_endpoint.address().is_unspecified()) {
      // This node is not in any peer's routing table yet
      LOG(kInfo) << "Connect RPC has relay IP " << local_endpoint;
      SetProtobufEndpoint(local_endpoint, message.mutable_relay());
    }
  }

  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

protobuf::Message FindNodes(const NodeId& node_id,
                            const NodeId& my_node_id,
                            bool relay_message,
                            boost::asio::ip::udp::endpoint local_endpoint) {
  assert(node_id.IsValid() && "Invalid node_id");
  protobuf::Message message;
  protobuf::FindNodesRequest find_nodes;
  find_nodes.set_num_nodes_requested(Parameters::closest_nodes_size);
  find_nodes.set_target_node(node_id.String());
  find_nodes.set_timestamp(GetTimeStamp());
  message.set_last_id(my_node_id.String());
  message.set_destination_id(node_id.String());
  message.add_data(find_nodes.SerializeAsString());
  message.set_direct(static_cast<int32_t>(ConnectType::kGroup));
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kFindNodesRequest));
  message.set_id(0);
  message.set_id(RandomUint32() % 10000);
  message.add_route_history(my_node_id.String());
  message.set_client_node(false);
  if (!relay_message) {
    message.set_source_id(my_node_id.String());
  } else {
    message.set_relay_id(my_node_id.String());
    if (!local_endpoint.address().is_unspecified()) {
      // This node is not in any peer's routing table yet
      LOG(kInfo) << "FindNodes RPC has relay IP " << local_endpoint;
      SetProtobufEndpoint(local_endpoint, message.mutable_relay());
    }
  }
  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

protobuf::Message ProxyConnect(const NodeId& node_id,
                               const NodeId& my_node_id,
                               const rudp::EndpointPair& endpoint_pair,
                               bool relay_message,
                               boost::asio::ip::udp::endpoint local_endpoint) {
  assert(node_id.IsValid() && "Invalid node_id");
  assert(my_node_id.IsValid() && "Invalid my node_id");
  assert(!endpoint_pair.external.address().is_unspecified() && "Unspecified external endpoint");
  assert(!endpoint_pair.local.address().is_unspecified() && "Unspecified local endpoint");
  protobuf::Message message;
  protobuf::ProxyConnectRequest proxy_connect_request;
  SetProtobufEndpoint(endpoint_pair.local, proxy_connect_request.mutable_local_endpoint());
  SetProtobufEndpoint(endpoint_pair.external, proxy_connect_request.mutable_external_endpoint());
  message.set_destination_id(node_id.String());
  message.add_data(proxy_connect_request.SerializeAsString());
  message.set_direct(static_cast<int32_t>(ConnectType::kSingle));
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kProxyConnectRequest));
  message.set_id(0);
  message.set_client_node(false);
  if (!relay_message) {
    message.set_source_id(my_node_id.String());
  } else {
    message.set_relay_id(my_node_id.String());
    if (!local_endpoint.address().is_unspecified()) {
      // This node is not in any peer's routing table yet
      LOG(kInfo) << "ProxyConnect RPC has relay IP " << local_endpoint;
      SetProtobufEndpoint(local_endpoint, message.mutable_relay());
    }
  }
  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

}  // namespace rpcs

}  // namespace routing

}  // namespace maidsafe
