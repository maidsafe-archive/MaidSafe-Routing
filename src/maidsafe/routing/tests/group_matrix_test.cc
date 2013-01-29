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
#include <numeric>
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

class GroupMatrixTest : public testing::TestWithParam<bool> {
 protected:
  GroupMatrixTest()
    : own_node_id_(NodeId::kRandomId),
      own_node_info_(),
      client_mode_(GetParam()),
      matrix_(own_node_id_, client_mode_) {
    own_node_info_.node_id = own_node_id_;
  }

  void SetUp() {
    EXPECT_EQ(0, matrix_.GetConnectedPeers().size());
  }

  NodeId own_node_id_;
  NodeInfo own_node_info_;
  bool client_mode_;
  GroupMatrix matrix_;
};

TEST_P(GroupMatrixTest, BEH_EmptyMatrix) {
  const NodeId target_id(NodeId::kRandomId);
  EXPECT_EQ(NodeId(), matrix_.GetConnectedPeerFor(target_id).node_id);

  std::vector<NodeInfo> row_result;
  EXPECT_FALSE(matrix_.GetRow(target_id, row_result));

  NodeId group_leader;
  EXPECT_TRUE(matrix_.IsThisNodeGroupLeader(target_id, group_leader));

  EXPECT_EQ(0, matrix_.GetUniqueNodes().size());

  matrix_.RemoveConnectedPeer(NodeInfo());
  EXPECT_TRUE(matrix_.IsThisNodeGroupLeader(target_id, group_leader));

  if (client_mode_)
    EXPECT_EQ(0, matrix_.GetUniqueNodes().size());
  else
    EXPECT_EQ(1, matrix_.GetUniqueNodes().size());
}

TEST_P(GroupMatrixTest, BEH_AddSamePeerTwice) {
  NodeInfo peer;
  peer.node_id = NodeId(NodeId::kRandomId);

  matrix_.AddConnectedPeer(peer);
  EXPECT_EQ(1, matrix_.GetConnectedPeers().size());
  matrix_.AddConnectedPeer(peer);
  EXPECT_EQ(1, matrix_.GetConnectedPeers().size());
}

TEST_P(GroupMatrixTest, BEH_OneRowOnly) {
  // Partially populate row
  NodeInfo row_1;
  row_1.node_id = NodeId(NodeId::kRandomId);
  std::vector<NodeInfo> row_entries_1;
  uint32_t length(RandomUint32() % (Parameters::node_group_size - 2) + 1);
  uint32_t i(0);
  NodeInfo node_info;
  while (i < length) {
    node_info.node_id = NodeId(NodeId::kRandomId);
    row_entries_1.push_back(node_info);
    ++i;
  }
  matrix_.AddConnectedPeer(row_1);
  EXPECT_EQ(1, matrix_.GetConnectedPeers().size());
  matrix_.UpdateFromConnectedPeer(row_1.node_id, row_entries_1);
  EXPECT_EQ(1, matrix_.GetConnectedPeers().size());

  // Check row contents
  std::vector<NodeInfo> row_result;
  EXPECT_TRUE(matrix_.GetRow(row_1.node_id, row_result));
  EXPECT_TRUE(CompareListOfNodeInfos(row_entries_1, row_result));

  // Check GetConnectedPeerFor
  for (auto target_id : row_entries_1) {
    EXPECT_EQ(row_1.node_id, matrix_.GetConnectedPeerFor(target_id.node_id).node_id);
  }

  // Check IsThisNodeGroupMemberFor
  const NodeId target_id_1(NodeId::kRandomId);
  EXPECT_TRUE(matrix_.IsNodeInGroupRange(target_id_1));

  // Fully populate row
  while (row_entries_1.size() < size_t(Parameters::closest_nodes_size - 1)) {
    node_info.node_id = NodeId(NodeId::kRandomId);
    row_entries_1.push_back(node_info);
  }
  matrix_.UpdateFromConnectedPeer(row_1.node_id, row_entries_1);
  EXPECT_EQ(1, matrix_.GetConnectedPeers().size());

  // Check row contents
  EXPECT_TRUE(matrix_.GetRow(row_1.node_id, row_result));
  EXPECT_TRUE(CompareListOfNodeInfos(row_entries_1, row_result));

  // Check GetConnectedPeerFor
  for (auto target_id : row_entries_1) {
    EXPECT_EQ(row_1.node_id, matrix_.GetConnectedPeerFor(target_id.node_id).node_id);
  }

  // Check IsThisNodeGroupMemberFor
  NodeInfo target_id_2;
  target_id_2.node_id = NodeId(NodeId::kRandomId);
  std::vector<NodeInfo> node_ids(row_entries_1);
  if (!client_mode_)
    node_ids.push_back(own_node_info_);
  node_ids.push_back(row_1);
  SortNodeInfosFromTarget(own_node_id_ , node_ids);
  bool is_group_member(!NodeId::CloserToTarget(node_ids.at(Parameters::node_group_size - 1).node_id,
                                               target_id_2.node_id,
                                               own_node_id_));
  EXPECT_EQ(is_group_member, matrix_.IsNodeInGroupRange(target_id_2.node_id));
}

TEST_P(GroupMatrixTest, BEH_OneColumnOnly) {
  // Populate matrix
  std::vector<NodeInfo> row_ids;
  uint32_t i(0);
  while (i < Parameters::closest_nodes_size) {
    NodeInfo new_node_id;
    new_node_id.node_id = NodeId(NodeId::kRandomId);
    row_ids.push_back(new_node_id);
    matrix_.AddConnectedPeer(new_node_id);
    ++i;
    EXPECT_EQ(i, matrix_.GetConnectedPeers().size());
  }

  // Check rows
  std::vector<NodeInfo> row_result;
  for (auto row_id : row_ids) {
    EXPECT_TRUE(matrix_.GetRow(row_id.node_id, row_result));
    EXPECT_EQ(0, row_result.size());
  }

  // Check GetUniqueNodes
  if (!client_mode_)
    row_ids.push_back(own_node_info_);
  SortNodeInfosFromTarget(own_node_id_, row_ids);
  EXPECT_TRUE(CompareListOfNodeInfos(row_ids, matrix_.GetUniqueNodes()));

  // Check GetConnectedPeerFor
  const NodeId target_id(NodeId::kRandomId);
  EXPECT_EQ(NodeId(), matrix_.GetConnectedPeerFor(target_id).node_id);
}

TEST_P(GroupMatrixTest, BEH_RowsContainSameNodes) {
  // Populate matrix
  std::vector<NodeInfo> row_ids;
  std::vector<NodeInfo> row_entries;

  NodeInfo node_info;
  node_info.node_id = NodeId(NodeId::kRandomId);
  row_ids.push_back(node_info);
  for (uint16_t i(0); i < (Parameters::closest_nodes_size - 1); ++i) {
    node_info.node_id = NodeId(NodeId::kRandomId);
    row_ids.push_back(node_info);
    node_info.node_id = NodeId(NodeId::kRandomId);
    row_entries.push_back(node_info);
  }

  SortNodeInfosFromTarget(own_node_id_, row_entries);
  std::vector<NodeInfo> row_result;
  for (auto row_id : row_ids) {
    matrix_.AddConnectedPeer(row_id);
    matrix_.UpdateFromConnectedPeer(row_id.node_id, row_entries);
    EXPECT_TRUE(matrix_.GetRow(row_id.node_id, row_result));
    EXPECT_EQ(row_result.size(), row_entries.size());
    EXPECT_TRUE(CompareListOfNodeInfos(row_result, row_entries));
    row_result.clear();
  }

  std::vector<NodeInfo> node_ids;
  if (!client_mode_)
    node_ids.push_back(own_node_info_);
  for (auto row_id : row_ids)
    node_ids.push_back(row_id);
  for (auto row_entry : row_entries)
    node_ids.push_back(row_entry);

  // Check size of unique_nodes_
  uint32_t expected_size(2 * Parameters::closest_nodes_size);
  if (client_mode_)
    --expected_size;
  EXPECT_EQ(expected_size, matrix_.GetUniqueNodes().size());

  // Check IsThisNodeGroupMemberFor to verify that entries of unique_nodes_ are deduplicated
  NodeId target_id;
  bool expect_is_group_member;
  bool expect_is_group_leader;
  for (uint32_t i(0); i < 20; ++i) {
    target_id = NodeId(NodeId::kRandomId);
    SortNodeInfosFromTarget(own_node_id_, node_ids);
    expect_is_group_member = !NodeId::CloserToTarget(
                                 node_ids[Parameters::node_group_size - 1].node_id,
                                 target_id,
                                 own_node_id_);
    expect_is_group_leader = (std::find_if(node_ids.begin(), node_ids.end(),
                                          [&](const NodeInfo& node_info)->bool {
                                            return ((node_info.node_id != target_id) &&
                                                    (NodeId::CloserToTarget(node_info.node_id,
                                                                            own_node_id_,
                                                                            target_id)));
                                          }) == node_ids.end());
    EXPECT_EQ(expect_is_group_member, matrix_.IsNodeInGroupRange(target_id));
    NodeId leader;
    EXPECT_EQ(expect_is_group_leader, matrix_.IsThisNodeGroupLeader(target_id, leader));
  }

  // Check GetConnectedPeerFor gives identifier of the first row added to the matrix
  for (auto row_entry : row_entries)
    EXPECT_EQ(row_ids.at(0).node_id, matrix_.GetConnectedPeerFor(row_entry.node_id).node_id);
}

TEST_P(GroupMatrixTest, BEH_UpdateFromNonPeer) {
  NodeInfo node_id_1;
  node_id_1.node_id = NodeId(NodeId::kRandomId);

  std::vector<NodeInfo> row_entries;
  uint32_t length(RandomUint32() % (Parameters::closest_nodes_size - 1));
  uint32_t i(0);
  NodeInfo node_info;
  while (i < length) {
    node_info.node_id = NodeId(NodeId::kRandomId);
    row_entries.push_back(node_info);
    ++i;
  }

  matrix_.UpdateFromConnectedPeer(node_id_1.node_id, row_entries);
  EXPECT_EQ(0, matrix_.GetConnectedPeers().size());
}

TEST_P(GroupMatrixTest, BEH_AddUpdateGetRemovePeers) {
  // Add peers
  std::vector<NodeInfo> row_ids;
  uint32_t i(0);
  NodeInfo node_info;
  while (i < Parameters::closest_nodes_size) {
    node_info.node_id = NodeId(NodeId::kRandomId);
    row_ids.push_back(node_info);
    matrix_.AddConnectedPeer(node_info);
    ++i;
    EXPECT_EQ(i, matrix_.GetConnectedPeers().size());
    SortNodeInfosFromTarget(own_node_id_, row_ids);
    EXPECT_TRUE(CompareListOfNodeInfos(row_ids, matrix_.GetConnectedPeers()));
  }

  SortNodeInfosFromTarget(own_node_id_, row_ids);
  EXPECT_TRUE(CompareListOfNodeInfos(row_ids, matrix_.GetConnectedPeers()));

  // Update peers
  std::vector<NodeInfo> row_entries;
  std::vector<NodeInfo> row_result;
  SortNodeInfosFromTarget(NodeId(NodeId::kRandomId), row_ids);
  for (auto row_id : row_ids) {
    row_entries.clear();
    uint32_t length(RandomUint32() % (Parameters::closest_nodes_size - 2) + 1);
    for (uint32_t i(0); i < length; ++i) {
      node_info.node_id = NodeId(NodeId::kRandomId);
      row_entries.push_back(node_info);
    }
    matrix_.UpdateFromConnectedPeer(row_id.node_id, row_entries);
    EXPECT_TRUE(matrix_.GetRow(row_id.node_id, row_result));
    EXPECT_TRUE(CompareListOfNodeInfos(row_result, row_entries));
  }

  SortNodeInfosFromTarget(own_node_id_, row_ids);
  EXPECT_EQ(row_ids.size(), matrix_.GetConnectedPeers().size());

  // Remove peers
  SortNodeInfosFromTarget(own_node_id_, row_ids);
  while (!row_ids.empty()) {
    uint32_t index(RandomUint32() % row_ids.size());
    matrix_.RemoveConnectedPeer(row_ids.at(index));
    row_ids.erase(row_ids.begin() + index);
    EXPECT_TRUE(CompareListOfNodeInfos(row_ids, matrix_.GetConnectedPeers()));
  }

  EXPECT_EQ(0, matrix_.GetConnectedPeers().size());
}

TEST_P(GroupMatrixTest, BEH_GetConnectedPeerFor) {
  // Populate matrix
  NodeInfo row_1;
  row_1.node_id = NodeId(NodeId::kRandomId);
  NodeInfo row_2;
  row_2.node_id = NodeId(NodeId::kRandomId);
  NodeInfo row_3;
  row_3.node_id = NodeId(NodeId::kRandomId);
  std::vector<NodeInfo> row_ids;
  matrix_.AddConnectedPeer(row_1);
  row_ids.push_back(row_1);
  EXPECT_EQ(1, matrix_.GetConnectedPeers().size());
  matrix_.AddConnectedPeer(row_2);
  row_ids.push_back(row_2);
  EXPECT_EQ(2, matrix_.GetConnectedPeers().size());
  matrix_.AddConnectedPeer(row_3);
  row_ids.push_back(row_3);
  EXPECT_EQ(3, matrix_.GetConnectedPeers().size());
  std::vector<NodeInfo> row_entries_1;
  std::vector<NodeInfo> row_entries_2;
  std::vector<NodeInfo> row_entries_3;
  uint32_t length_1(RandomUint32() % (Parameters::closest_nodes_size - 1) + 1);
  uint32_t length_2(RandomUint32() % (Parameters::closest_nodes_size - 1) + 1);
  uint32_t length_3(RandomUint32() % (Parameters::closest_nodes_size - 1) + 1);
  uint32_t i(0);
  NodeInfo node_info;
  while (i < length_1) {
    node_info.node_id = NodeId(NodeId::kRandomId);
    row_entries_1.push_back(node_info);
    ++i;
  }
  i = 0;
  while (i < length_2) {
    node_info.node_id = NodeId(NodeId::kRandomId);
    row_entries_2.push_back(node_info);
    ++i;
  }
  i = 0;
  while (i < length_3) {
    node_info.node_id = NodeId(NodeId::kRandomId);
    row_entries_3.push_back(node_info);
    ++i;
  }
  matrix_.UpdateFromConnectedPeer(row_1.node_id, row_entries_1);
  matrix_.UpdateFromConnectedPeer(row_2.node_id, row_entries_2);
  matrix_.UpdateFromConnectedPeer(row_3.node_id, row_entries_3);
  std::vector<NodeInfo> row_result;
  EXPECT_TRUE(matrix_.GetRow(row_1.node_id, row_result));
  EXPECT_TRUE(CompareListOfNodeInfos(row_result, row_entries_1));
  EXPECT_TRUE(matrix_.GetRow(row_2.node_id, row_result));
  EXPECT_TRUE(CompareListOfNodeInfos(row_result, row_entries_2));
  EXPECT_TRUE(matrix_.GetRow(row_3.node_id, row_result));
  EXPECT_TRUE(CompareListOfNodeInfos(row_result, row_entries_3));

  NodeId row_id;
  std::vector<NodeInfo> row_entries;
  for (uint16_t j(0); j < (Parameters::closest_nodes_size - 3); ++j) {
    row_entries.clear();
    row_id = NodeId(NodeId::kRandomId);
    node_info.node_id = row_id;
    row_ids.push_back(node_info);
    uint32_t length(RandomUint32() % Parameters::closest_nodes_size);
    for (uint32_t i(0); i < length; ++i) {
      NodeInfo node;
      node.node_id = NodeId(NodeId::kRandomId);
      row_entries.push_back(node);
    }
    matrix_.AddConnectedPeer(node_info);
    matrix_.UpdateFromConnectedPeer(row_id, row_entries);
    EXPECT_TRUE(matrix_.GetRow(row_id, row_result));
    EXPECT_TRUE(CompareListOfNodeInfos(row_result, row_entries));
  }

  // Verify matrix row ids
  SortNodeInfosFromTarget(own_node_id_, row_ids);
  EXPECT_TRUE(CompareListOfNodeInfos(row_ids, matrix_.GetConnectedPeers()));


  // GetConnectedPeersFor
  for (auto target_id : row_entries_1)
    EXPECT_EQ(row_1.node_id, (matrix_.GetConnectedPeerFor(target_id.node_id)).node_id);
  for (auto target_id : row_entries_2)
    EXPECT_EQ(row_2.node_id, matrix_.GetConnectedPeerFor(target_id.node_id).node_id);
  for (auto target_id : row_entries_3)
    EXPECT_EQ(row_3.node_id, matrix_.GetConnectedPeerFor(target_id.node_id).node_id);
  const NodeId target_id(NodeId::kRandomId);
  EXPECT_EQ(NodeId(), matrix_.GetConnectedPeerFor(target_id).node_id);
}

TEST_P(GroupMatrixTest, BEH_IsNodeInGroupRange) {
  std::vector<NodeInfo> node_ids;
  if (!client_mode_)
    node_ids.push_back(own_node_info_);

  // Populate matrix
  NodeInfo row_entry;
  std::vector<NodeInfo> row_entries;
  for (uint32_t j(0); j < Parameters::closest_nodes_size; ++j) {
    row_entries.clear();
    row_entry.node_id = NodeId(NodeId::kRandomId);
    uint32_t length(RandomUint32() % Parameters::closest_nodes_size);
    for (uint32_t i(0); i < length; ++i) {
      NodeInfo node;
      node.node_id = NodeId(NodeId::kRandomId);
      row_entries.push_back(node);
    }
    matrix_.AddConnectedPeer(row_entry);
    matrix_.UpdateFromConnectedPeer(row_entry.node_id, row_entries);
    node_ids.push_back(row_entry);
    for (auto node_id : row_entries)
      node_ids.push_back(node_id);
  }

  // Sort and deduplicate node_ids
  SortNodeInfosFromTarget(own_node_id_, node_ids);
  // Check if this node is group leader for different target NodeIds
  NodeId group_leader_id;
  EXPECT_TRUE(matrix_.IsThisNodeGroupLeader(own_node_id_, group_leader_id));

  NodeId target_id;
  bool expect_is_group_member;
  bool expect_is_group_leader;
  for (int i(0); i < 100; ++i) {
    target_id = NodeId(NodeId::kRandomId);
    SortNodeInfosFromTarget(own_node_id_, node_ids);
    expect_is_group_member = NodeId::CloserToTarget(target_id,
                                 node_ids.at(Parameters::node_group_size - 1).node_id,
                                 own_node_id_);
    expect_is_group_leader =
        (std::find_if(node_ids.begin(), node_ids.end(),
                      [target_id, this] (const NodeInfo& node)->bool {
                        return ((node.node_id != target_id) &&
                                (NodeId::CloserToTarget(node.node_id,
                                                        this->own_node_id_,
                                                        target_id)));
                      }) == node_ids.end());
    EXPECT_EQ(expect_is_group_member, matrix_.IsNodeInGroupRange(target_id));
    EXPECT_EQ(expect_is_group_leader, matrix_.IsThisNodeGroupLeader(target_id, group_leader_id));
  }
}

TEST_P(GroupMatrixTest, BEH_UpdateFromConnectedPeer) {
  // Populate matrix
  NodeInfo row_1;
  row_1.node_id = NodeId(NodeId::kRandomId);

  std::vector<NodeInfo> row_ids;
  matrix_.AddConnectedPeer(row_1);
  row_ids.push_back(row_1);
  EXPECT_EQ(1, matrix_.GetConnectedPeers().size());
  std::vector<NodeInfo> row_entries_1;
  uint32_t i(1);
  NodeInfo node_info;
  while (i < Parameters::closest_nodes_size) {
    node_info.node_id = NodeId(NodeId::kRandomId);
    row_entries_1.push_back(node_info);
    ++i;
  }
  matrix_.UpdateFromConnectedPeer(row_1.node_id, row_entries_1);
  std::vector<NodeInfo> row_result;
  EXPECT_TRUE(matrix_.GetRow(row_1.node_id, row_result));
  EXPECT_TRUE(CompareListOfNodeInfos(row_result, row_entries_1));

  NodeInfo row_id;
  std::vector<NodeInfo> row_entries;
  for (uint16_t j(0); j < (Parameters::closest_nodes_size - 1); ++j) {
    row_entries.clear();
    row_id.node_id = NodeId(NodeId::kRandomId);
    row_ids.push_back(row_id);
    uint32_t length(RandomUint32() % Parameters::closest_nodes_size);
    for (uint32_t i(0); i < length; ++i) {
      NodeInfo node;
      node.node_id = NodeId(NodeId::kRandomId);
      row_entries.push_back(node);
    }
    matrix_.AddConnectedPeer(row_id);
    matrix_.UpdateFromConnectedPeer(row_id.node_id, row_entries);
    EXPECT_TRUE(matrix_.GetRow(row_id.node_id, row_result));
    EXPECT_TRUE(CompareListOfNodeInfos(row_result, row_entries));
  }

  // Verify matrix row ids
  SortNodeInfosFromTarget(own_node_id_, row_ids);
  EXPECT_TRUE(CompareListOfNodeInfos(row_ids, matrix_.GetConnectedPeers()));

  // Update matrix row
  std::vector<NodeInfo> row_entries_2;
  uint32_t j(1);
  while (j < Parameters::closest_nodes_size) {
    NodeInfo node;
    node.node_id = NodeId(NodeId::kRandomId);
    row_entries_2.push_back(node);
    ++j;
  }
  matrix_.UpdateFromConnectedPeer(row_1.node_id, row_entries_2);

  // Check matrix row contains all the new nodes and none of the old ones
  EXPECT_TRUE(matrix_.GetRow(row_1.node_id, row_result));
  EXPECT_TRUE(CompareListOfNodeInfos(row_result, row_entries_2));
  for (auto old_node_id : row_entries_1)
    EXPECT_EQ(NodeId(), matrix_.GetConnectedPeerFor(old_node_id.node_id).node_id);
  for (auto new_node_id : row_entries_2)
    EXPECT_EQ(row_1.node_id, matrix_.GetConnectedPeerFor(new_node_id.node_id).node_id);
}

TEST_P(GroupMatrixTest, BEH_CheckUniqueNodeList) {
  // Add rows to matrix and check GetUniqueNodes
  std::vector<NodeInfo> row_ids;
  for (uint32_t i(0); i < Parameters::closest_nodes_size; ++i) {
    NodeInfo node;
    node.node_id = NodeId(NodeId::kRandomId);
    row_ids.push_back(node);
  }
  EXPECT_EQ(0, matrix_.GetUniqueNodes().size());
  std::vector<NodeInfo> node_ids;
  if (!client_mode_)
    node_ids.push_back(own_node_info_);
  for (auto row_id : row_ids) {
    node_ids.push_back(row_id);
    SortNodeInfosFromTarget(own_node_id_, node_ids);
    matrix_.AddConnectedPeer(row_id);
    EXPECT_TRUE(CompareListOfNodeInfos(node_ids, matrix_.GetUniqueNodes()));
  }

  // Update rows of matrix and check GetUniqueNodes
  std::vector<NodeInfo> row_entries;
  NodeInfo new_row_entry;
  for (auto row_id : row_ids) {
    if (row_entries.size() < size_t(Parameters::closest_nodes_size - 1)) {
      new_row_entry.node_id = NodeId(NodeId::kRandomId);
      row_entries.push_back(new_row_entry);
      node_ids.push_back(new_row_entry);
    }
    matrix_.UpdateFromConnectedPeer(row_id.node_id, row_entries);
    SortNodeInfosFromTarget(own_node_id_, node_ids);
    EXPECT_TRUE(CompareListOfNodeInfos(node_ids, matrix_.GetUniqueNodes()));
  }
}

INSTANTIATE_TEST_CASE_P(VaultModeClientMode,
                        GroupMatrixTest,
                        testing::Bool());



}  // namespace test
}  // namespace routing
}  // namespace maidsafe

