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

#include "cereal/types/array.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/rudp/contact.h"
#include "maidsafe/rudp/types.h"

#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

// NEW
// #################################################################################################

// MurmurHash2 is based on the MurMurHash2 implementation written by Austin Appleby, and placed in
// the public domain by him.
MurmurHash MurmurHash2(const std::vector<byte>& input);



// OLD
// #################################################################################################
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
// Address NodeInNthBucket(const Address& Address, int bucket);
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

namespace cereal {

template <typename Archive>
void save(Archive& archive, const maidsafe::rudp::Endpoint& endpoint) {
  using address_v6 = boost::asio::ip::address_v6;
  address_v6 ip_address;
  if (endpoint.address().is_v4()) {
    ip_address = address_v6::v4_compatible(endpoint.address().to_v4());
  } else {
    ip_address = endpoint.address().to_v6();
  }
  address_v6::bytes_type bytes = ip_address.to_bytes();
  archive(bytes, endpoint.port());
}

template <typename Archive>
void load(Archive& archive, maidsafe::rudp::Endpoint& endpoint) {
  using address_v6 = boost::asio::ip::address_v6;
  using address = boost::asio::ip::address;
  address_v6::bytes_type bytes;
  unsigned short port;
  archive(bytes, port);
  address_v6 ip_v6_address(bytes);
  address ip_address;
  if (ip_v6_address.is_v4_compatible())
    ip_address = ip_v6_address.to_v4();
  else
    ip_address = ip_v6_address;
  endpoint = maidsafe::rudp::Endpoint(ip_address, port);
}

template <typename Archive>
void serialize(Archive& archive, maidsafe::rudp::EndpointPair& endpoints) {
  archive(endpoints.local, endpoints.external);
}

}  // namespace cereal

#endif  // MAIDSAFE_ROUTING_UTILS_H_
