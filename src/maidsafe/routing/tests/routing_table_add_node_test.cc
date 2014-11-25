/*  Copyright 2014 MaidSafe.net limited

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

#include "maidsafe/routing/routing_table.h"

#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/node_info.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST(routing_table_test, BEH_add_node) {
  routing_table table{NodeId{RandomString(NodeId::kSize)}};
  const NodeId their_id{RandomString(NodeId::kSize)};
  const asymm::Keys their_keys{asymm::GenerateKeyPair()};

  // Try with invalid NodeId
  node_info their_info;
  their_info.public_key = their_keys.public_key;
  auto result_of_add = table.add_node(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(0, table.size());

  // Try with invalid public key
  their_info.id = their_id;
  their_info.public_key = asymm::PublicKey();
  result_of_add = table.add_node(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(0, table.size());

  // Try with our ID
  their_info.id = table.our_id();
  their_info.public_key = their_keys.public_key;
  result_of_add = table.add_node(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(0, table.size());


  GTEST_FAIL();  // complete test
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
