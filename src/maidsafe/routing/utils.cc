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

#include "maidsafe/routing/utils.h"

#include <limits>

#include "maidsafe/common/utils.h"

namespace maidsafe {

namespace routing {

// NEW
// ##################################################################################################

MurmurHash MurmurHash2(const std::vector<byte>& input) {
  static_assert(sizeof(int) == 4, "This implementation requires size of int to be 4 bytes.");
  assert(input.size() < std::numeric_limits<uint32_t>::max());

  // 'm' and 'r' are mixing constants generated offline.
  // They're not really 'magic', they just happen to work well.

  const uint32_t m = 0x5bd1e995;
  const int r = 24;

  // Initialize the hash to a 'random' value

  uint32_t len = static_cast<uint32_t>(input.size());
  uint32_t h = len;

  // Mix 4 bytes at a time into the hash

  const unsigned char* data = &input[0];

  while (len >= 4) {
    uint32_t k = *reinterpret_cast<const uint32_t*>(data);

    k *= m;
    k ^= k >> r;
    k *= m;

    h *= m;
    h ^= k;

    data += 4;
    len -= 4;
  }

  // Handle the last few bytes of the input array

  switch (len) {
    case 3:
      h ^= data[2] << 16;
    case 2:
      h ^= data[1] << 8;
    case 1:
      h ^= data[0];
      h *= m;
  };

  // Do a few final mixes of the hash to ensure the last few
  // bytes are well-incorporated.

  h ^= h >> 13;
  h *= m;
  h ^= h >> 15;

  return h;
}

// OLD
// ##################################################################################################
//
// #ifdef WIN32
// #include <pwd.h>
// #endif
//
// #include <string>
// #include <algorithm>
//
// #include "boost/filesystem/operations.hpp"
// #include "boost/filesystem/path.hpp"
//
// #include "maidsafe/common/log.h"
// #include "maidsafe/common/Address.h"
// #include "maidsafe/rudp/return_codes.h"
//
// #include "maidsafe/routing/client_routing_table.h"
// #include "maidsafe/routing/message_handler.h"
// #include "maidsafe/routing/message.h"
// #include "maidsafe/routing/network.h"
// #include "maidsafe/routing/return_codes.h"
// #include "maidsafe/routing/routing.pb.h"
// #include "maidsafe/routing/routing_table.h"
// #include "maidsafe/routing/rpcs.h"
//
// int AddToRudp(Network& network, const Address& this_Address, const Address& this_connection_id,
//               const Address& peer_id, const Address& peer_connection_id,
//               rudp::EndpointPair peer_endpoint_pair, bool requestor, bool client) {
//   LOG(kVerbose) << "AddToRudp. peer_id: " << peer_id << " ,connection id: " <<
//   peer_connection_id;
//   protobuf::Message connect_success(
//       rpcs::ConnectSuccess(peer_id, this_Address, this_connection_id, requestor, client));
//   int result =
//       network.Add(peer_connection_id, peer_endpoint_pair, connect_success.SerializeAsString());
//   if (result == rudp::kConnectionAlreadyExists) {
//     network.SendToDirect(connect_success, peer_id, peer_connection_id);
//   } else if (result == kSuccess) {
//     LOG(kVerbose) << "rudp.Add succeeded for peer node [" << peer_id
//                   << "]. Connection id : " << peer_connection_id;
//   } else {
//     LOG(kError) << "rudp add failed for peer node [" << peer_id
//                 << "]. Connection id : " << peer_connection_id << ". result : " << result;
//   }
//   return result;
// }
//
// bool ValidateAndAddToRoutingTable(Network& network, RoutingTable& routing_table,
//                                   ClientRoutingTable& client_routing_table, const Address&
//                                   peer_id,
//                                   const Address& connection_id, const asymm::PublicKey&
//                                   public_key,
//                                   bool client) {
//   if (network.MarkConnectionAsValid(connection_id) != kSuccess) {
//     LOG(kError) << "[" << routing_table.kAddress()
//                 << "]  Rudp failed to validate connection with  Peer id : " << peer_id
//                 << " , Connection id : " << connection_id;
//     return false;
//   }
//
//   NodeInfo peer;
//   peer.id = peer_id;
//   peer.public_key = public_key;
//   peer.connection_id = connection_id;
//   bool routing_accepted_node(false);
//   if (client) {
//     Address furthest_close_Address =
//         routing_table.GetNthClosestNode(Address(routing_table.kNodeId()),
//                                         2 * Parameters::closest_nodes_size).id;
//
//     if (client_routing_table.AddNode(peer, furthest_close_Address))
//       routing_accepted_node = true;
//   } else {  // Vaults
//     if (routing_table.AddNode(peer))
//       routing_accepted_node = true;
//   }
//
//   if (routing_accepted_node) {
//     LOG(kVerbose) << "[" << routing_table.kAddress() << "] "
//                   << "added " << (client ? "client-" : "") << "node to " << (client ? "non-" :
//                   "")
//                   << "routing table.  Node ID: " << HexSubstr(peer_id.string());
//     return true;
//   }
//
//   LOG(kInfo) << "[" << routing_table.kAddress() << "] "
//              << "failed to add " << (client ? "client-" : "") << "node to "
//              << (client ? "non-" : "") << "routing table.  Node ID: " <<
//              HexSubstr(peer_id.string())
//              << ". Added rudp connection will be removed.";
//   network.Remove(connection_id);
//   return false;
// }
//
// void InformClientOfNewCloseNode(Network& network, const NodeInfo& client,
//                                 const NodeInfo& new_close_node, const Address& this_Address) {
//   protobuf::Message inform_client_of_new_close_node(
//       rpcs::InformClientOfNewCloseNode(new_close_node.id, this_Address, client.id));
//   network.SendToDirect(inform_client_of_new_close_node, client.id, client.connection_id);
// }
//
// // GroupRangeStatus GetProximalRange(const Address& target_id, const Address& Address,
// //                                   const Address& this_Address,
// //                                   const crypto::BigInt& proximity_radius,
// //                                   const std::vector<Address>& holders) {
// //   assert((std::find(holders.begin(), holders.end(), target_id) == holders.end()) &&
// //          "Ensure to remove target id entry from holders, if present");
// //   assert(std::is_sorted(holders.begin(), holders.end(),
// //                         [target_id](const Address & lhs, const Address & rhs) {
// //            return Address::CloserToTarget(lhs, rhs, target_id);
// //          }) &&
// //          "Ensure to sort holders in order of distance to targer_id");
// //   assert(holders.size() <= Parameters::group_size);
// //
// //   if ((target_id == Address) || (target_id == this_node_id))
// //     return GroupRangeStatus::kOutwithRange;
// //
// //   if (std::find(holders.begin(), holders.end(), Address) != holders.end()) {
// //     return GroupRangeStatus::kInRange;
// //   }
// //
// //   Address distance_id(Address ^ target_id);
// //   crypto::BigInt distance((distance_id.ToStringEncoded(
// //       Address::EncodingType::kHex) + 'h').c_str());
// //   return (distance < proximity_radius) ? GroupRangeStatus::kInProximalRange
// //                                        : GroupRangeStatus::kOutwithRange;
// // }
//
// bool IsRoutingMessage(const protobuf::Message& message) { return message.routing_message(); }
//
// bool IsNodeLevelMessage(const protobuf::Message& message) { return !IsRoutingMessage(message); }
//
// bool IsRequest(const protobuf::Message& message) { return (message.request()); }
//
// bool IsResponse(const protobuf::Message& message) { return !IsRequest(message); }
//
// bool IsDirect(const protobuf::Message& message) { return message.direct(); }
//
// bool IsCacheableGet(const protobuf::Message& message) {
//   return (!(message.has_relay_id() || message.has_relay_connection_id()) &&
//           message.has_cacheable() &&
//           (static_cast<Cacheable>(message.cacheable()) == Cacheable::kGet));
// }
//
// bool IsCacheablePut(const protobuf::Message& message) {
//   return (!(message.has_relay_id() || message.has_relay_connection_id()) &&
//           message.has_cacheable() &&
//           (static_cast<Cacheable>(message.cacheable()) == Cacheable::kPut));
// }
//
// bool IsAck(const protobuf::Message& message) {
//   return message.type() == static_cast<int>(MessageType::kAcknowledgement);
// }
//
// bool IsConnectSuccessAcknowledgement(const protobuf::Message& message) {
//   return message.type() == static_cast<int>(MessageType::kConnectSuccessAcknowledgement);
// }
//
// bool IsClientToClientMessageWithDifferentAddresss(const protobuf::Message& message,
//                                                  const bool is_destination_client) {
//   return (is_destination_client && message.request() && message.client_node() &&
//           (message.DestinationAddress() != message.SourceAddress()));
// }
//
// bool CheckId(const std::string& id_to_test) { return id_to_test.size() == Address::kSize; }
//
// bool ValidateMessage(const protobuf::Message& message) {
//   if (!message.IsInitialized()) {
//     LOG(kWarning) << "Uninitialised message dropped.";
//     return false;
//   }
//
//   // Message has traversed more hops than expected
//   if (message.hops_to_live() <= 0) {
//     std::string route_history;
//     for (const auto& route : message.route_history())
//       route_history += HexSubstr(route) + ", ";
//     LOG(kError) << "Message has traversed more hops than expected. "
//                 << Parameters::max_route_history
//                 << " last hops in route history are: " << route_history
//                 << " \nMessage source: " << HexSubstr(message.SourceAddress())
//                 << ", \nMessage destination: " << HexSubstr(message.DestinationAddress())
//                 << ", \nMessage type: " << message.type() << ", \nMessage id: " << message.id();
//     return false;
//   }
//   // Invalid destination id, unknown message
//   if (!CheckId(message.DestinationAddress())) {
//     LOG(kWarning) << "Stray message dropped, need destination ID for processing."
//                   << " id: " << message.id();
//     return false;
//   }
//
//   if (!(message.has_SourceAddress() || (message.has_relay_id() &&
//   message.has_relay_connection_id()))) {
//     LOG(kWarning) << "Message should have either src id or relay information.";
//     assert(false && "Message should have either src id or relay information.");
//     return false;
//   }
//
//   if (message.has_SourceAddress() && !CheckId(message.source_address())) {
//     LOG(kWarning) << "Invalid source id field.";
//     return false;
//   }
//
//   if (message.has_relay_id() && Address(message.relay_id()).IsZero()) {
//     LOG(kWarning) << "Invalid relay id field.";
//     return false;
//   }
//
//   if (message.has_relay_connection_id() && Address(message.relay_connection_id()).IsZero()) {
//     LOG(kWarning) << "Invalid relay connection id field.";
//     return false;
//   }
//
//   if (static_cast<MessageType>(message.type()) == MessageType::kConnect)
//     if (!message.direct()) {
//       LOG(kWarning) << "kConnectRequest type message must be direct.";
//       return false;
//     }
//
//   if (static_cast<MessageType>(message.type()) == MessageType::kFindNodes &&
//       (message.request() == false))
//     if ((!message.direct())) {
//       LOG(kWarning) << "kFindNodesResponse type message must be direct.";
//       return false;
//     }
//
//   return true;
// }

// Address NodeInNthBucket(const Address& Address, int bucket) {
//   // assert(bucket < Address::kSize * 8);
//   auto binary_string(Address.ToStringEncoded(Address::EncodingType::kBinary));
//   while (bucket >= 0) {
//     binary_string.at(bucket) = (binary_string.at(bucket) == '1') ? '0' : '1';
//     bucket--;
//   }
//   return Address(binary_string, Address::EncodingType::kBinary);
// }
//
// void SetProtobufEndpoint(const boost::asio::ip::udp::endpoint& endpoint,
//                          protobuf::Endpoint* pb_endpoint) {
//   if (pb_endpoint) {
//     pb_endpoint->set_ip(endpoint.address().to_string().c_str());
//     pb_endpoint->set_port(endpoint.port());
//   }
// }
//
// boost::asio::ip::udp::endpoint GetEndpointFromProtobuf(const protobuf::Endpoint& pb_endpoint) {
//   return boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string(pb_endpoint.ip()),
//                                         static_cast<uint16_t>(pb_endpoint.port()));
// }
//
// std::string MessageTypeString(const protobuf::Message& message) {
//   std::string message_type;
//   switch (static_cast<MessageType>(message.type())) {
//     case MessageType::kPing:
//       message_type = "kPing     ";
//       break;
//     case MessageType::kConnect:
//       message_type = "kConnect  ";
//       break;
//     case MessageType::kFindNodes:
//       message_type = "kFindNodes";
//       break;
//     case MessageType::kConnectSuccess:
//       message_type = "kC-Success";
//       break;
//     case MessageType::kConnectSuccessAcknowledgement:
//       message_type = "kC-Suc-Ack";
//       break;
//     case MessageType::kGetGroup:
//       message_type = "kGetGroup";
//       break;
//     case MessageType::kNodeLevel:
//       message_type = "kNodeLevel";
//       break;
//     case MessageType::kAcknowledgement:
//       message_type = "kAcknowledgement";
//       break;
//     default:
//       message_type = "Unknown  ";
//   }
//   if (message.request())
//     message_type = message_type + " Req";
//   else
//     message_type = message_type + " Res";
//   return message_type;
// }
//
// std::vector<boost::asio::ip::udp::endpoint> OrderBootstrapList(
//     std::vector<boost::asio::ip::udp::endpoint> peer_endpoints) {
//   if (peer_endpoints.empty())
//     return peer_endpoints;
//   auto copy_vector(peer_endpoints);
//   for (auto& endpoint : copy_vector) {
//     endpoint.port(kLivePort);
//   }
//   auto it = std::unique(copy_vector.begin(), copy_vector.end());
//   copy_vector.resize(it - copy_vector.begin());
//   std::reverse(peer_endpoints.begin(), peer_endpoints.end());
//   peer_endpoints.resize(peer_endpoints.size() + copy_vector.size());
//   for (const auto& i : copy_vector)
//     peer_endpoints.push_back(i);
//   std::reverse(peer_endpoints.begin(), peer_endpoints.end());
//   return peer_endpoints;
// }
//
// protobuf::NatType NatTypeProtobuf(const rudp::NatType& nat_type) {
//   switch (nat_type) {
//     case rudp::NatType::kSymmetric:
//       return protobuf::NatType::kSymmetric;
//       break;
//     case rudp::NatType::kOther:
//       return protobuf::NatType::kOther;
//       break;
//     default:
//       return protobuf::NatType::kUnknown;
//       break;
//   }
// }
//
// rudp::NatType NatTypeFromProtobuf(const protobuf::NatType& nat_type_proto) {
//   switch (nat_type_proto) {
//     case protobuf::NatType::kSymmetric:
//       return rudp::NatType::kSymmetric;
//       break;
//     case protobuf::NatType::kOther:
//       return rudp::NatType::kOther;
//       break;
//     default:
//       return rudp::NatType::kUnknown;
//       break;
//   }
// }
//
// std::string PrintMessage(const protobuf::Message& message) {
//   std::string s = "\n\n Message type : ";
//   s += MessageTypeString(message);
//   std::string direct((message.direct() ? "direct" : "group"));
//   s += std::string("\n direct : " + direct);
//   if (message.has_SourceAddress())
//     s += std::string("\n SourceAddress : " + HexSubstr(message.source_address()));
//   if (message.has_DestinationAddress())
//     s += std::string("\n DestinationAddress : " + HexSubstr(message.destination_id()));
//   if (message.has_relay_id())
//     s += std::string("\n relay_id : " + HexSubstr(message.relay_id()));
//   if (message.has_relay_connection_id())
//     s += std::string("\n relay_connection_id : " + HexSubstr(message.relay_connection_id()));
//   std::stringstream id;
//   id << message.id();
//   if (message.has_id())
//     s += std::string("\n id : " + id.str());
//   s += "\n\n";
//   return s;
// }
//
// std::vector<Address> DeserializeNodeIdList(const std::string& node_list_str) {
//   std::vector<Address> node_list;
//   protobuf::AddressList node_list_msg;
//   node_list_msg.ParseFromString(node_list_str);
//   for (int i = 0; i < node_list_msg.Address_list_size(); ++i)
//     node_list.push_back(Address(node_list_msg.Address_list(i).node_id()));
//   return node_list;
// }
//
// std::string SerializeAddressList(const std::vector<Address>& node_list) {
//   protobuf::AddressList node_list_msg;
//   for (const auto& Address : node_list) {
//     auto entry = node_list_msg.add_Address_list();
//     entry->set_Address(node_id.string());
//   }
//   return node_list_msg.SerializeAsString();
// }
//
// SingleToSingleMessage CreateSingleToSingleMessage(const protobuf::Message& proto_message) {
//   return SingleToSingleMessage(proto_message.data(0),
//                                SingleSource(Address(proto_message.SourceAddress())),
//                                SingleId(Address(proto_message.DestinationAddress())),
//                                static_cast<Cacheable>(proto_message.cacheable()));
// }
//
// SingleToGroupMessage CreateSingleToGroupMessage(const protobuf::Message& proto_message) {
//   return SingleToGroupMessage(proto_message.data(0),
//                               SingleSource(Address(proto_message.SourceAddress())),
//                               GroupId(Address(proto_message.group_destination())),
//                               static_cast<Cacheable>(proto_message.cacheable()));
// }
//
// GroupToSingleMessage CreateGroupToSingleMessage(const protobuf::Message& proto_message) {
//   return GroupToSingleMessage(proto_message.data(0),
//                               GroupSource(GroupId(Address(proto_message.group_source())),
//                                           SingleId(Address(proto_message.SourceAddress()))),
//                               SingleId(Address(proto_message.DestinationAddress())),
//                               static_cast<Cacheable>(proto_message.cacheable()));
// }
//
// GroupToGroupMessage CreateGroupToGroupMessage(const protobuf::Message& proto_message) {
//   return GroupToGroupMessage(proto_message.data(0),
//                              GroupSource(GroupId(Address(proto_message.group_source())),
//                                          SingleId(Address(proto_message.SourceAddress()))),
//                              GroupId(Address(proto_message.group_destination())),
//                              static_cast<Cacheable>(proto_message.cacheable()));
// }
//
// SingleToGroupRelayMessage CreateSingleToGroupRelayMessage(const protobuf::Message& proto_message)
// {
//   SingleSource single_src(Address(proto_message.relay_id()));
//   Address connection_id(proto_message.relay_connection_id());
//   SingleSource single_src_relay_node(Address(proto_message.SourceAddress()));
//   SingleRelaySource single_relay_src(single_src,  // original sender
//                                      connection_id, single_src_relay_node);
//
//   return SingleToGroupRelayMessage(proto_message.data(0),
//                                    single_relay_src,  // relay node
//                                    GroupId(Address(proto_message.group_destination())),
//                                    static_cast<Cacheable>(proto_message.cacheable()));
//
//   //  return SingleToGroupRelayMessage(proto_message.data(0),
//   //      SingleSourceRelay(SingleSource(Address(proto_message.relay_id())), // original sender
//   //                        Address(proto_message.relay_connection_id()),
//   //                        SingleSource(Address(proto_message.SourceAddress()))),  // relay node
//   //          GroupId(Address(proto_message.group_destination())),
//   //              static_cast<Cacheable>(proto_message.cacheable()));
// }

}  // namespace routing

}  // namespace maidsafe
