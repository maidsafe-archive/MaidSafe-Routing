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


protobuf::Message Ping(const PeerNodeId& peer_id, const SelfNodeId& self_id);

protobuf::Message Connect(const PeerNodeId& node_id, const SelfEndpoint& our_endpoint,
                          const SelfNodeId& self_node_id,
                          const SelfConnectionId& self_connection_id,
                          IsClient is_client = IsClient(false),
                          NatType nat_type = NatType(rudp::NatType::kUnknown),
                          IsRelayMessage = IsRelayMessage(false),
                          RelayConnectionId relay_connection_id = RelayConnectionId(NodeId()));

protobuf::Message FindNodes(unsigned int num_nodes_requested, const PeerNodeId& peer_id,
                            const SelfNodeId& self_id,
                            IsRelayMessage relay_message = IsRelayMessage(false),
                            RelayConnectionId relay_connection_id = RelayConnectionId(NodeId()));

protobuf::Message ProxyConnect(const NodeId& node_id, const NodeId& this_node_id,
                               const rudp::EndpointPair& endpoint_pair, bool relay_message = false,
                               NodeId relay_connection_id = NodeId());

protobuf::Message ConnectSuccess(const PeerNodeId& peer_node_id, const SelfNodeId& self_node_id,
                                 const SelfConnectionId& self_connection_id, IsRequestor requestor,
                                 IsClient client_node);

protobuf::Message ConnectSuccessAcknowledgement(
    const PeerNodeId& node_id, const SelfNodeId& self_node_id,
    const SelfConnectionId& self_connection_id, IsRequestor requestor,
    const std::vector<NodeInfo>& close_ids, IsClient client_node);

protobuf::Message InformClientOfNewCloseNode(const PeerNodeId& node_id,
                                             const SelfNodeId& this_node_id,
                                             const NodeId& client_node_id);

protobuf::Message GetGroup(const NodeId& node_id, const NodeId& my_node_id);

}  // namespace rpcs

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_RPCS_H_
