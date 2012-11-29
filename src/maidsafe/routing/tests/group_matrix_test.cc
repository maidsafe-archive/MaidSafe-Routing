/*******************************************************************************
 *  Copyright 2012 maidsafe.net limited                                        *
 *                                                                             *
 *  The following source code is property of maidsafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the licence   *
 *  file licence.txt found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of maidsafe.net. *
 ******************************************************************************/

#include <bitset>
#include <memory>
#include <vector>

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/test.h"

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/group_matrix.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {
namespace routing {
namespace test {

TEST(GroupMatrixTest, FUNC_EmptyMatrix) {
  const NodeId node_id_0(NodeId::kRandomId);
  GroupMatrix matrix(node_id_0);

  EXPECT_EQ(0, matrix.GetConnectedPeers().size());

  const NodeId target_id(NodeId::kRandomId);
  EXPECT_EQ(NodeId(), matrix.GetConnectedPeerFor(target_id));

  std::vector<NodeId> row_result;
  EXPECT_FALSE(matrix.GetRow(target_id, row_result));

  bool is_group_leader(false);
  EXPECT_TRUE(matrix.IsThisNodeGroupMemberFor(target_id, is_group_leader));
  EXPECT_TRUE(is_group_leader);

  EXPECT_EQ(0, matrix.GetUniqueNodes().size());

  matrix.RemoveConnectedPeer(NodeId(NodeId::kRandomId));
  EXPECT_TRUE(matrix.IsThisNodeGroupMemberFor(target_id, is_group_leader));
  EXPECT_TRUE(is_group_leader);

  EXPECT_EQ(1, matrix.GetUniqueNodes().size());
}

TEST(GroupMatrixTest, FUNC_AddSamePeerTwice) {
  const NodeId node_id_0(NodeId::kRandomId);
  GroupMatrix matrix(node_id_0);

  EXPECT_EQ(0, matrix.GetConnectedPeers().size());

  const NodeId peer_node_id(NodeId::kRandomId);
  matrix.AddConnectedPeer(peer_node_id);
  EXPECT_EQ(1, matrix.GetConnectedPeers().size());
  matrix.AddConnectedPeer(peer_node_id);
  EXPECT_EQ(1, matrix.GetConnectedPeers().size());
}

TEST(GroupMatrixTest, FUNC_OneRowOnly) {
  const NodeId node_id_0(NodeId::kRandomId);
  GroupMatrix matrix(node_id_0);

  EXPECT_EQ(0, matrix.GetConnectedPeers().size());

  // Partially populate row
  const NodeId row_id_1(NodeId::kRandomId);
  std::vector<NodeId> row_entries_1;
  uint32_t length(RandomUint32() % (Parameters::node_group_size - 2) + 1);
  uint32_t i(0);
  while (i < length) {
    row_entries_1.push_back(NodeId(NodeId::kRandomId));
    ++i;
  }
  matrix.AddConnectedPeer(row_id_1);
  EXPECT_EQ(1, matrix.GetConnectedPeers().size());
  matrix.UpdateFromConnectedPeer(row_id_1, row_entries_1);
  EXPECT_EQ(1, matrix.GetConnectedPeers().size());

  // Check row contents
  std::vector<NodeId> row_result;
  EXPECT_TRUE(matrix.GetRow(row_id_1, row_result));
  EXPECT_EQ(row_entries_1, row_result);

  // Check GetConnectedPeerFor
  for (auto target_id : row_entries_1) {
    EXPECT_EQ(row_id_1, matrix.GetConnectedPeerFor(target_id));
  }

  // Check IsThisNodeGroupMemberFor
  const NodeId target_id_1(NodeId::kRandomId);
  bool is_group_leader(false);
  EXPECT_TRUE(matrix.IsThisNodeGroupMemberFor(target_id_1, is_group_leader));

  // Fully populate row
  while (row_entries_1.size() < (Parameters::closest_nodes_size - 1)) {
    row_entries_1.push_back(NodeId(NodeId::kRandomId));
  }
  matrix.UpdateFromConnectedPeer(row_id_1, row_entries_1);
  EXPECT_EQ(1, matrix.GetConnectedPeers().size());

  // Check row contents
  EXPECT_TRUE(matrix.GetRow(row_id_1, row_result));
  EXPECT_EQ(row_entries_1, row_result);

  // Check GetConnectedPeerFor
  for (auto target_id : row_entries_1) {
    EXPECT_EQ(row_id_1, matrix.GetConnectedPeerFor(target_id));
  }

  // Check IsThisNodeGroupMemberFor
  const NodeId target_id_2(NodeId::kRandomId);
  std::vector<NodeId> node_ids(row_entries_1);
  node_ids.push_back(node_id_0);
  node_ids.push_back(row_id_1);
  SortIdsFromTarget(target_id_2, node_ids);
  bool is_group_member((node_id_0 ^ target_id_2) <=
                       (node_ids.at(Parameters::node_group_size - 1) ^ target_id_2));
  EXPECT_EQ(is_group_member, matrix.IsThisNodeGroupMemberFor(target_id_2, is_group_leader));
}

TEST(GroupMatrixTest, FUNC_OneColumnOnly) {
  const NodeId node_id_0(NodeId::kRandomId);
  GroupMatrix matrix(node_id_0);

  EXPECT_EQ(0, matrix.GetConnectedPeers().size());

  // Populate matrix
  std::vector<NodeId> row_ids;
  uint32_t i(0);
  while (i < Parameters::closest_nodes_size) {
    const NodeId new_node_id(NodeId::kRandomId);
    row_ids.push_back(new_node_id);
    matrix.AddConnectedPeer(new_node_id);
    ++i;
    EXPECT_EQ(i, matrix.GetConnectedPeers().size());
  }

  // Check rows
  std::vector<NodeId> row_result;
  for (auto row_id : row_ids) {
    EXPECT_TRUE(matrix.GetRow(row_id, row_result));
    EXPECT_EQ(0, row_result.size());
  }

  // Check GetUniqueNodes
  row_ids.push_back(node_id_0);
  SortIdsFromTarget(node_id_0, row_ids);
  EXPECT_EQ(row_ids, matrix.GetUniqueNodes());

  // Check GetConnectedPeerFor
  const NodeId target_id(NodeId::kRandomId);
  EXPECT_EQ(NodeId(), matrix.GetConnectedPeerFor(target_id));
}

TEST(GroupMatrixTest, FUNC_RowsContainSameNodes) {
  const NodeId node_id_0(NodeId::kRandomId);
  GroupMatrix matrix(node_id_0);

  // Populate matrix
  std::vector<NodeId> row_ids;
  std::vector<NodeId> row_entries;

  row_ids.push_back(NodeId(NodeId::kRandomId));
  for (uint16_t i(0); i < (Parameters::closest_nodes_size - 1); ++i) {
    row_ids.push_back(NodeId(NodeId::kRandomId));
    row_entries.push_back(NodeId(NodeId::kRandomId));
  }

  SortIdsFromTarget(node_id_0, row_entries);
  std::vector<NodeId> row_result;
  for (auto row_id : row_ids) {
    matrix.AddConnectedPeer(row_id);
    matrix.UpdateFromConnectedPeer(row_id, row_entries);
    EXPECT_TRUE(matrix.GetRow(row_id, row_result));
    EXPECT_EQ(row_result, row_entries);
    row_result.clear();
  }

  std::vector<NodeId> node_ids;
  node_ids.push_back(node_id_0);
  for (auto row_id : row_ids)
    node_ids.push_back(row_id);
  for (auto row_entry : row_entries)
    node_ids.push_back(row_entry);

  // Check size of unique_nodes_
  uint32_t expected_size(2 * Parameters::closest_nodes_size);
  EXPECT_EQ(expected_size, matrix.GetUniqueNodes().size());

  // Check IsThisNodeGroupMemberFor to verify that entries of unique_nodes_ are deduplicated
  NodeId target_id;
  bool is_group_leader;
  bool expect_is_group_member;
  bool expect_is_group_leader;
  for (uint32_t i(0); i < 20; ++i) {
    target_id = NodeId(NodeId::kRandomId);
    SortIdsFromTarget(target_id, node_ids);
    expect_is_group_member = ((node_id_0 ^ target_id) <=
                              (node_ids.at(Parameters::node_group_size - 1) ^ target_id));
    expect_is_group_leader = (node_id_0 == node_ids.at(0));
    EXPECT_EQ(expect_is_group_member, matrix.IsThisNodeGroupMemberFor(target_id, is_group_leader));
    EXPECT_EQ(expect_is_group_leader, is_group_leader);
  }

  // Check GetConnectedPeerFor gives identifier of the first row added to the matrix
  for (auto row_entry : row_entries)
    EXPECT_EQ(row_ids.at(0), matrix.GetConnectedPeerFor(row_entry));
}

TEST(GroupMatrixTest, FUNC_UpdateFromNonPeer) {
  const NodeId node_id_0(NodeId::kRandomId);
  GroupMatrix matrix(node_id_0);

  EXPECT_EQ(0, matrix.GetConnectedPeers().size());
  const NodeId node_id_1(NodeId::kRandomId);

  std::vector<NodeId> row_entries;
  uint32_t length(RandomUint32() % (Parameters::closest_nodes_size - 1));
  uint32_t i(0);
  while (i < length) {
    row_entries.push_back(NodeId(NodeId::kRandomId));
    ++i;
  }

  matrix.UpdateFromConnectedPeer(node_id_1, row_entries);
  EXPECT_EQ(0, matrix.GetConnectedPeers().size());
}

TEST(GroupMatrixTest, FUNC_AddUpdateGetRemovePeers) {
  const NodeId node_id_0(NodeId::kRandomId);
  GroupMatrix matrix(node_id_0);

  EXPECT_EQ(0, matrix.GetConnectedPeers().size());

  // Add peers
  std::vector<NodeId> row_ids;
  uint32_t i(0);
  while (i < Parameters::closest_nodes_size) {
    const NodeId new_row_id(NodeId::kRandomId);
    row_ids.push_back(new_row_id);
    matrix.AddConnectedPeer(new_row_id);
    ++i;
    EXPECT_EQ(i, matrix.GetConnectedPeers().size());
    SortIdsFromTarget(node_id_0, row_ids);
    EXPECT_EQ(row_ids, matrix.GetConnectedPeers());
  }

  SortIdsFromTarget(node_id_0, row_ids);
  EXPECT_EQ(row_ids, matrix.GetConnectedPeers());

  // Update peers
  std::vector<NodeId> row_entries;
  std::vector<NodeId> row_result;
  SortIdsFromTarget(NodeId(NodeId::kRandomId), row_ids);
  for (auto row_id : row_ids) {
    row_entries.clear();
    uint32_t length(RandomUint32() % (Parameters::closest_nodes_size - 2) + 1);
    for (uint32_t i(0); i < length; ++i)
      row_entries.push_back(NodeId(NodeId::kRandomId));
    matrix.UpdateFromConnectedPeer(row_id, row_entries);
    EXPECT_TRUE(matrix.GetRow(row_id, row_result));
    EXPECT_EQ(row_result, row_entries);
  }

  SortIdsFromTarget(node_id_0, row_ids);
  EXPECT_EQ(row_ids, matrix.GetConnectedPeers());

  // Remove peers
  SortIdsFromTarget(node_id_0, row_ids);
  while (!row_ids.empty()) {
    uint32_t index(RandomUint32() % row_ids.size());
    matrix.RemoveConnectedPeer(row_ids.at(index));
    row_ids.erase(row_ids.begin() + index);
    EXPECT_EQ(row_ids.size(), matrix.GetConnectedPeers().size());
    EXPECT_EQ(row_ids, matrix.GetConnectedPeers());
  }

  EXPECT_EQ(0, matrix.GetConnectedPeers().size());
}

TEST(GroupMatrixTest, FUNC_GetConnectedPeerFor) {
  const NodeId node_id_0(NodeId::kRandomId);
  GroupMatrix matrix(node_id_0);

  EXPECT_EQ(0, matrix.GetConnectedPeers().size());

  // Populate matrix
  const NodeId row_id_1(NodeId::kRandomId);
  const NodeId row_id_2(NodeId::kRandomId);
  const NodeId row_id_3(NodeId::kRandomId);
  std::vector<NodeId> row_ids;
  matrix.AddConnectedPeer(row_id_1);
  row_ids.push_back(row_id_1);
  EXPECT_EQ(1, matrix.GetConnectedPeers().size());
  matrix.AddConnectedPeer(row_id_2);
  row_ids.push_back(row_id_2);
  EXPECT_EQ(2, matrix.GetConnectedPeers().size());
  matrix.AddConnectedPeer(row_id_3);
  row_ids.push_back(row_id_3);
  EXPECT_EQ(3, matrix.GetConnectedPeers().size());
  std::vector<NodeId> row_entries_1;
  std::vector<NodeId> row_entries_2;
  std::vector<NodeId> row_entries_3;
  uint32_t length_1(RandomUint32() % (Parameters::closest_nodes_size - 1) + 1);
  uint32_t length_2(RandomUint32() % (Parameters::closest_nodes_size - 1) + 1);
  uint32_t length_3(RandomUint32() % (Parameters::closest_nodes_size - 1) + 1);
  uint32_t i(0);
  while (i < length_1) {
    row_entries_1.push_back(NodeId(NodeId::kRandomId));
    ++i;
  }
  i = 0;
  while (i < length_2) {
    row_entries_2.push_back(NodeId(NodeId::kRandomId));
    ++i;
  }
  i = 0;
  while (i < length_3) {
    row_entries_3.push_back(NodeId(NodeId::kRandomId));
    ++i;
  }
  matrix.UpdateFromConnectedPeer(row_id_1, row_entries_1);
  matrix.UpdateFromConnectedPeer(row_id_2, row_entries_2);
  matrix.UpdateFromConnectedPeer(row_id_3, row_entries_3);
  std::vector<NodeId> row_result;
  EXPECT_TRUE(matrix.GetRow(row_id_1, row_result));
  EXPECT_EQ(row_result, row_entries_1);
  EXPECT_TRUE(matrix.GetRow(row_id_2, row_result));
  EXPECT_EQ(row_result, row_entries_2);
  EXPECT_TRUE(matrix.GetRow(row_id_3, row_result));
  EXPECT_EQ(row_result, row_entries_3);

  NodeId row_id;
  std::vector<NodeId> row_entries;
  for (uint16_t j(0); j < (Parameters::closest_nodes_size - 3); ++j) {
    row_entries.clear();
    row_id = NodeId(NodeId::kRandomId);
    row_ids.push_back(row_id);
    uint32_t length(RandomUint32() % Parameters::closest_nodes_size);
    for (uint32_t i(0); i < length; ++i)
      row_entries.push_back(NodeId(NodeId::kRandomId));
    matrix.AddConnectedPeer(row_id);
    matrix.UpdateFromConnectedPeer(row_id, row_entries);
    EXPECT_TRUE(matrix.GetRow(row_id, row_result));
    EXPECT_EQ(row_result, row_entries);
  }

  // Verify matrix row ids
  SortIdsFromTarget(node_id_0, row_ids);
  EXPECT_EQ(row_ids, matrix.GetConnectedPeers());


  // GetConnectedPeersFor
  for (auto target_id : row_entries_1)
    EXPECT_EQ(row_id_1, matrix.GetConnectedPeerFor(target_id));
  for (auto target_id : row_entries_2)
    EXPECT_EQ(row_id_2, matrix.GetConnectedPeerFor(target_id));
  for (auto target_id : row_entries_3)
    EXPECT_EQ(row_id_3, matrix.GetConnectedPeerFor(target_id));
  const NodeId target_id(NodeId::kRandomId);
  EXPECT_EQ(NodeId(), matrix.GetConnectedPeerFor(target_id));
}

TEST(GroupMatrixTest, FUNC_IsThisNodeGroupMemberFor) {
  const NodeId node_id_0(NodeId::kRandomId);
  GroupMatrix matrix(node_id_0);

  std::vector<NodeId> node_ids(1, node_id_0);

  // Populate matrix
  NodeId row_id;
  std::vector<NodeId> row_entries;
  for (uint32_t j(0); j < Parameters::closest_nodes_size; ++j) {
    row_entries.clear();
    row_id = NodeId(NodeId::kRandomId);
    uint32_t length(RandomUint32() % Parameters::closest_nodes_size);
    for (uint32_t i(0); i < length; ++i)
      row_entries.push_back(NodeId(NodeId::kRandomId));
    matrix.AddConnectedPeer(row_id);
    matrix.UpdateFromConnectedPeer(row_id, row_entries);
    node_ids.push_back(row_id);
    for (auto node_id : row_entries)
      node_ids.push_back(node_id);
  }

  // Sort and deduplicate node_ids
  SortIdsFromTarget(node_id_0, node_ids);
  auto itr = std::unique(node_ids.begin(), node_ids.end());
  node_ids.resize(itr - node_ids.begin());

  // Check if this node is group leader for different target NodeIds
  bool is_group_leader(false);
  EXPECT_TRUE(matrix.IsThisNodeGroupMemberFor(node_id_0, is_group_leader));
  EXPECT_TRUE(is_group_leader);

  NodeId target_id;
  bool expect_is_group_member;
  bool expect_is_group_leader;
  for (int i(0); i < 20; ++i) {
    target_id = NodeId(NodeId::kRandomId);
    SortIdsFromTarget(target_id, node_ids);
    expect_is_group_member = ((node_id_0 ^ target_id) <=
                              (node_ids.at(Parameters::node_group_size - 1) ^ target_id));
    expect_is_group_leader = (node_id_0 == node_ids.at(0));
    EXPECT_EQ(expect_is_group_member, matrix.IsThisNodeGroupMemberFor(target_id, is_group_leader));
    EXPECT_EQ(expect_is_group_leader, is_group_leader);
  }
}

TEST(GroupMatrixTest, FUNC_UpdateFromConnectedPeer) {
  const NodeId node_id_0(NodeId::kRandomId);
  GroupMatrix matrix(node_id_0);

  EXPECT_EQ(0, matrix.GetConnectedPeers().size());

  // Populate matrix
  const NodeId row_id_1(NodeId::kRandomId);
  std::vector<NodeId> row_ids;
  matrix.AddConnectedPeer(row_id_1);
  row_ids.push_back(row_id_1);
  EXPECT_EQ(1, matrix.GetConnectedPeers().size());
  std::vector<NodeId> row_entries_1;
  uint32_t i(1);
  while (i < Parameters::closest_nodes_size) {
    row_entries_1.push_back(NodeId(NodeId::kRandomId));
    ++i;
  }
  matrix.UpdateFromConnectedPeer(row_id_1, row_entries_1);
  std::vector<NodeId> row_result;
  EXPECT_TRUE(matrix.GetRow(row_id_1, row_result));
  EXPECT_EQ(row_result, row_entries_1);

  NodeId row_id;
  std::vector<NodeId> row_entries;
  for (uint16_t j(0); j < (Parameters::closest_nodes_size - 1); ++j) {
    row_entries.clear();
    row_id = NodeId(NodeId::kRandomId);
    row_ids.push_back(row_id);
    uint32_t length(RandomUint32() % Parameters::closest_nodes_size);
    for (uint32_t i(0); i < length; ++i)
      row_entries.push_back(NodeId(NodeId::kRandomId));
    matrix.AddConnectedPeer(row_id);
    matrix.UpdateFromConnectedPeer(row_id, row_entries);
    EXPECT_TRUE(matrix.GetRow(row_id, row_result));
    EXPECT_EQ(row_result, row_entries);
  }

  // Verify matrix row ids
  SortIdsFromTarget(node_id_0, row_ids);
  EXPECT_EQ(row_ids, matrix.GetConnectedPeers());

  // Update matrix row
  std::vector<NodeId> row_entries_2;
  uint32_t j(1);
  while (j < Parameters::closest_nodes_size) {
    row_entries_2.push_back(NodeId(NodeId::kRandomId));
    ++j;
  }
  matrix.UpdateFromConnectedPeer(row_id_1, row_entries_2);

  // Check matrix row contains all the new nodes and none of the old ones
  EXPECT_TRUE(matrix.GetRow(row_id_1, row_result));
  EXPECT_EQ(row_result, row_entries_2);
  for (auto old_node_id : row_entries_1)
    EXPECT_EQ(NodeId(), matrix.GetConnectedPeerFor(old_node_id));
  for (auto new_node_id : row_entries_2)
    EXPECT_EQ(row_id_1, matrix.GetConnectedPeerFor(new_node_id));
}

TEST(GroupMatrixTest, FUNC_CheckUniqueNodeList) {
  const NodeId node_id_0(NodeId::kRandomId);
  GroupMatrix matrix(node_id_0);

  // Add rows to matrix and check GetUniqueNodes
  std::vector<NodeId> row_ids;
  for (uint32_t i(0); i < Parameters::closest_nodes_size; ++i)
    row_ids.push_back(NodeId(NodeId::kRandomId));
  EXPECT_EQ(0, matrix.GetUniqueNodes().size());
  std::vector<NodeId> node_ids;
  node_ids.push_back(node_id_0);
  for (auto row_id : row_ids) {
    node_ids.push_back(row_id);
    SortIdsFromTarget(node_id_0, node_ids);
    matrix.AddConnectedPeer(row_id);
    EXPECT_EQ(node_ids, matrix.GetUniqueNodes());
  }

  // Update rows of matrix and check GetUniqueNodes
  std::vector<NodeId> row_entries;
  NodeId new_row_entry;
  for (auto row_id : row_ids) {
    if (row_entries.size() < (Parameters::closest_nodes_size - 1)) {
      new_row_entry = NodeId(NodeId::kRandomId);
      row_entries.push_back(new_row_entry);
      node_ids.push_back(new_row_entry);
    }
    matrix.UpdateFromConnectedPeer(row_id, row_entries);
    SortIdsFromTarget(node_id_0, node_ids);
    EXPECT_EQ(node_ids, matrix.GetUniqueNodes());
  }
}

}  // namespace test
}  // namespace routing
}  // namespace maidsafe

