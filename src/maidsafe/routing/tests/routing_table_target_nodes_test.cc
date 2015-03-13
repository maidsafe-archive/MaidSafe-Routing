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

TEST_F(RoutingTableUnitTest, BEH_TargetNodes) {
  // Check on empty table
  auto target_nodes = table_.TargetNodes(MakeIdentity());
  EXPECT_TRUE(target_nodes.empty());

  // Partially fill the table with < GroupSize contacts
  PartiallyFillTable();

  // Check we get all contacts returned
  target_nodes = table_.TargetNodes(MakeIdentity());
  EXPECT_EQ(initial_count_, target_nodes.size());
  for (size_t i = 0; i < initial_count_; ++i) {
    EXPECT_TRUE(
        std::any_of(std::begin(target_nodes), std::end(target_nodes),
                    [&](const NodeInfo& node) { return node.id == buckets_[i].mid_contact; }));
  }

  // Complete filling the table up to RoutingTable::OptimalSize() contacts
  CompleteFillingTable();

#ifdef NDEBUG
  // Try with invalid Address
  EXPECT_THROW(table_.TargetNodes(Address{}), common_error);
#endif

  // Try with our ID (should return closest to us, i.e. buckets 63 to 32)
  target_nodes = table_.TargetNodes(table_.OurId());
  EXPECT_EQ(GroupSize, target_nodes.size());
  for (size_t i = RoutingTable::OptimalSize() - 1; i > RoutingTable::OptimalSize() - 1 - GroupSize;
       --i) {
    EXPECT_TRUE(
        std::any_of(std::begin(target_nodes), std::end(target_nodes),
                    [&](const NodeInfo& node) { return node.id == buckets_[i].mid_contact; }));
  }

  // Try with nodes far from us, first time *not* in table and second time *in* table (should return
  // 'RoutingTable::Parallelism()' contacts closest to target)
  Address target;
  for (int count = 0; count < 2; ++count) {
    for (size_t i = 0; i < RoutingTable::OptimalSize() - GroupSize; ++i) {
      target = (count == 0) ? buckets_[i].far_contact : buckets_[i].mid_contact;
      target_nodes = table_.TargetNodes(target);
      EXPECT_EQ(RoutingTable::Parallelism(), target_nodes.size());
      std::partial_sort(std::begin(added_ids_),
                        std::begin(added_ids_) + RoutingTable::Parallelism(), std::end(added_ids_),
                        [&](const Address& lhs, const Address& rhs) {
        return CloserToTarget(lhs, rhs, target);
      });
      for (const auto& target_node : target_nodes) {
        EXPECT_TRUE(std::any_of(
            std::begin(added_ids_), std::begin(added_ids_) + RoutingTable::Parallelism(),
            [&](const Address& added_id) { return added_id == target_node.id; }));
      }
    }
  }

  // Try with nodes close to us, first time *not* in table and second time *in* table (should return
  // GroupSize closest to target)
  for (int count = 0; count < 2; ++count) {
    for (size_t i = RoutingTable::OptimalSize() - GroupSize; i < RoutingTable::OptimalSize(); ++i) {
      target = (count == 0) ? buckets_[i].far_contact : buckets_[i].mid_contact;
      target_nodes = table_.TargetNodes(target);
      EXPECT_EQ(GroupSize, target_nodes.size());
      std::partial_sort(std::begin(added_ids_), std::begin(added_ids_) + GroupSize,
                        std::end(added_ids_), [&](const Address& lhs, const Address& rhs) {
        return CloserToTarget(lhs, rhs, target);
      });

      for (const auto& target_node : target_nodes) {
        EXPECT_TRUE(
            std::any_of(std::begin(added_ids_), std::begin(added_ids_) + GroupSize,
                        [&](const Address& added_id) { return added_id == target_node.id; }));
      }
    }
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
