/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#include "maidsafe/routing/rpcs.h"

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/message_handler.h"


namespace maidsafe {

namespace routing {

namespace rpcs {

// This is maybe not required and might be removed
protobuf::Message Ping(const PeerNodeId& peer_id, const SelfNodeId& self_id) {
  assert(!peer_id.data.IsZero() && "Invalid node_id");
  assert(self_id.data.IsZero() && "Invalid identity");
  protobuf::Message message;
  protobuf::PingRequest ping_request;
  ping_request.set_ping(true);
#ifdef TESTING
  ping_request.set_timestamp(GetTimeStamp());
#endif
  SetMessageProperties(message, self_id, peer_id);
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

protobuf::Message Connect(const PeerNodeId& peer_node_id, const SelfEndpoint& our_endpoint,
                          const SelfNodeId& self_node_id,
                          const SelfConnectionId& self_connection_id, IsClient is_client,
                          NatType nat_type, IsRelayMessage is_relay_message,
                          RelayConnectionId relay_connection_id) {
  assert(!peer_node_id.data.IsZero() && "Invalid node_id");
  assert(!self_node_id.data.IsZero() && "Invalid my node_id");
  assert(!self_connection_id.data.IsZero() && "Invalid this_connection_id");
  assert((!our_endpoint.data.external.address().is_unspecified() ||
          !our_endpoint.data.local.address().is_unspecified()) &&
         "Unspecified endpoint");
  protobuf::Message message;
  protobuf::ConnectRequest protobuf_connect_request;
  protobuf_connect_request.set_peer_id(peer_node_id.data.string());
  protobuf::Contact* contact = protobuf_connect_request.mutable_contact();
  SetProtobufEndpoint(our_endpoint.data.external, contact->mutable_public_endpoint());
  SetProtobufEndpoint(our_endpoint.data.local, contact->mutable_private_endpoint());
  contact->set_connection_id(self_connection_id.data.string());
  contact->set_node_id(self_node_id.data.string());
  contact->set_nat_type(NatTypeProtobuf(nat_type));
#ifdef TESTING
  protobuf_connect_request.set_timestamp(GetTimeStamp());
#endif
  message.set_id(RandomUint32() % 10000);
  message.set_destination_id(peer_node_id.data.string());
  message.set_routing_message(true);
  message.add_data(protobuf_connect_request.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kConnect));
  message.set_request(true);
  message.set_client_node(is_client);
  message.set_hops_to_live(Parameters::hops_to_live);

  if (!is_relay_message) {
    message.set_source_id(self_node_id.data.string());
  } else {
    message.set_relay_id(self_node_id.data.string());
    // This node is not in any peer's routing table yet
    LOG(kVerbose) << "Connect RPC has relay connection id " << DebugId(relay_connection_id);
    message.set_relay_connection_id(relay_connection_id.data.string());
  }

  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

protobuf::Message FindNodes(unsigned int num_nodes_requested, const PeerNodeId& peer_id,
                            const SelfNodeId& self_id, IsRelayMessage relay_message,
                            RelayConnectionId relay_connection_id) {
  assert(!peer_id.data.IsZero() && "Invalid node_id");
  assert(!self_id.data.IsZero() && "Invalid my node_id");
  protobuf::Message message;
  protobuf::FindNodesRequest find_nodes;
  find_nodes.set_num_nodes_requested(num_nodes_requested);
#ifdef TESTING
  find_nodes.set_timestamp(GetTimeStamp());
#endif
  message.set_last_id(self_id.data.string());
  SetMessageProperties(message, peer_id);
  message.set_routing_message(true);
  message.add_data(find_nodes.SerializeAsString());
  message.set_direct(false);
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kFindNodes));
  message.set_request(true);
  message.add_route_history(self_id.data.string());
  message.set_client_node(false);
  message.set_visited(false);
  message.set_id(RandomUint32() % 10000);
  if (!relay_message) {
    message.set_source_id(self_id.data.string());
  } else {
    message.set_relay_id(self_id.data.string());
    // This node is not in any peer's routing table yet
    LOG(kVerbose) << "FindNodes RPC has relay connection id " << relay_connection_id.data;
    message.set_relay_connection_id(relay_connection_id.data.string());
  }
  message.set_hops_to_live(Parameters::hops_to_live);
  //  message.set_id(RandomUint32() % 10000);
  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

protobuf::Message ConnectSuccess(const PeerNodeId& peer_node_id, const SelfNodeId& self_node_id,
                                 const SelfConnectionId& self_connection_id, IsRequestor requestor,
                                 IsClient client_node) {
  assert(!peer_node_id.data.IsZero() && "Invalid node_id");
  assert(!self_node_id.data.IsZero() && "Invalid my node_id");
  assert(!self_connection_id.data.IsZero() && "Invalid this_connection_id");
  protobuf::Message message;
  protobuf::ConnectSuccess protobuf_connect_success;
  protobuf_connect_success.set_node_id(self_node_id.data.string());
  protobuf_connect_success.set_connection_id(self_connection_id.data.string());
  protobuf_connect_success.set_requestor(requestor);
  message.set_destination_id(peer_node_id.data.string());
  message.set_routing_message(true);
  message.add_data(protobuf_connect_success.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kConnectSuccess));
  message.set_client_node(client_node);
  message.set_hops_to_live(Parameters::hops_to_live);
  message.set_source_id(self_node_id.data.string());
  message.set_request(true);
  message.set_id(RandomUint32() % 10000);
  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

protobuf::Message ConnectSuccessAcknowledgement(
    const PeerNodeId& peer_node_id, const SelfNodeId& self_node_id,
    const SelfConnectionId& self_connection_id, IsRequestor requestor,
    const std::vector<NodeInfo>& close_ids, IsClient client_node) {
  assert(!peer_node_id.data.IsZero() && "Invalid node_id");
  assert(!self_node_id.data.IsZero() && "Invalid my node_id");
  assert(!self_connection_id.data.IsZero() && "Invalid this_connection_id");
  protobuf::Message message;
  protobuf::ConnectSuccessAcknowledgement protobuf_connect_success_ack;
  protobuf_connect_success_ack.set_node_id(self_node_id.data.string());
  protobuf_connect_success_ack.set_connection_id(self_connection_id.data.string());
  protobuf_connect_success_ack.set_requestor(requestor);
  for (const auto& i : close_ids)
    protobuf_connect_success_ack.add_close_ids(i.id.string());
  message.set_destination_id(peer_node_id.data.string());
  message.set_routing_message(true);
  message.add_data(protobuf_connect_success_ack.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kConnectSuccessAcknowledgement));
  message.set_client_node(client_node);
  message.set_hops_to_live(Parameters::hops_to_live);
  message.set_source_id(self_node_id.data.string());
  message.set_request(false);
  message.set_id(RandomUint32() % 10000);
  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

protobuf::Message InformClientOfNewCloseNode(const PeerNodeId& peer_node_id,
                                             const SelfNodeId& self_node_id,
                                             const NodeId& client_node_id) {
  assert(!peer_node_id.data.IsZero() && "Invalid node_id");
  assert(!self_node_id.data.IsZero() && "Invalid my node_id");
  protobuf::Message message;
  protobuf::InformClientOfhNewCloseNode inform_client_of_new_close_node;
  inform_client_of_new_close_node.set_node_id(peer_node_id.data.string());
  message.add_data(inform_client_of_new_close_node.SerializeAsString());
  message.set_destination_id(client_node_id.string());
  message.set_source_id(self_node_id.data.string());
  message.set_routing_message(true);
  message.set_direct(false);
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kInformClientOfNewCloseNode));
  message.set_request(true);
  message.set_client_node(false);
  message.set_hops_to_live(2);
  message.set_visited(false);
  message.set_id(RandomUint32() % 10000);
  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

protobuf::Message GetGroup(const NodeId& node_id, const NodeId& my_node_id) {
  assert(!node_id.IsZero() && "Invalid node_id");
  assert(!my_node_id.IsZero() && "Invalid my node_id");
  protobuf::Message message;
  protobuf::GetGroup get_group;
  get_group.set_node_id(node_id.string());
  message.add_data(get_group.SerializeAsString());
  message.set_destination_id(node_id.string());
  message.set_source_id(my_node_id.string());
  message.set_routing_message(true);
  message.set_direct(false);
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kGetGroup));
  message.set_request(true);
  message.set_client_node(false);
  message.set_hops_to_live(Parameters::hops_to_live);
  message.set_visited(false);
  message.set_id(RandomUint32() % 10000);
  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

template <>
void SetMessageProperty(protobuf::Message& message, const SelfNodeId& value) {
  message.set_source_id(value.data.string());
}

template <>
void SetMessageProperty(protobuf::Message& message, const PeerNodeId& value) {
  message.set_destination_id(value.data.string());
}

void SetMessageProperties(protobuf::Message& /*message*/) {}

}  // namespace rpcs

}  // namespace routing

}  // namespace maidsafe
