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

#ifndef MAIDSAFE_ROUTING_TYPES_H_
#define MAIDSAFE_ROUTING_TYPES_H_

#include "maidsafe/common/node_id.h"

#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

typedef TaggedValue<NodeId, struct NodeIdentifierType> LocalNodeId;
typedef TaggedValue<NodeId, struct ConnectionIdType> LocalConnectionId;
typedef TaggedValue<boost::asio::ip::udp::endpoint, struct LocalEndpointType> LocalEndpoint;
typedef TaggedValue<rudp::EndpointPair, struct LocalEndpointPairType> LocalEndpointPair;
typedef TaggedValue<NodeId, struct DestinationIdType> PeerNodeId;
typedef TaggedValue<NodeId, struct ConnectionIdType> PeerConnectionId;
typedef TaggedValue<boost::asio::ip::udp::endpoint, struct PeerEndpointType> PeerEndpoint;
typedef TaggedValue<rudp::EndpointPair, struct PeerEndpointPairType> PeerEndpointPair;
typedef TaggedValue<NodeInfo, struct PeerNodeInfoType> PeerNodeInfo;
typedef TaggedValue<bool, struct NodeType> IsClient;
typedef TaggedValue<std::string, struct IdentityType> Identity;
typedef TaggedValue<bool, struct RelayMessageType> IsRelayMessage;
typedef TaggedValue<NodeId, struct RelayIdType> RelayNodeId;
typedef TaggedValue<NodeId, struct RelayConnectionIdType> RelayConnectionId;
typedef TaggedValue<bool, struct RequestorType> IsRequestor;
typedef TaggedValue<bool, struct IsDirectType> IsDirectMessage;
typedef TaggedValue<bool, struct IsRequestType> IsRequestMessage;
typedef TaggedValue<std::string, struct MessageDataType> MessageData;
typedef TaggedValue<bool, struct IsRoutingMessageType> IsRoutingRpCMessage;
typedef TaggedValue<bool, struct IsCacheableType> IsCacheable;
typedef TaggedValue<NodeId, struct LastIdType> LastNodeId;
typedef TaggedValue<int, struct MessageIdType> MessageId;

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TYPES_H_
