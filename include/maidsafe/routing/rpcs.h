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
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

typedef TaggedValue<NodeId, struct NodeIdentifierType> SelfNodeId;
typedef TaggedValue<NodeId, struct ConnectionIdType> SelfConnectionId;
typedef TaggedValue<bool, struct NodeType> SelfNodeType;
typedef TaggedValue<rudp::EndpointPair, struct EndpointPair> SelfEndpoint;
typedef TaggedValue<rudp::NatType, struct NatType> SelfNatType;
typedef TaggedValue<NodeId, struct DestinationIdType> PeerNodeId;
typedef TaggedValue<std::string, struct IdentityType> Identity;
typedef TaggedValue<bool, struct RelayMessageType> RelayMessage;
typedef TaggedValue<NodeId, struct RelayConnectionIdType> RelayConnectionId;
typedef TaggedValue<bool, struct RequestorType> Requestor;
typedef TaggedValue<std::vector<NodeInfo>, struct CloseNodeIdsType> CloseNodeIds;
typedef TaggedValue<MessageType, struct MessageRpcType> RpcType;

namespace rpcs {

template <typename PropertyType, typename... Args>
void SetMessageProperties(protobuf::Message& message, const PropertyType& value, Args... args) {
  SetMessageProperty(message, value);
  SetMessageProperties(message, args...);
}

template <typename PropertyType>
void SetMessageProperty(protobuf::Message& /*message*/, const PropertyType& /*value*/) {}

protobuf::Message Ping(const NodeId& node_id, const std::string& identity);

protobuf::Message Connect(const NodeId& node_id, const rudp::EndpointPair& our_endpoint,
                          const NodeId& this_node_id, const NodeId& this_connection_id,
                          bool client_node = false,
                          rudp::NatType nat_type = rudp::NatType::kUnknown,
                          bool relay_message = false, NodeId relay_connection_id = NodeId());

protobuf::Message FindNodes(const NodeId& node_id, const NodeId& this_node_id,
                            int num_nodes_requested, bool relay_message = false,
                            NodeId relay_connection_id = NodeId());

protobuf::Message ProxyConnect(const NodeId& node_id, const NodeId& this_node_id,
                               const rudp::EndpointPair& endpoint_pair, bool relay_message = false,
                               NodeId relay_connection_id = NodeId());

protobuf::Message ConnectSuccess(const NodeId& node_id, const NodeId& this_node_id,
                                 const NodeId& this_connection_id, bool requestor,
                                 bool client_node);

protobuf::Message ConnectSuccessAcknowledgement(const NodeId& node_id, const NodeId& this_node_id,
                                                const NodeId& this_connection_id, bool requestor,
                                                const std::vector<NodeInfo>& close_ids,
                                                bool client_node);

protobuf::Message InformClientOfNewCloseNode(const NodeId& node_id, const NodeId& this_node_id,
                                             const NodeId& client_node_id);

protobuf::Message GetGroup(const NodeId& node_id, const NodeId& my_node_id);

}  // namespace rpcs

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_RPCS_H_
