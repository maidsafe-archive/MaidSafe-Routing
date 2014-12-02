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

#include "maidsafe/common/rsa.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/tests/utils/test_utils.h"


namespace maidsafe {

namespace routing {

namespace test {

TEST(RoutingTableTest, FUNC_add_many_nodes_check_target) {
  const auto network_size(500);
  auto routing_tables(RoutingTableNetwork(network_size));
  asymm::Keys key(asymm::GenerateKeyPair());
  std::vector<Address> addresses;
  addresses.reserve(network_size);
  // iterate and try to add each node to each other node
  for (auto& node : routing_tables) {
    addresses.push_back(node->OurId());
    for (const auto& node_to_add : routing_tables) {
      NodeInfo nodeinfo_to_add;
      nodeinfo_to_add.id = node_to_add->OurId();
      nodeinfo_to_add.public_key = key.public_key;
      node->AddNode(nodeinfo_to_add);
    }
  }

  for (const auto& node : routing_tables) {
    std::sort(std::begin(addresses), std::end(addresses),
              [&node](const Address& lhs, const Address& rhs) {
      return Address::CloserToTarget(lhs, rhs, node->OurId());
    });
    // if target is in close group return the whole close group
    for (size_t i = 1; i < kGroupSize + 1; ++i) {
      auto target_close_group = node->TargetNodes(addresses.at(i));
      // check the close group is correct
      for (size_t j = 0; j < kGroupSize; ++j) {
        EXPECT_EQ(target_close_group.at(j).id, addresses.at(j + 1)) << " node mismatch at " << j;
        EXPECT_EQ(kGroupSize, (node->TargetNodes(addresses.at(j + 1))).size())
            << "mismatch at index " << j;
      }
    }

    // nodes further than the close group, should return a single target
    // as some nodes can be close the the end of the close group and the
    // tested node then we need to put in place a buffer. This magic number is
    // selected to be way past any chance of closeness to an colse group member
    // but not so far as to not check any of the return values being == 1
    // so magic number but for the best reasons we can think of.
    auto xor_closeness_buffer(10);
    for (size_t i = kGroupSize + xor_closeness_buffer; i < network_size - 1; ++i) {
      EXPECT_EQ(1, (node->TargetNodes(addresses.at(i))).size()) << "mismatch at index " << i;
    }
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
