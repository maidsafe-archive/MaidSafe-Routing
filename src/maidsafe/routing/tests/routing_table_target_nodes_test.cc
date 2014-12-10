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
  auto target_nodes = table_.TargetNodes(Address{RandomString(Address::kSize)});
  EXPECT_TRUE(target_nodes.empty());

  // Partially fill the table with < kGroupSize contacts
  const size_t initial_count = (RandomUint32() % (kGroupSize - 1)) + 1;
  NodeInfo info;
  const asymm::Keys keys{asymm::GenerateKeyPair()};
  info.public_key = keys.public_key;
  std::vector<Address> added_ids;
  for (size_t i = 0; i < initial_count; ++i) {
    info.id = buckets_[i].mid_contact;
    added_ids.push_back(info.id);
    ASSERT_TRUE(table_.AddNode(info).first);
  }
  ASSERT_EQ(initial_count, table_.Size());

  // Check we get all contacts returned
  target_nodes = table_.TargetNodes(Address{RandomString(Address::kSize)});
  EXPECT_EQ(initial_count, target_nodes.size());
  for (size_t i = 0; i < initial_count; ++i) {
    EXPECT_TRUE(
        std::any_of(std::begin(target_nodes), std::end(target_nodes),
                    [&](const NodeInfo& node) { return node.id == buckets_[i].mid_contact; }));
  }

  // Complete filling the table
  for (size_t i = initial_count; i < RoutingTable::OptimalSize(); ++i) {
    info.id = buckets_[i].mid_contact;
    added_ids.push_back(info.id);
    ASSERT_TRUE(table_.AddNode(info).first);
  }
  ASSERT_EQ(RoutingTable::OptimalSize(), table_.Size());

#ifdef NDEBUG
  // Try with invalid Address
  EXPECT_THROW(table_.TargetNodes(Address{}), common_error);
#endif

  // Try with our ID (should return closest to us, i.e. buckets 63 to 32)
  target_nodes = table_.TargetNodes(table_.OurId());
  EXPECT_EQ(kGroupSize, target_nodes.size());
  for (size_t i = RoutingTable::OptimalSize() - 1; i > RoutingTable::OptimalSize() - 1 - kGroupSize;
       --i) {
    EXPECT_TRUE(
        std::any_of(std::begin(target_nodes), std::end(target_nodes),
                    [&](const NodeInfo& node) { return node.id == buckets_[i].mid_contact; }));
  }

  // Try with nodes far from us *not* in table (should return 'RoutingTable::Parallelism()' contacts
  // closest to target)
  for (size_t i = 0; i < RoutingTable::OptimalSize() - kGroupSize; ++i) {
    target_nodes = table_.TargetNodes(buckets_[i].far_contact);
    EXPECT_EQ(RoutingTable::Parallelism(), target_nodes.size());
    std::partial_sort(std::begin(added_ids), std::begin(added_ids) + RoutingTable::Parallelism(),
                      std::end(added_ids), [&](const Address& lhs, const Address& rhs) {
      return Address::CloserToTarget(lhs, rhs, buckets_[i].far_contact);
    });
    for (const auto& target_node : target_nodes) {
      EXPECT_TRUE(std::any_of(std::begin(added_ids),
                              std::begin(added_ids) + RoutingTable::Parallelism(),
                              [&](const Address& added_id) { return added_id == target_node.id; }));
    }
  }

  // Try with node far from us, but *in* table (should return 'RoutingTable::Parallelism()' contacts
  // closest to target excluding target itself)
  for (size_t i = 0; i < RoutingTable::OptimalSize() - kGroupSize; ++i) {
    target_nodes = table_.TargetNodes(buckets_[i].mid_contact);
    EXPECT_EQ(RoutingTable::Parallelism(), target_nodes.size());
    std::partial_sort(std::begin(added_ids),
                      std::begin(added_ids) + RoutingTable::Parallelism() + 1, std::end(added_ids),
                      [&](const Address& lhs, const Address& rhs) {
      return Address::CloserToTarget(lhs, rhs, buckets_[i].mid_contact);
    });
    for (const auto& target_node : target_nodes) {
      EXPECT_TRUE(std::any_of(std::begin(added_ids) + 1,
                              std::begin(added_ids) + RoutingTable::Parallelism() + 1,
                              [&](const Address& added_id) { return added_id == target_node.id; }));
    }
  }

  // Try with node close to us *not* in table (should return kGroupSize closest to target)
  for (size_t i = RoutingTable::OptimalSize() - kGroupSize; i < RoutingTable::OptimalSize(); ++i) {
    target_nodes = table_.TargetNodes(buckets_[i].far_contact);
    EXPECT_EQ(kGroupSize, target_nodes.size());
    std::partial_sort(std::begin(added_ids), std::begin(added_ids) + kGroupSize,
                      std::end(added_ids), [&](const Address& lhs, const Address& rhs) {
      return Address::CloserToTarget(lhs, rhs, buckets_[i].far_contact);
    });

    for (const auto& target_node : target_nodes) {
      EXPECT_TRUE(std::any_of(std::begin(added_ids), std::begin(added_ids) + kGroupSize,
                              [&](const Address& added_id) { return added_id == target_node.id; }));
    }
  }

  // Try with node close to us, but *in* table (should return kGroupSize closest to target excluding
  // target itself)
  for (size_t i = RoutingTable::OptimalSize() - kGroupSize; i < RoutingTable::OptimalSize(); ++i) {
    target_nodes = table_.TargetNodes(buckets_[i].mid_contact);
    EXPECT_EQ(kGroupSize, target_nodes.size());
    std::partial_sort(std::begin(added_ids), std::begin(added_ids) + kGroupSize + 1,
                      std::end(added_ids), [&](const Address& lhs, const Address& rhs) {
      return Address::CloserToTarget(lhs, rhs, buckets_[i].mid_contact);
    });
    for (const auto& target_node : target_nodes) {
      EXPECT_TRUE(std::any_of(std::begin(added_ids) + 1, std::begin(added_ids) + kGroupSize + 1,
                              [&](const Address& added_id) { return added_id == target_node.id; }));
    }
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
