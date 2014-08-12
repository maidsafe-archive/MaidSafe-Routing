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

#ifndef MAIDSAFE_ROUTING_RPCS_H_
#define MAIDSAFE_ROUTING_RPCS_H_

#include <string>
#include <vector>

#include "boost/asio/ip/udp.hpp"

#include "maidsafe/common/node_id.h"

#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/types.h"

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
void SetMessageProperty(protobuf::Message& message, const LocalNodeId& value);

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
void SetMessageProperty(protobuf::Message& message, const RelayNodeId& value);

template <>
void SetMessageProperty(protobuf::Message& message, const RelayConnectionId& value);

template <>
void SetMessageProperty(protobuf::Message& message, const IsClient& value);

template <>
void SetMessageProperty(protobuf::Message& message, const IsRoutingRpCMessage& value);

template <>
void SetMessageProperty(protobuf::Message& message, const LastNodeId& value);

template <>
void SetMessageProperty(protobuf::Message& message, const MessageId& value);

template <>
void SetMessageProperty(protobuf::Message& message, const Cacheable& value);

template <>
void SetMessageProperty(protobuf::Message& message, const IsRequestor& value);

}  // namespace detail


protobuf::Message Ping(const PeerNodeId& peer_id, const LocalNodeId& self_id);

protobuf::Message Connect(const PeerNodeId& node_id, const LocalEndpointPair& our_endpoint,
                          const LocalNodeId& self_node_id,
                          const LocalConnectionId& self_connection_id,
                          IsClient is_client = IsClient(false),
                          rudp::NatType nat_type = rudp::NatType::kUnknown,
                          IsRelayMessage = IsRelayMessage(false),
                          RelayConnectionId relay_connection_id = RelayConnectionId(NodeId()));

protobuf::Message FindNodes(unsigned int num_nodes_requested, const PeerNodeId& peer_id,
                            const LocalNodeId& self_node_id,
                            IsRelayMessage relay_message = IsRelayMessage(false),
                            RelayConnectionId relay_connection_id = RelayConnectionId(NodeId()));

protobuf::Message ConnectSuccess(const PeerNodeId& peer_node_id, const LocalNodeId& self_node_id,
                                 const LocalConnectionId& self_connection_id, IsRequestor requestor,
                                 IsClient client_node);

protobuf::Message ConnectSuccessAcknowledgement(const PeerNodeId& node_id,
                                                const LocalNodeId& self_node_id,
                                                const LocalConnectionId& self_connection_id,
                                                IsRequestor requestor,
                                                const std::vector<NodeInfo>& close_ids,
                                                IsClient client_node);

protobuf::Message InformClientOfNewCloseNode(const PeerNodeId& node_id,
                                             const LocalNodeId& this_node_id,
                                             const PeerNodeId& client_node_id);

protobuf::Message GetGroup(const NodeId& node_id, const NodeId& my_node_id);

}  // namespace rpcs

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_RPCS_H_
