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

#include <memory>
#include <vector>

#include "maidsafe/common/log.h"
#include "maidsafe/common/Address.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/make_unique.h"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/tests/utils/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST(routing_table_test, FUNC_add_many_nodes_check_close_groups) {
  const auto network_size(500);
  auto routing_tables(routing_table_network(network_size));
  std::vector<Address> Addresss;
  Addresss.reserve(network_size);
  asymm::Keys key(asymm::GenerateKeyPair());
  // iterate and try to add each node to each other node
  for (auto& node : routing_tables) {
    Addresss.push_back(node->our_id());
    for (const auto& node_to_add : routing_tables) {
      node_info nodeinfo_to_add;
      nodeinfo_to_add.id = node_to_add->our_id();
      nodeinfo_to_add.public_key = key.public_key;
      node->add_node(nodeinfo_to_add);
    }
  }
  for (const auto& node : routing_tables) {
    auto id = node->our_id();
    // + 1 as Addresss includes our ID
    std::partial_sort(std::begin(Addresss), std::begin(node_ids) + group_size + 1,
                      std::end(Addresss), [id](const Address& lhs, const Address& rhs) {
      return Address::CloserToTarget(lhs, rhs, id);
    });
    auto groups = node->our_close_group();
    EXPECT_EQ(groups.size(), group_size);
    auto last = std::unique(std::begin(groups), std::end(groups));
    ASSERT_EQ(last, std::end(groups));
    groups.erase(last, std::end(groups));
    EXPECT_EQ(groups.size(), group_size);
    for (size_t i = 0; i < group_size; ++i) {
      // + 1 as Addresss includes our ID
      EXPECT_EQ(groups.at(i).id, Addresss.at(i + 1)) << " node mismatch at " << i;
    }
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
