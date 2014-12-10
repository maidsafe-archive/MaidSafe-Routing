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

#include "maidsafe/routing/tests/utils/routing_table_unit_test.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST_F(RoutingTableUnitTest, BEH_AddNode) {
  const asymm::Keys their_keys{asymm::GenerateKeyPair()};
  NodeInfo their_info;
  their_info.public_key = their_keys.public_key;

#ifdef NDEBUG
  // Try with invalid Address (should throw)
  EXPECT_THROW(table_.AddNode(their_info), common_error);
  EXPECT_EQ(0, table_.Size());
#endif

  // Try with invalid public key (should fail)
  their_info.id = buckets_[0].far_contact;
  their_info.public_key = asymm::PublicKey();
  auto result_of_add = table_.AddNode(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(0, table_.Size());

  // Try with our ID (should fail)
  their_info.id = table_.OurId();
  their_info.public_key = their_keys.public_key;
  result_of_add = table_.AddNode(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(0, table_.Size());

  // Add first contact
  their_info.id = buckets_[0].far_contact;
  result_of_add = table_.AddNode(their_info);
  EXPECT_TRUE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(1U, table_.Size());

  // Try with the same contact (should fail)
  result_of_add = table_.AddNode(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(1U, table_.Size());

  // Add further 'OptimalSize()' - 1 contacts (should all succeed with no removals).  Set this up so
  // that bucket 0 (furthest) and bucket 1 have 3 contacts each and all others have 0 or 1 contacts.

  // Bucket 0
  their_info.id = buckets_[0].mid_contact;
  result_of_add = table_.AddNode(their_info);
  EXPECT_TRUE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(2U, table_.Size());
  result_of_add = table_.AddNode(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(2U, table_.Size());

  their_info.id = buckets_[0].close_contact;
  result_of_add = table_.AddNode(their_info);
  EXPECT_TRUE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(3U, table_.Size());
  result_of_add = table_.AddNode(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(3U, table_.Size());

  // Bucket 1
  their_info.id = buckets_[1].far_contact;
  result_of_add = table_.AddNode(their_info);
  EXPECT_TRUE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(4U, table_.Size());
  result_of_add = table_.AddNode(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(4U, table_.Size());

  their_info.id = buckets_[1].mid_contact;
  result_of_add = table_.AddNode(their_info);
  EXPECT_TRUE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(5U, table_.Size());
  result_of_add = table_.AddNode(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(5U, table_.Size());

  their_info.id = buckets_[1].close_contact;
  result_of_add = table_.AddNode(their_info);
  EXPECT_TRUE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(6U, table_.Size());
  result_of_add = table_.AddNode(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(6U, table_.Size());

  // Add remaining contacts
  for (size_t i = 2; i < RoutingTable::OptimalSize() - 4; ++i) {
    their_info.id = buckets_[i].mid_contact;
    result_of_add = table_.AddNode(their_info);
    EXPECT_TRUE(result_of_add.first);
    EXPECT_FALSE(result_of_add.second.is_initialized());
    EXPECT_EQ(i + 5, table_.Size());
    result_of_add = table_.AddNode(their_info);
    EXPECT_FALSE(result_of_add.first);
    EXPECT_FALSE(result_of_add.second.is_initialized());
    EXPECT_EQ(i + 5, table_.Size());
  }

  // Check next 4 closer additions return 'buckets_[0].far_contact', 'buckets_[0].mid_contact',
  // 'buckets_[1].far_contact', and 'buckets_[1].mid_contact' as dropped (in that order)
  std::vector<Address> dropped;
  for (size_t i = RoutingTable::OptimalSize() - 4; i < RoutingTable::OptimalSize(); ++i) {
    their_info.id = buckets_[i].mid_contact;
    result_of_add = table_.AddNode(their_info);
    EXPECT_TRUE(result_of_add.first);
    ASSERT_TRUE(result_of_add.second.is_initialized());
    dropped.push_back(result_of_add.second.get().id);
    EXPECT_EQ(RoutingTable::OptimalSize(), table_.Size());
    result_of_add = table_.AddNode(their_info);
    EXPECT_FALSE(result_of_add.first);
    EXPECT_FALSE(result_of_add.second.is_initialized());
    EXPECT_EQ(RoutingTable::OptimalSize(), table_.Size());
  }
  EXPECT_EQ(buckets_[0].far_contact, dropped.at(0));
  EXPECT_EQ(buckets_[0].mid_contact, dropped.at(1));
  EXPECT_EQ(buckets_[1].far_contact, dropped.at(2));
  EXPECT_EQ(buckets_[1].mid_contact, dropped.at(3));

  // Try to add far contacts again (should fail)
  for (const auto& far_contact : dropped) {
    their_info.id = far_contact;
    result_of_add = table_.AddNode(their_info);
    EXPECT_FALSE(result_of_add.first);
    EXPECT_FALSE(result_of_add.second.is_initialized());
    EXPECT_EQ(RoutingTable::OptimalSize(), table_.Size());
  }

  // Add final close contact to push size of table_ above OptimalSize()
  their_info.id = buckets_[RoutingTable::OptimalSize()].mid_contact;
  result_of_add = table_.AddNode(their_info);
  EXPECT_TRUE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(RoutingTable::OptimalSize() + 1, table_.Size());
  result_of_add = table_.AddNode(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(RoutingTable::OptimalSize() + 1, table_.Size());
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
