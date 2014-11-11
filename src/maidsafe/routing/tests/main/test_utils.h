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

#ifndef MAIDSAFE_ROUTING_TESTS_MAIN_TEST_UTILS_H_
#define MAIDSAFE_ROUTING_TESTS_MAIN_TEST_UTILS_H_

#include <cstdint>
#include <vector>
#include <string>

#include "boost/asio/ip/address.hpp"
#include "boost/asio/ip/udp.hpp"
#include "boost/filesystem/path.hpp"

#include "maidsafe/common/rsa.h"

#include "maidsafe/passport/types.h"

#include "maidsafe/routing/bootstrap_file_operations.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/utils.h"


namespace maidsafe {

namespace routing {

namespace test {

struct ScopedBootstrapFile {
  explicit ScopedBootstrapFile(const BootstrapContacts& bootstrap_contacts);
  ~ScopedBootstrapFile();

 private:
  const boost::filesystem::path kFilePath;
};

struct node_infoAndPrivateKey {
  node_infoAndPrivateKey() : node_info(), private_key() {}
  node_infoAndPrivateKey(const node_infoAndPrivateKey& info)
      : node_info(info.node_info), private_key(info.private_key) {}
  node_info node_info;
  asymm::PrivateKey private_key;
};

template <typename FobType>
node_infoAndPrivateKey Makenode_infoAndKeysWithFob(FobType fob) {
  node_info node;
  node.id = NodeId(fob.name()->string());
  node.public_key = fob.public_key();
  node_infoAndPrivateKey node_info_and_private_key;
  node_info_and_private_key.node_info = node;
  node_info_and_private_key.private_key = fob.private_key();
  return node_info_and_private_key;
}

node_infoAndPrivateKey Makenode_infoAndKeys();
node_infoAndPrivateKey Makenode_infoAndKeysWithPmid(passport::Pmid pmid);
node_infoAndPrivateKey Makenode_infoAndKeysWithMaid(passport::Maid maid);

node_info MakeNode();

NodeId GenerateUniqueRandomId(const NodeId& holder, unsigned int pos);
NodeId GenerateUniqueRandomId(unsigned int pos);
NodeId GenerateUniqueRandomNodeId(const std::vector<NodeId>& esisting_ids);

int NetworkStatus(bool client, int status);

void SortFromTarget(const NodeId& target, std::vector<node_info>& nodes);

void PartialSortFromTarget(const NodeId& target, std::vector<node_info>& nodes, size_t num_to_sort);

void SortIdsFromTarget(const NodeId& target, std::vector<NodeId>& nodes);

void Sortnode_infosFromTarget(const NodeId& target, std::vector<node_info>& nodes);

bool CompareListOfnode_infos(const std::vector<node_info>& lhs, const std::vector<node_info>& rhs);

std::vector<std::unique_ptr<routing_table>> routing_table_network(size_t size);

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TESTS_MAIN_TEST_UTILS_H_
