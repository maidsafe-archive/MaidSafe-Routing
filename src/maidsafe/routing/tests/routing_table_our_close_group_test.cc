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

#include <algorithm>

#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/tests/utils/routing_table_unit_test.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST_F(RoutingTableUnitTest, BEH_OurCloseGroup) {
  // Check on empty table
  auto our_close_group = table_.OurCloseGroup();
  EXPECT_TRUE(our_close_group.empty());

  // Partially fill the table with < GroupSize contacts
  PartiallyFillTable();

  // Check we get all contacts returned
  our_close_group = table_.OurCloseGroup();
  EXPECT_EQ(initial_count_, our_close_group.size());
  for (size_t i = 0; i < initial_count_; ++i) {
    EXPECT_TRUE(
        std::any_of(std::begin(our_close_group), std::end(our_close_group),
                    [&](const NodeInfo& node) { return node.id == buckets_[i].mid_contact; }));
  }

  // Complete filling the table up to RoutingTable::OptimalSize() contacts and test again
  CompleteFillingTable();
  our_close_group = table_.OurCloseGroup();
  EXPECT_EQ(GroupSize, our_close_group.size());
  std::partial_sort(std::begin(added_ids_), std::begin(added_ids_) + GroupSize,
                    std::end(added_ids_), [&](const Address& lhs, const Address& rhs) {
    return CloserToTarget(lhs, rhs, table_.OurId());
  });
  for (const auto& node : our_close_group) {
    EXPECT_TRUE(std::any_of(std::begin(added_ids_), std::begin(added_ids_) + GroupSize,
                            [&](const Address& added_id) { return added_id == node.id; }));
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
