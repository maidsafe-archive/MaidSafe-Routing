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

TEST(RoutingTableTest, FUNC_AddManyNodesCheckCloseGroups) {
  const auto network_size(500);
  auto routing_tables(RoutingTableNetwork(network_size));
  std::vector<Address> addresses;
  addresses.reserve(network_size);
  passport::PublicPmid fob{passport::Pmid(passport::Anpmid())};
  // iterate and try to add each node to each other node
  for (auto& node : routing_tables) {
    addresses.push_back(node->OurId());
    for (const auto& node_to_add : routing_tables) {
      NodeInfo nodeinfo_to_add{node_to_add->OurId(), fob};
      node->AddNode(nodeinfo_to_add);
    }
  }
  for (const auto& node : routing_tables) {
    auto id = node->OurId();
    // + 1 as Addresss includes our ID
    std::partial_sort(std::begin(addresses), std::begin(addresses) + GroupSize + 1,
                      std::end(addresses), [id](const Address& lhs, const Address& rhs) {
      return Address::CloserToTarget(lhs, rhs, id);
    });
    auto groups = node->OurCloseGroup();
    EXPECT_EQ(groups.size(), GroupSize);
    auto last = std::unique(std::begin(groups), std::end(groups));
    ASSERT_EQ(last, std::end(groups));
    groups.erase(last, std::end(groups));
    EXPECT_EQ(groups.size(), GroupSize);
    for (size_t i = 0; i < GroupSize; ++i) {
      // + 1 as Addresss includes our ID
      EXPECT_EQ(groups.at(i).id, addresses.at(i + 1)) << " node mismatch at " << i;
    }
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
