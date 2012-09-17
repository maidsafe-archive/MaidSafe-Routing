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


#include <string>
#include <vector>

#include "boost/asio/ip/udp.hpp"

#include "maidsafe/common/rsa.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/routing_pb.h"


namespace maidsafe {

class NodeId;
namespace routing {

namespace protobuf {

class Message;
class Endpoint;

}  // namespace protobuf

class Message {
  explicit Message(protobuf::Message message);
};

class NetworkUtils;
class NonRoutingTable;
class RoutingTable;

void ValidateAndAddToRudp(NetworkUtils& network,
                          const NodeId& this_node_id,
                          const NodeId& peer_id,
                          rudp::EndpointPair peer_endpoint_pair,
                          const asymm::PublicKey& public_key,
                          const bool& client);
void ValidateAndAddToRoutingTable(NetworkUtils& network,
                                  RoutingTable& routing_table,
                                  NonRoutingTable& non_routing_table,
                                  const NodeId& peer_id,
                                  const NodeId& connection_id,
                                  const asymm::PublicKey& public_key,
                                  const bool& client);
void HandleSymmetricNodeAdd(RoutingTable& routing_table, const NodeId& peer_id,
                            const asymm::PublicKey& public_key);
bool IsRoutingMessage(const protobuf::Message& message);
bool IsNodeLevelMessage(const protobuf::Message& message);
bool IsRequest(const protobuf::Message& message);
bool IsResponse(const protobuf::Message& message);
bool IsDirect(const protobuf::Message& message);
bool CheckId(const std::string id_to_test);
bool ValidateMessage(const protobuf::Message &message);
void SetProtobufEndpoint(const boost::asio::ip::udp::endpoint& endpoint,
                         protobuf::Endpoint* pb_endpoint);
boost::asio::ip::udp::endpoint GetEndpointFromProtobuf(const protobuf::Endpoint& pb_endpoint);
std::string MessageTypeString(const protobuf::Message& message);
std::vector<boost::asio::ip::udp::endpoint> OrderBootstrapList(
                                  std::vector<boost::asio::ip::udp::endpoint> peer_endpoints);
protobuf::NatType NatTypeProtobuf(const rudp::NatType& nat_type);
rudp::NatType NatTypeFromProtobuf(const protobuf::NatType& nat_type_proto);
std::string PrintMessage(const protobuf::Message& message);
}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_UTILS_H_
