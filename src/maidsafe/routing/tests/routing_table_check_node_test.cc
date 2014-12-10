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

TEST_F(RoutingTableUnitTest, BEH_CheckNode) {
#ifdef NDEBUG
  // Try with invalid Address
  EXPECT_THROW(table_.CheckNode(Address{}), common_error);
#endif

  // Try with our ID
  EXPECT_FALSE(table_.CheckNode(table_.OurId()));

  // Should return true for empty routing table
  EXPECT_TRUE(table_.CheckNode(buckets_[0].far_contact));

  // Add the first contact, and check it doesn't allow duplicates
  NodeInfo info;
  info.id = buckets_[0].far_contact;
  const asymm::Keys keys{asymm::GenerateKeyPair()};
  info.public_key = keys.public_key;
  ASSERT_TRUE(table_.AddNode(info).first);
  EXPECT_FALSE(table_.CheckNode(buckets_[0].far_contact));

  // Add further 'OptimalSize()' - 1 contacts (should all succeed with no removals).  Set this up so
  // that bucket 0 (furthest) and bucket 1 have 3 contacts each and all others have 0 or 1 contacts.
  info.id = buckets_[0].mid_contact;
  EXPECT_TRUE(table_.CheckNode(info.id));
  ASSERT_TRUE(table_.AddNode(info).first);
  EXPECT_FALSE(table_.CheckNode(info.id));

  info.id = buckets_[0].close_contact;
  EXPECT_TRUE(table_.CheckNode(info.id));
  ASSERT_TRUE(table_.AddNode(info).first);
  EXPECT_FALSE(table_.CheckNode(info.id));

  info.id = buckets_[1].far_contact;
  EXPECT_TRUE(table_.CheckNode(info.id));
  ASSERT_TRUE(table_.AddNode(info).first);
  EXPECT_FALSE(table_.CheckNode(info.id));

  info.id = buckets_[1].mid_contact;
  EXPECT_TRUE(table_.CheckNode(info.id));
  ASSERT_TRUE(table_.AddNode(info).first);
  EXPECT_FALSE(table_.CheckNode(info.id));

  info.id = buckets_[1].close_contact;
  EXPECT_TRUE(table_.CheckNode(info.id));
  ASSERT_TRUE(table_.AddNode(info).first);
  EXPECT_FALSE(table_.CheckNode(info.id));

  for (size_t i = 2; i < RoutingTable::OptimalSize() - 4; ++i) {
    info.id = buckets_[i].mid_contact;
    EXPECT_TRUE(table_.CheckNode(info.id));
    ASSERT_TRUE(table_.AddNode(info).first);
    EXPECT_FALSE(table_.CheckNode(info.id));
  }

  // Check the table's full
  ASSERT_EQ(RoutingTable::OptimalSize(), table_.Size());

  // Check next 4 closer additions return true.  Add each of these which will casue
  // 'buckets_[0].far_contact', 'buckets_[0].mid_contact', 'buckets_[1].far_contact', and
  // 'buckets_[1].mid_contact' to be dropped
  for (size_t i = RoutingTable::OptimalSize() - 4; i < RoutingTable::OptimalSize(); ++i) {
    info.id = buckets_[i].mid_contact;
    EXPECT_TRUE(table_.CheckNode(info.id));
    ASSERT_TRUE(table_.AddNode(info).first);
    EXPECT_FALSE(table_.CheckNode(info.id));
    ASSERT_EQ(RoutingTable::OptimalSize(), table_.Size());
  }

  // Check far contacts again which are now not in the table
  EXPECT_FALSE(table_.CheckNode(buckets_[0].far_contact));
  EXPECT_FALSE(table_.CheckNode(buckets_[0].mid_contact));
  EXPECT_FALSE(table_.CheckNode(buckets_[1].far_contact));
  EXPECT_FALSE(table_.CheckNode(buckets_[1].mid_contact));

  // Check final close contact which would push size of table_ above OptimalSize()
  EXPECT_TRUE(table_.CheckNode(buckets_[RoutingTable::OptimalSize()].mid_contact));
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
