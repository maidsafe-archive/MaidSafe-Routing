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

#include <vector>

#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/passport/types.h"
#include "maidsafe/passport/passport.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/tests/utils/test_utils.h"


namespace maidsafe {

namespace routing {

namespace test {

TEST(RoutingTableTest, FUNC_AddCheckMultipleNodes) {
  const auto size(50);
  auto routing_tables(RoutingTableNetwork(size));
  passport::PublicPmid fob{passport::Pmid(passport::Anpmid())};
  // iterate and try to add each node to each other node
  for (auto& node : routing_tables) {
    for (const auto& node_to_add : routing_tables) {
      NodeInfo nodeinfo_to_add(node_to_add->OurId(), fob);
      if (node->CheckNode(nodeinfo_to_add.id)) {
        auto removed_node = node->AddNode(nodeinfo_to_add);
        EXPECT_TRUE(removed_node.first);
      }
    }
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
