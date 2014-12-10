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

#include <random>

#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/tests/utils/routing_table_unit_test.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST_F(RoutingTableUnitTest, BEH_DropNode) {
  // Check on empty table
  EXPECT_NO_THROW(table_.DropNode(buckets_[0].far_contact));
  EXPECT_EQ(0, table_.Size());

  // Fill the table
  NodeInfo info;
  const asymm::Keys keys{asymm::GenerateKeyPair()};
  info.public_key = keys.public_key;
  std::vector<Address> added_ids;
  for (size_t i = 0; i < RoutingTable::OptimalSize(); ++i) {
    info.id = buckets_[i].mid_contact;
    added_ids.push_back(info.id);
    ASSERT_TRUE(table_.AddNode(info).first);
  }
  ASSERT_EQ(RoutingTable::OptimalSize(), table_.Size());

#ifdef NDEBUG
  // Try with invalid Address
  EXPECT_THROW(table_.DropNode(Address{}), common_error);
  EXPECT_EQ(RoutingTable::OptimalSize(), table_.Size());
#endif

  // Try with our ID
  EXPECT_NO_THROW(table_.DropNode(table_.OurId()));
  EXPECT_EQ(RoutingTable::OptimalSize(), table_.Size());

  // Try with Address of node not in table
  EXPECT_NO_THROW(table_.DropNode(buckets_[0].far_contact));
  EXPECT_EQ(RoutingTable::OptimalSize(), table_.Size());

  // Remove all nodes one at a time
  std::mt19937 rng(RandomUint32());
  std::shuffle(std::begin(added_ids), std::end(added_ids), rng);
  auto size = table_.Size();
  for (const auto& id : added_ids) {
    EXPECT_NO_THROW(table_.DropNode(id));
    EXPECT_EQ(--size, table_.Size());
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
