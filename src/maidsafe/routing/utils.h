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

#ifndef MAIDSAFE_ROUTING_UTILS_H_
#define MAIDSAFE_ROUTING_UTILS_H_

#include "maidsafe/routing/parameters.h"

namespace maidsafe {

namespace routing {

namespace protobuf { class Message;}  // namespace protobuf

class Message {
  explicit Message(protobuf::Message message);
};

class NetworkUtils;
class NonRoutingTable;
class RoutingTable;

bool ClosestToMe(protobuf::Message &message);

bool InClosestNodesToMe(protobuf::Message &message);

void ValidateThisNode(NetworkUtils &network_,
                      RoutingTable &routing_table,
                      NonRoutingTable &non_routing_table,
                      const NodeId& node_id,
                      const asymm::PublicKey &public_key,
                      const rudp::EndpointPair &their_endpoint,
                      const rudp::EndpointPair &our_endpoint,
                      const bool &client);

bool IsRoutingMessage(const protobuf::Message &message);
bool IsNodeLevelMessage(const protobuf::Message &message);
bool IsRequest(const protobuf::Message &message);
bool IsResponse(const protobuf::Message &message);
bool ValidateMessage(const protobuf::Message &message);

void SetProtobufEndpoint(const Endpoint& endpoint, protobuf::Endpoint *pbendpoint);
Endpoint GetEndpointFromProtobuf(const protobuf::Endpoint &pbendpoint);

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_UTILS_H_
