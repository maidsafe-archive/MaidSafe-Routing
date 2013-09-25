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

#include "maidsafe/routing/tests/test_utils.h"

#include <algorithm>
#include <set>
#include <bitset>
#include <string>

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/parameters.h"

namespace asio = boost::asio;
namespace ip = asio::ip;

namespace maidsafe {

namespace routing {

namespace test {

namespace {

template <typename FobType>
NodeInfoAndPrivateKey MakeNodeInfoAndKeysWithFob(FobType fob) {
  NodeInfo node;
  node.node_id = NodeId(fob.name()->string());
  node.public_key = fob.public_key();
  node.connection_id = node.node_id;
  NodeInfoAndPrivateKey node_info_and_private_key;
  node_info_and_private_key.node_info = node;
  node_info_and_private_key.private_key = fob.private_key();
  return node_info_and_private_key;
}

}  // unnamed namespace

NodeInfo MakeNode() {
  NodeInfo node;
  node.node_id = NodeId(RandomString(64));
  asymm::Keys keys(asymm::GenerateKeyPair());
  node.public_key = keys.public_key;
  node.connection_id = node.node_id;
  return node;
}

NodeInfoAndPrivateKey MakeNodeInfoAndKeys() {
  passport::Pmid pmid(MakePmid());
  return MakeNodeInfoAndKeysWithFob(pmid);
}

NodeInfoAndPrivateKey MakeNodeInfoAndKeysWithPmid(passport::Pmid pmid) {
  return MakeNodeInfoAndKeysWithFob(pmid);
}

NodeInfoAndPrivateKey MakeNodeInfoAndKeysWithMaid(passport::Maid maid) {
  return MakeNodeInfoAndKeysWithFob(maid);
}

passport::Maid MakeMaid() {
  passport::Anmaid anmaid;
  return passport::Maid(anmaid);
}

passport::Pmid MakePmid() { return passport::Pmid(MakeMaid()); }

/* Fob GetFob(const NodeInfoAndPrivateKey& node) {
  Fob fob;
  fob.identity = Identity(node.node_info.node_id.string());
  fob.keys.public_key = node.node_info.public_key;
  fob.keys.private_key = node.private_key;
  return fob;
} */

NodeId GenerateUniqueRandomId(const NodeId& holder, const uint16_t& pos) {
  std::string holder_id = holder.ToStringEncoded(NodeId::EncodingType::kBinary);
  std::bitset<64 * 8> holder_id_binary_bitset(holder_id);
  NodeId new_node;
  std::string new_node_string;
  // generate a random ID and make sure it has not been generated previously
  while (new_node_string == "" || new_node_string == holder_id) {
    new_node = NodeId(NodeId::kRandomId);
    std::string new_id = new_node.ToStringEncoded(NodeId::EncodingType::kBinary);
    std::bitset<64 * 8> binary_bitset(new_id);
    for (uint16_t i(0); i < pos; ++i)
      holder_id_binary_bitset[i] = binary_bitset[i];
    new_node_string = holder_id_binary_bitset.to_string();
    if (pos == 0)
      break;
  }
  new_node = NodeId(new_node_string, NodeId::EncodingType::kBinary);
  return new_node;
}

NodeId GenerateUniqueNonRandomId(const NodeId& holder, const uint64_t& id) {
  std::string holder_id = holder.ToStringEncoded(NodeId::EncodingType::kBinary);
  std::bitset<64 * 8> holder_id_binary_bitset(holder_id);
  NodeId new_node;
  std::string new_node_string;
  // generate a random ID and make sure it has not been generated previously
  new_node = NodeId(NodeId::kRandomId);
  std::string new_id = new_node.ToStringEncoded(NodeId::EncodingType::kBinary);
  std::bitset<64> binary_bitset(id);
  for (uint16_t i(0); i < 64; ++i)
    holder_id_binary_bitset[i] = binary_bitset[i];
  new_node_string = holder_id_binary_bitset.to_string();
  new_node = NodeId(new_node_string, NodeId::EncodingType::kBinary);
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
  uint16_t max_size(client ? Parameters::max_routing_table_size_for_client
                           : Parameters::max_routing_table_size);
  return (status > 0) ? (status * 100 / max_size) : status;
}

void SortFromTarget(const NodeId& target, std::vector<NodeInfo>& nodes) {
  std::sort(nodes.begin(), nodes.end(), [target](const NodeInfo & lhs, const NodeInfo & rhs) {
    return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target);
  });
}

void PartialSortFromTarget(const NodeId& target, std::vector<NodeInfo>& nodes, size_t num_to_sort) {
  assert(num_to_sort <= nodes.size());
  std::partial_sort(nodes.begin(), nodes.begin() + num_to_sort, nodes.end(),
                    [target](const NodeInfo & lhs, const NodeInfo & rhs) {
    return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target);
  });
}

void SortIdsFromTarget(const NodeId& target, std::vector<NodeId>& nodes) {
  std::sort(nodes.begin(), nodes.end(), [target](const NodeId & lhs, const NodeId & rhs) {
    return NodeId::CloserToTarget(lhs, rhs, target);
  });
}

void SortNodeInfosFromTarget(const NodeId& target, std::vector<NodeInfo>& nodes) {
  std::sort(nodes.begin(), nodes.end(), [target](const NodeInfo & lhs, const NodeInfo & rhs) {
    return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target);
  });
}

bool CompareListOfNodeInfos(const std::vector<NodeInfo>& lhs, const std::vector<NodeInfo>& rhs) {
  if (lhs.size() != rhs.size())
    return false;
  for (const auto& node_info : lhs) {
    if (std::find_if(rhs.begin(), rhs.end(), [&](const NodeInfo & node) {
          return node.node_id == node_info.node_id;
        }) == rhs.end())
      return false;
  }

  for (const auto& node_info : rhs) {
    if (std::find_if(lhs.begin(), lhs.end(), [&](const NodeInfo & node) {
          return node.node_id == node_info.node_id;
        }) == lhs.end())
      return false;
  }
  return true;
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
