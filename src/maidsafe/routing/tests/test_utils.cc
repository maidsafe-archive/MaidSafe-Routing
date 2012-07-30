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

#include "maidsafe/routing/tests/test_utils.h"

#include <set>
#include <bitset>
#include <string>

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/node_id.h"

namespace asio = boost::asio;
namespace ip = asio::ip;

namespace maidsafe {

namespace routing {

namespace test {

uint16_t GetRandomPort() {
  static std::set<uint16_t> already_used_ports;
  bool unique(false);
  uint16_t port(0);
  uint16_t failed_attempts(0);
  do {
    port = (RandomUint32() % 48126) + 1025;
    unique = (already_used_ports.insert(port)).second;
  } while (!unique && failed_attempts++ < 1000);
  if (failed_attempts > 1000)
    LOG(kError) << "Unable to generate unique ports";
  return port;
}

NodeInfo MakeNode() {
  NodeInfo node;
  node.node_id = NodeId(RandomString(64));
  asymm::Keys keys;
  asymm::GenerateKeyPair(&keys);
  node.public_key = keys.public_key;
  node.endpoint.address(GetLocalIp());
  node.endpoint.port(GetRandomPort());
  return node;
}

NodeInfoAndPrivateKey MakeNodeInfoAndKeys() {
  NodeInfo node;
  node.node_id = NodeId(RandomString(64));
  asymm::Keys keys;
  asymm::GenerateKeyPair(&keys);
  node.public_key = keys.public_key;
  node.endpoint.address(GetLocalIp());
  node.endpoint.port(GetRandomPort());
  NodeInfoAndPrivateKey node_info_and_private_key;
  node_info_and_private_key.node_info = node;
  node_info_and_private_key.private_key = keys.private_key;
  return node_info_and_private_key;
}

asymm::Keys MakeKeys() {
  NodeInfoAndPrivateKey node(MakeNodeInfoAndKeys());
  asymm::Keys keys;
  keys.identity = node.node_info.node_id.String();
  keys.public_key = node.node_info.public_key;
  keys.private_key = node.private_key;
  return keys;
}

asymm::Keys GetKeys(const NodeInfoAndPrivateKey &node) {
  asymm::Keys keys;
  keys.identity = node.node_info.node_id.String();
  keys.public_key = node.node_info.public_key;
  keys.private_key = node.private_key;
  return keys;
}

NodeId GenerateUniqueRandomId(const NodeId &holder, const uint16_t &pos) {
  std::string holder_id = holder.ToStringEncoded(NodeId::kBinary);
  std::bitset<64*8> holder_id_binary_bitset(holder_id);
  NodeId new_node;
  std::string new_node_string;
  // generate a random ID and make sure it has not been generated previously
  new_node = NodeId(NodeId::kRandomId);
  std::string new_id = new_node.ToStringEncoded(NodeId::kBinary);
  std::bitset<64*8> binary_bitset(new_id);
  for (uint16_t i = kKeySizeBits - 1; i >= pos; --i)
    binary_bitset[i] = holder_id_binary_bitset[i];
  binary_bitset[pos].flip();
  new_node_string = binary_bitset.to_string();
  new_node = NodeId(new_node_string, NodeId::kBinary);
  return new_node;
}

int NetworkStatus(const bool &client, const int &status) {
  uint16_t max_size(client ? Parameters::max_client_routing_table_size :
                      Parameters::max_routing_table_size);
  return (status > 0) ? (status * 100 / max_size) : status;
}

ip::address GetLocalIp(ip::udp::endpoint peer_endpoint) {
  asio::io_service io_service;
  ip::udp::socket socket(io_service);
  try {
    socket.connect(peer_endpoint);
    if (socket.local_endpoint().address().is_unspecified() ||
        socket.local_endpoint().address().is_loopback())
      return ip::address();
    return socket.local_endpoint().address();
  }
  catch(const std::exception &e) {
    LOG(kError) << "Failed trying to connect to " << peer_endpoint << " - " << e.what();
    return ip::address();
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
