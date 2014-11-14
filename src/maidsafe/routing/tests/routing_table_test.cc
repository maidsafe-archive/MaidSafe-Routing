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

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/tests/main/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST(routing_table_test, BEH_add_close_nodes) {
  NodeId node_id(RandomString(NodeId::kSize));
  const auto keys = asymm::GenerateKeyPair();
  routing_table routing_table(node_id, keys);
  node_info node;
  // check the node is useful when false is set
  for (unsigned int i = 0; i < group_size; ++i) {
    node.id = NodeId(RandomString(NodeId::kSize));
    EXPECT_TRUE(routing_table.check_node(node.id));
  }
  EXPECT_EQ(0, routing_table.size());
  // everything should be set to go now
  for (unsigned int i = 0; i < group_size; ++i) {
    node.id = NodeId(RandomString(NodeId::kSize));
    node.public_key = keys.public_key;
    EXPECT_TRUE(routing_table.add_node(node).first);
  }
  EXPECT_EQ(group_size, routing_table.size());
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
