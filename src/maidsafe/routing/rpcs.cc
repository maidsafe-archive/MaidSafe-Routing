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

namespace detail {

protobuf::Message InitialisedMessage();

template <typename PropertyType>
void SetMessageProperty(protobuf::Message& message, const PropertyType& value);

void SetMessageProperties(protobuf::Message& message);

template <typename PropertyType, typename... Args>
void SetMessageProperties(protobuf::Message& message, PropertyType value, Args... args) {
  SetMessageProperty(message, value);
  SetMessageProperties(message, args...);
}

template <typename PropertyType>
void SetMessageProperty(protobuf::Message& /*message*/, const PropertyType& /*value*/) {
  PropertyType::No_generic_handler_is_available__Specialisation_is_required;
}

template <>
void SetMessageProperty(protobuf::Message& message, const SelfNodeId& value);

template <>
void SetMessageProperty(protobuf::Message& message, const PeerNodeId& value);

template <>
void SetMessageProperty(protobuf::Message& message, const MessageType& value);

template <>
void SetMessageProperty(protobuf::Message& message, const IsDirectMessage& value);

template <>
void SetMessageProperty(protobuf::Message& message, const MessageData& value);

template <>
void SetMessageProperty(protobuf::Message& message, const IsRequestMessage& value);

template <>
void SetMessageProperty(protobuf::Message& message, const RelayId& value);

template <>
void SetMessageProperty(protobuf::Message& message, const RelayConnectionId& value);

template <>
void SetMessageProperty(protobuf::Message& message, const IsClient& value);

template <>
void SetMessageProperty(protobuf::Message& message, const IsRoutingRpCMessage& value);

protobuf::Message InitialisedMessage() {
  protobuf::Message message;
  SetMessageProperties(message, IsDirectMessage(true), IsRequestMessage(true), IsClient(false),
                       IsRoutingRpCMessage(true));
  message.set_hops_to_live(Parameters::hops_to_live);
  message.set_id(RandomUint32() % 10000);
  message.set_replication(1);
  return message;
}

template <>
void SetMessageProperty(protobuf::Message& message, const SelfNodeId& value) {
  assert(!value.data.IsZero());
  message.set_source_id(value.data.string());
}

template <>
void SetMessageProperty(protobuf::Message& message, const PeerNodeId& value) {
  assert(!value.data.IsZero());
  message.set_destination_id(value.data.string());
}

template <>
void SetMessageProperty(protobuf::Message& message, const IsDirectMessage& value) {
  message.set_direct(value.data);
}

template <>
void SetMessageProperty(protobuf::Message& message, const MessageType &value) {
  message.set_type(static_cast<uint32_t>(value));
}

template <>
void SetMessageProperty(protobuf::Message& message, const MessageData& value) {
  assert(!value.data.empty() && "Invalid empty value");
  message.add_data(value.data);
}

template <>
void SetMessageProperty(protobuf::Message& message, const IsRequestMessage& value) {
  message.set_request(value.data);
}

template <>
void SetMessageProperty(protobuf::Message& message, const RelayId& value) {
  assert(!value.data.IsZero() && "invalid data value");
  message.set_relay_id(value.data.string());
}

template <>
void SetMessageProperty(protobuf::Message& message, const RelayConnectionId& value) {
  assert(!value.data.IsZero() && "invalid data value");
  message.set_relay_connection_id(value.data.string());
}

template <>
void SetMessageProperty(protobuf::Message& message, const IsClient& value) {
  message.set_client_node(value.data);
}

template <>
void SetMessageProperty(protobuf::Message& message, const IsRoutingRpCMessage& value) {
  message.set_routing_message(value.data);
}

void SetMessageProperties(protobuf::Message& /*message*/) {
  // assert(message.IsInitialized() && "Uninitialised message");
}

}  // namespace detail

// This is maybe not required and might be removed
protobuf::Message Ping(const PeerNodeId& peer_id, const SelfNodeId& self_id) {
  protobuf::Message message(detail::InitialisedMessage());
  protobuf::PingRequest ping_request;
  ping_request.set_ping(true);
#ifdef TESTING
  ping_request.set_timestamp(GetTimeStamp());
#endif
  detail::SetMessageProperties(message, self_id, peer_id, MessageType::kPing,
                               MessageData(ping_request.SerializeAsString()));
  return message;
}

protobuf::Message Connect(const PeerNodeId& peer_node_id, const SelfEndpoint& our_endpoint,
                          const SelfNodeId& self_node_id,
                          const SelfConnectionId& self_connection_id, IsClient is_client,
                          rudp::NatType nat_type, IsRelayMessage is_relay_message,
                          RelayConnectionId relay_connection_id) {
  assert((!our_endpoint.data.external.address().is_unspecified() ||
          !our_endpoint.data.local.address().is_unspecified()) &&
         "Unspecified endpoint");
  assert(!self_connection_id.data.IsZero() && "Invalid connection id");
  protobuf::Message message(detail::InitialisedMessage());
  protobuf::ConnectRequest protobuf_connect_request;
  {
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
  }
  detail::SetMessageProperties(message, peer_node_id, MessageType::kConnect,
                               MessageData(protobuf_connect_request.SerializeAsString()),
                               is_client);
  if (!is_relay_message.data) {
    detail::SetMessageProperty(message, self_node_id);
  } else {
    // This node is not in any peer's routing table yet
    LOG(kVerbose) << "Connect RPC has relay connection id " << relay_connection_id;
    detail::SetMessageProperties(message, RelayId(self_node_id.data),
                                 relay_connection_id);
  }
  return message;
}

protobuf::Message FindNodes(unsigned int num_nodes_requested, const PeerNodeId& peer_id,
                            const SelfNodeId& self_node_id, IsRelayMessage relay_message,
                            RelayConnectionId relay_connection_id) {
  protobuf::Message message(detail::InitialisedMessage());
  protobuf::FindNodesRequest find_nodes;
  {
    find_nodes.set_num_nodes_requested(num_nodes_requested);
#ifdef TESTING
    find_nodes.set_timestamp(GetTimeStamp());
#endif
  }
  detail::SetMessageProperties(message, peer_id, MessageData(find_nodes.SerializeAsString()),
                               IsDirectMessage(false), MessageType::kFindNodes);
  message.add_route_history(self_node_id.data.string());
  message.set_last_id(self_node_id.data.string());
  message.set_visited(false);
  if (!relay_message.data) {
    detail::SetMessageProperty(message, self_node_id);
  } else {
    // This node is not in any peer's routing table yet
    LOG(kVerbose) << "Connect RPC has relay connection id " << DebugId(relay_connection_id);
    detail::SetMessageProperties(message, RelayId(self_node_id.data),
                                 relay_connection_id);
  }
  assert(message.IsInitialized() && "Uninitialised message");
  return message;
}

protobuf::Message ConnectSuccess(const PeerNodeId& peer_node_id, const SelfNodeId& self_node_id,
                                 const SelfConnectionId& self_connection_id, IsRequestor requestor,
                                 IsClient client_node) {
  assert(!self_connection_id.data.IsZero() && "Invalid connection id");
  protobuf::Message message(detail::InitialisedMessage());
  protobuf::ConnectSuccess protobuf_connect_success;
  {
    protobuf_connect_success.set_node_id(self_node_id.data.string());
    protobuf_connect_success.set_connection_id(self_connection_id.data.string());
    protobuf_connect_success.set_requestor(requestor);
  }
  detail::SetMessageProperties(message, peer_node_id,
                               MessageData(protobuf_connect_success.SerializeAsString()),
                               MessageType::kConnectSuccess, client_node, self_node_id);
  assert(message.IsInitialized() && "Uninitialised message");
  return message;
}

protobuf::Message ConnectSuccessAcknowledgement(
    const PeerNodeId& peer_node_id, const SelfNodeId& self_node_id,
    const SelfConnectionId& self_connection_id, IsRequestor requestor,
    const std::vector<NodeInfo>& close_ids, IsClient client_node) {
  assert(!self_connection_id.data.IsZero() && "Invalid connection id");
  protobuf::Message message(detail::InitialisedMessage());
  protobuf::ConnectSuccessAcknowledgement protobuf_connect_success_ack;
  {
    protobuf_connect_success_ack.set_node_id(self_node_id.data.string());
    protobuf_connect_success_ack.set_connection_id(self_connection_id.data.string());
    protobuf_connect_success_ack.set_requestor(requestor);
    for (const auto& i : close_ids)
      protobuf_connect_success_ack.add_close_ids(i.id.string());
  }
  detail::SetMessageProperties(message, peer_node_id,
                               MessageData(protobuf_connect_success_ack.SerializeAsString()),
                               MessageType::kConnectSuccessAcknowledgement, client_node,
                               self_node_id, IsRequestMessage(false));
  assert(message.IsInitialized() && "Uninitialised message");
  return message;
}

protobuf::Message InformClientOfNewCloseNode(const PeerNodeId& added_peer_node_id,
                                             const SelfNodeId& self_node_id,
                                             const PeerNodeId& client_node_id) {
  protobuf::Message message(detail::InitialisedMessage());
  message.set_hops_to_live(2);
  protobuf::InformClientOfhNewCloseNode inform_client_of_new_close_node;
  {
    inform_client_of_new_close_node.set_node_id(added_peer_node_id.data.string());
  }
  detail::SetMessageProperties(message, client_node_id, self_node_id,
                               MessageData(inform_client_of_new_close_node.SerializeAsString()),
                               MessageType::kInformClientOfNewCloseNode);
  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

protobuf::Message GetGroup(const PeerNodeId& peer_node_id, const SelfNodeId& self_node_id) {
  protobuf::Message message(detail::InitialisedMessage());
  message.set_visited(false);
  protobuf::GetGroup get_group;
  {
    get_group.set_node_id(peer_node_id.data.string());
  }
  detail::SetMessageProperties(message,  MessageData(get_group.SerializeAsString()), peer_node_id,
                               self_node_id, IsDirectMessage(false), MessageType::kGetGroup);
  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

}  // namespace rpcs

}  // namespace routing

}  // namespace maidsafe
