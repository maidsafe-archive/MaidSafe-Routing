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

TEST_F(RoutingTableUnitTest, BEH_TrivialFunctions) {  // 'GetPublicKey', 'OurId', and 'Size'
  // Check on empty table
  EXPECT_FALSE(table_.GetPublicKey(buckets_[0].mid_contact));
  EXPECT_EQ(our_id_, table_.OurId());
  EXPECT_EQ(0, table_.Size());

  // Check on partially filled the table
  PartiallyFillTable();
  auto test_id = Address{RandomString(Address::kSize)};
  info_.id = test_id;
  auto keys = asymm::GenerateKeyPair();
  info_.dht_fob = PublicFob();
  ASSERT_TRUE(table_.AddNode(info_).first);

  ASSERT_TRUE(!!table_.GetPublicKey(info_.id));
  EXPECT_TRUE(asymm::MatchingKeys(info_.dht_fob->public_key(), *table_.GetPublicKey(info_.id)));
  EXPECT_FALSE(table_.GetPublicKey(buckets_.back().far_contact));
  EXPECT_EQ(our_id_, table_.OurId());
  EXPECT_EQ(initial_count_ + 1, table_.Size());

  // Check on fully filled the table
  table_.DropNode(test_id);
  CompleteFillingTable();
  table_.DropNode(buckets_[0].mid_contact);
  info_.id = test_id;
  ASSERT_TRUE(table_.AddNode(info_).first);

  ASSERT_TRUE(!!table_.GetPublicKey(info_.id));
  EXPECT_TRUE(asymm::MatchingKeys(info_.dht_fob->public_key(), *table_.GetPublicKey(info_.id)));
  EXPECT_FALSE(table_.GetPublicKey(buckets_.back().far_contact));
  EXPECT_EQ(our_id_, table_.OurId());
  EXPECT_EQ(RoutingTable::OptimalSize(), table_.Size());

#ifdef NDEBUG
  // Try with invalid Address
  EXPECT_THROW(table_.GetPublicKey(Address{}), common_error);
#endif
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
