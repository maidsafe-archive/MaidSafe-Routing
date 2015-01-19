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

#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/tests/utils/test_utils.h"


namespace maidsafe {

namespace routing {

namespace test {

TEST(RoutingTableTest, FUNC_AddManyNodesCheckChurn) {
  const auto network_size(500);
  auto nodes_to_remove(50);

  asymm::Keys key(asymm::GenerateKeyPair());
  auto routing_tables(RoutingTableNetwork(network_size));
  std::vector<Address> addresses;
  addresses.reserve(network_size);
  auto fob = PublicFob();
  // iterate and try to add each node to each other node
  for (auto& node : routing_tables) {
    addresses.push_back(node->OurId());
    for (const auto& node_to_add : routing_tables) {
      NodeInfo nodeinfo_to_add(node_to_add->OurId(), fob);
      node->AddNode(nodeinfo_to_add);
    }
  }
  // now remove nodes
  std::vector<Address> drop_vec;
  drop_vec.reserve(nodes_to_remove);
  std::copy(std::begin(addresses), std::begin(addresses) + nodes_to_remove,
            std::back_inserter(drop_vec));
  routing_tables.erase(std::begin(routing_tables), std::begin(routing_tables) + nodes_to_remove);

  for (auto& node : routing_tables) {
    for (const auto& drop : drop_vec)
      node->DropNode(drop);
  }
  // remove ids too
  addresses.erase(std::begin(addresses), std::begin(addresses) + nodes_to_remove);

  for (const auto& node : routing_tables) {
    size_t size = std::min(GroupSize, static_cast<size_t>(node->Size()));
    auto id = node->OurId();
    // + 1 as addresses includes our ID
    std::partial_sort(std::begin(addresses), std::begin(addresses) + size + 1, std::end(addresses),
                      [id](const Address& lhs,
                           const Address& rhs) { return Address::CloserToTarget(lhs, rhs, id); });
    auto groups = node->OurCloseGroup();
    EXPECT_EQ(groups.size(), size);
    // currently disabled as nodes are not doing a get_close_group to begin and this
    // causes issues with tracking routing tables as the close group attraction for
    // routing tables means this test network is not currently as th enetwork shoudl be
    // FIXME(dirvine) Create a closer to reality netwokr join  :23/11/2014
    // size = std::min(quorum_size, static_cast<size_t>(node->size()));
    // for (size_t i = 0; i < size; ++i) {
    //   // + 1 as addresses includes our ID
    //   EXPECT_EQ(groups.at(i).id, addresses.at(i + 1)) << " node mismatch at " << i;
    // }
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
