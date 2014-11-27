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

#include "maidsafe/routing/tests/utils/routing_table_unit_test.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST_F(routing_table_unit_test, BEH_add_node) {
  // Set up 'their_id' as furthest from our ID.
  const NodeId their_id{table_.our_id() ^ NodeId(std::string(NodeId::kSize, -1))};
  const asymm::Keys their_keys{asymm::GenerateKeyPair()};

  // Try with invalid NodeId (should fail)
  node_info their_info;
  their_info.public_key = their_keys.public_key;
  auto result_of_add = table_.add_node(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(0, table_.size());

  // Try with invalid public key (should fail)
  their_info.id = their_id;
  their_info.public_key = asymm::PublicKey();
  result_of_add = table_.add_node(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(0, table_.size());

  // Try with our ID (should fail)
  their_info.id = table_.our_id();
  their_info.public_key = their_keys.public_key;
  result_of_add = table_.add_node(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(0, table_.size());

  // Add first contact
  their_info.id = their_id;
  result_of_add = table_.add_node(their_info);
  EXPECT_TRUE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(1U, table_.size());

  // Try with the same contact (should fail)
  result_of_add = table_.add_node(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(1U, table_.size());

  // Add further 'optimal_size' - 1 contacts (should all succeed with no removals).  Set this up so
  // that bucket 0 (furthest) and bucket 1 have 3 contacts each and all others have 0 or 1 contacts.

  // Bucket 0
  const NodeId bucket0_far_contact = their_id;
  std::bitset<NodeId::kSize * 8> id{their_id.ToStringEncoded(NodeId::EncodingType::kBinary)};

  id.flip(0);
  const NodeId bucket0_mid_contact = NodeId{id.to_string(), NodeId::EncodingType::kBinary};
  id.flip(0);
  their_info.id = bucket0_mid_contact;
  result_of_add = table_.add_node(their_info);
  EXPECT_TRUE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(2U, table_.size());
  result_of_add = table_.add_node(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(2U, table_.size());

  id.flip(1);
  const NodeId bucket0_close_contact = NodeId{id.to_string(), NodeId::EncodingType::kBinary};
  id.flip(1);
  their_info.id = bucket0_close_contact;
  result_of_add = table_.add_node(their_info);
  EXPECT_TRUE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(3U, table_.size());
  result_of_add = table_.add_node(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(3U, table_.size());

  // Bucket 1
  id.flip((NodeId::kSize * 8) - 1);
  const NodeId bucket1_far_contact = NodeId{id.to_string(), NodeId::EncodingType::kBinary};
  their_info.id = bucket1_far_contact;
  result_of_add = table_.add_node(their_info);
  EXPECT_TRUE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(4U, table_.size());
  result_of_add = table_.add_node(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(4U, table_.size());

  id.flip(0);
  const NodeId bucket1_mid_contact = NodeId{id.to_string(), NodeId::EncodingType::kBinary};
  id.flip(0);
  their_info.id = bucket1_mid_contact;
  result_of_add = table_.add_node(their_info);
  EXPECT_TRUE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(5U, table_.size());
  result_of_add = table_.add_node(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(5U, table_.size());

  id.flip(1);
  const NodeId bucket1_close_contact = NodeId{id.to_string(), NodeId::EncodingType::kBinary};
  id.flip(1);
  their_info.id = bucket1_close_contact;
  result_of_add = table_.add_node(their_info);
  EXPECT_TRUE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(6U, table_.size());
  result_of_add = table_.add_node(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(6U, table_.size());

  // Check test logic - that these six contacts are ordered as expected
  ASSERT_TRUE(NodeId::CloserToTarget(bucket0_mid_contact, bucket0_far_contact, table_.our_id()));
  ASSERT_TRUE(NodeId::CloserToTarget(bucket0_close_contact, bucket0_mid_contact, table_.our_id()));
  ASSERT_TRUE(NodeId::CloserToTarget(bucket1_far_contact, bucket0_close_contact, table_.our_id()));
  ASSERT_TRUE(NodeId::CloserToTarget(bucket1_mid_contact, bucket1_far_contact, table_.our_id()));
  ASSERT_TRUE(NodeId::CloserToTarget(bucket1_close_contact, bucket1_mid_contact, table_.our_id()));

  // Add remaining contacts
  for (size_t i = 2; i < routing_table::optimal_size - 4; ++i) {
    id.flip((NodeId::kSize * 8) - i);
    their_info.id = NodeId{ id.to_string(), NodeId::EncodingType::kBinary };
    result_of_add = table_.add_node(their_info);
    EXPECT_TRUE(result_of_add.first);
    EXPECT_FALSE(result_of_add.second.is_initialized());
    EXPECT_EQ(i + 5, table_.size());
    result_of_add = table_.add_node(their_info);
    EXPECT_FALSE(result_of_add.first);
    EXPECT_FALSE(result_of_add.second.is_initialized());
    EXPECT_EQ(i + 5, table_.size());
  }

  // Check next 4 closer additions return 'bucket0_far_contact', 'bucket0_mid_contact',
  // 'bucket1_far_contact', and 'bucket1_mid_contact' as dropped (in that order)
  std::vector<NodeId> dropped;
  for (size_t i = routing_table::optimal_size - 4; i < routing_table::optimal_size; ++i) {
    id.flip((NodeId::kSize * 8) - i);
    their_info.id = NodeId{ id.to_string(), NodeId::EncodingType::kBinary };
    result_of_add = table_.add_node(their_info);
    EXPECT_TRUE(result_of_add.first);
    ASSERT_TRUE(result_of_add.second.is_initialized());
    dropped.push_back(result_of_add.second.get().id);
    EXPECT_EQ(routing_table::optimal_size, table_.size());
    result_of_add = table_.add_node(their_info);
    EXPECT_FALSE(result_of_add.first);
    EXPECT_FALSE(result_of_add.second.is_initialized());
    EXPECT_EQ(routing_table::optimal_size, table_.size());
  }
  EXPECT_EQ(bucket0_far_contact, dropped.at(0));
  EXPECT_EQ(bucket0_mid_contact, dropped.at(1));
  EXPECT_EQ(bucket1_far_contact, dropped.at(2));
  EXPECT_EQ(bucket1_mid_contact, dropped.at(3));

  // Try to add far contacts again (should fail)
  for (const auto& far_contact : dropped) {
    their_info.id = far_contact;
    result_of_add = table_.add_node(their_info);
    EXPECT_FALSE(result_of_add.first);
    EXPECT_FALSE(result_of_add.second.is_initialized());
    EXPECT_EQ(routing_table::optimal_size, table_.size());
  }

  // Add final close contact to push size of table_ above optimal_size
  id.flip((NodeId::kSize * 8) - routing_table::optimal_size);
  their_info.id = NodeId{ id.to_string(), NodeId::EncodingType::kBinary };
  result_of_add = table_.add_node(their_info);
  EXPECT_TRUE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(routing_table::optimal_size + 1, table_.size());
  result_of_add = table_.add_node(their_info);
  EXPECT_FALSE(result_of_add.first);
  EXPECT_FALSE(result_of_add.second.is_initialized());
  EXPECT_EQ(routing_table::optimal_size + 1, table_.size());
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
