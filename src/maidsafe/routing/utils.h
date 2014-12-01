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

#ifndef MAIDSAFE_ROUTING_UTILS_H_
#define MAIDSAFE_ROUTING_UTILS_H_

#include <vector>

#include "maidsafe/common/Address.h"

#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

// NEW
// ##################################################################################################

// murmur_hash2 is based on the MurMurHash2 implementation written by Austin Appleby, and placed in
// the public domain by him.
murmur_hash murmur_hash2(const std::vector<byte>& input);


// OLD
// ##################################################################################################
//
// #include "boost/asio/ip/udp.hpp"
//
// #include "maidsafe/common/rsa.h"
// #include "maidsafe/common/Address.h"
//
// #include "maidsafe/rudp/managed_connections.h"
//
// #include "maidsafe/passport/types.h"
//
// #include "maidsafe/routing/api_config.h"
// #include "maidsafe/routing/node_info.h"
// #include "maidsafe/routing/parameters.h"
// #include "maidsafe/routing/routing.pb.h"
// #include "maidsafe/routing/public_key_holder.h"
//
// namespace protobuf {
//
// class Message;
// class Endpoint;
//
// }  // namespace protobuf
//
// class Network;
// class ClientRoutingTable;
// class RoutingTable;
//
// int AddToRudp(Network& network, const Address& this_Address, const Address& this_connection_id,
//               const Address& peer_id, const Address& peer_connection_id,
//               rudp::EndpointPair peer_endpoint_pair, bool requestor, bool client);
//
// bool ValidateAndAddToRoutingTable(Network& network, RoutingTable& routing_table,
//                                   ClientRoutingTable& client_routing_table, const Address&
//                                   peer_id,
//                                   const Address& connection_id, const asymm::PublicKey&
//                                   public_key,
//                                   bool client);
//
// void InformClientOfNewCloseNode(Network& network, const NodeInfo& client,
//                                 const NodeInfo& new_close_node, const Address& this_Address);
//
// // GroupRangeStatus GetProximalRange(const Address& target_id, const Address& Address,
// //                                   const Address& this_Address,
// //                                   const crypto::BigInt& proximity_radius,
// //                                   const std::vector<Address>& holders);
//
// bool IsRoutingMessage(const protobuf::Message& message);
// bool IsNodeLevelMessage(const protobuf::Message& message);
// bool IsRequest(const protobuf::Message& message);
// bool IsResponse(const protobuf::Message& message);
// bool IsDirect(const protobuf::Message& message);
// bool IsCacheableGet(const protobuf::Message& message);
// bool IsCacheablePut(const protobuf::Message& message);
// bool IsAck(const protobuf::Message& message);
// bool IsConnectSuccessAcknowledgement(const protobuf::Message& message);
// bool IsClientToClientMessageWithDifferentAddresss(const protobuf::Message& message,
//                                                  const bool is_destination_client);
// bool CheckId(const std::string& id_to_test);
// bool ValidateMessage(const protobuf::Message& message);
Address NodeInNthBucket(const Address& Address, int bucket);
// void SetProtobufEndpoint(const boost::asio::ip::udp::endpoint& endpoint,
//                          protobuf::Endpoint* pb_endpoint);
// boost::asio::ip::udp::endpoint GetEndpointFromProtobuf(const protobuf::Endpoint& pb_endpoint);
// std::string MessageTypeString(const protobuf::Message& message);
// std::vector<boost::asio::ip::udp::endpoint> OrderBootstrapList(
//     std::vector<boost::asio::ip::udp::endpoint> peer_endpoints);
// protobuf::NatType NatTypeProtobuf(const rudp::NatType& nat_type);
// rudp::NatType NatTypeFromProtobuf(const protobuf::NatType& nat_type_proto);
// std::string PrintMessage(const protobuf::Message& message);
// std::vector<Address> DeserializeNodeIdList(const std::string& node_list_str);
// std::string SerializeAddressList(const std::vector<Address>& node_list);
// SingleToSingleMessage CreateSingleToSingleMessage(const protobuf::Message& proto_message);
// SingleToGroupMessage CreateSingleToGroupMessage(const protobuf::Message& proto_message);
// GroupToSingleMessage CreateGroupToSingleMessage(const protobuf::Message& proto_message);
// GroupToGroupMessage CreateGroupToGroupMessage(const protobuf::Message& proto_message);
// SingleToGroupRelayMessage CreateSingleToGroupRelayMessage(const protobuf::Message&
// proto_message);
//
//
}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_UTILS_H_
