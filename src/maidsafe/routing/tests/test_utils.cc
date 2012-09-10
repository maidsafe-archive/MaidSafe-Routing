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

NodeInfo MakeNode() {
  NodeInfo node;
  node.node_id = NodeId(RandomString(64));
  asymm::Keys keys;
  asymm::GenerateKeyPair(&keys);
  node.public_key = keys.public_key;
  node.connection_id = node.node_id;
  return node;
}

NodeInfoAndPrivateKey MakeNodeInfoAndKeys() {
  NodeInfo node;
  node.node_id = NodeId(RandomString(64));
  asymm::Keys keys;
  asymm::GenerateKeyPair(&keys);
  node.public_key = keys.public_key;
//  node.endpoint.address(GetLocalIp());
//  node.endpoint.port(GetRandomPort());
  node.connection_id = NodeId(NodeId::kRandomId);
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

asymm::Keys GetKeys(const NodeInfoAndPrivateKey& node) {
  asymm::Keys keys;
  keys.identity = node.node_info.node_id.String();
  keys.public_key = node.node_info.public_key;
  keys.private_key = node.private_key;
  return keys;
}

NodeId GenerateUniqueRandomId(const NodeId& holder, const uint16_t& pos) {
  std::string holder_id = holder.ToStringEncoded(NodeId::kBinary);
  std::bitset<64*8> holder_id_binary_bitset(holder_id);
  NodeId new_node;
  std::string new_node_string;
  // generate a random ID and make sure it has not been generated previously
  new_node = NodeId(NodeId::kRandomId);
  std::string new_id = new_node.ToStringEncoded(NodeId::kBinary);
  std::bitset<64*8> binary_bitset(new_id);
  for (uint16_t i(0); i < pos; ++i)
    holder_id_binary_bitset[i] = binary_bitset[i];
  new_node_string = holder_id_binary_bitset.to_string();
  new_node = NodeId(new_node_string, NodeId::kBinary);
  return new_node;
}

NodeId GenerateUniqueNonRandomId(const NodeId& holder, const uint64_t& id) {
  std::string holder_id = holder.ToStringEncoded(NodeId::kBinary);
  std::bitset<64*8> holder_id_binary_bitset(holder_id);
  NodeId new_node;
  std::string new_node_string;
    // generate a random ID and make sure it has not been generated previously
  new_node = NodeId(NodeId::kRandomId);
  std::string new_id = new_node.ToStringEncoded(NodeId::kBinary);
  std::bitset<64> binary_bitset(id);
  for (uint16_t i(0); i < 64; ++i)
    holder_id_binary_bitset[i] = binary_bitset[i];
  new_node_string = holder_id_binary_bitset.to_string();
  new_node = NodeId(new_node_string, NodeId::kBinary);
  return new_node;
}


NodeId GenerateUniqueRandomId(const uint16_t& pos) {
  NodeId holder(NodeId(NodeId::kMaxId) ^ NodeId(NodeId::kMaxId));
  return GenerateUniqueRandomId(holder, pos);
}

NodeId GenerateUniqueNonRandomId(const uint64_t& pos) {
  NodeId holder(NodeId(NodeId::kMaxId) ^ NodeId(NodeId::kMaxId));
  return GenerateUniqueNonRandomId(holder, pos);
}

int NetworkStatus(const bool& client, const int& status) {
  uint16_t max_size(client ? Parameters::max_client_routing_table_size :
                      Parameters::max_routing_table_size);
  return (status > 0) ? (status * 100 / max_size) : status;
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
