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

TEST(GroupMatrixTest, BEH_EmptyMatrix) {
  const NodeId node_0(NodeId::kRandomId);
  GroupMatrix matrix(node_0);

  EXPECT_EQ(0, matrix.GetConnectedPeers().size());

  const NodeId target_id(NodeId::kRandomId);
  EXPECT_EQ(NodeId(), matrix.GetConnectedPeerFor(target_id).node_id);

  std::vector<NodeInfo> row_result;
  EXPECT_FALSE(matrix.GetRow(target_id, row_result));

  NodeId group_leader;
  EXPECT_TRUE(matrix.IsThisNodeGroupLeader(target_id, group_leader));

  EXPECT_EQ(0, matrix.GetUniqueNodes().size());

  matrix.RemoveConnectedPeer(NodeInfo());
  EXPECT_TRUE(matrix.IsThisNodeGroupLeader(target_id, group_leader));

  EXPECT_EQ(1, matrix.GetUniqueNodes().size());
}

TEST(GroupMatrixTest, BEH_AddSamePeerTwice) {
  const NodeId node_0(NodeId::kRandomId);
  GroupMatrix matrix(node_0);

  EXPECT_EQ(0, matrix.GetConnectedPeers().size());

  const NodeInfo peer;
  matrix.AddConnectedPeer(peer);
  EXPECT_EQ(1, matrix.GetConnectedPeers().size());
  matrix.AddConnectedPeer(peer);
  EXPECT_EQ(1, matrix.GetConnectedPeers().size());
}

TEST(GroupMatrixTest, BEH_AverageDistance) {
  NodeId node_id(NodeId::kRandomId);
  LOG(kVerbose) << node_id.ToStringEncoded(NodeId::kBinary);
  NodeId average(node_id);
  GroupMatrix matrix(node_id);
  matrix.average_distance_ = average;
  matrix.AverageDistance(average);
  EXPECT_EQ(matrix.average_distance_.ToStringEncoded(NodeId::kBinary).substr(0, 511),
            average.ToStringEncoded(NodeId::kBinary).substr(0, 511));
  node_id = NodeId();
  matrix.average_distance_ = node_id;
  average = node_id;
  matrix.AverageDistance(node_id);
  EXPECT_EQ(matrix.average_distance_.ToStringEncoded(NodeId::kBinary).substr(0, 511),
            average.ToStringEncoded(NodeId::kBinary).substr(0, 511));
  node_id = NodeId(NodeId::kMaxId);
  matrix.average_distance_ = node_id;
  average = node_id;
  matrix.AverageDistance(node_id);
  EXPECT_EQ(matrix.average_distance_.ToStringEncoded(NodeId::kBinary).substr(0, 511),
            average.ToStringEncoded(NodeId::kBinary).substr(0, 511));
  node_id = NodeId();
  matrix.average_distance_ = NodeId(NodeId::kMaxId);
  average = matrix.average_distance_;
  for (auto index(0); index < 512; ++index) {
    matrix.AverageDistance(node_id);
    average = NodeId("0" + average.ToStringEncoded(NodeId::kBinary).substr(0, 511),
                     NodeId::kBinary);
    EXPECT_EQ(matrix.average_distance_.ToStringEncoded(NodeId::kBinary).substr(0, 511),
              average.ToStringEncoded(NodeId::kBinary).substr(0, 511));
  }
  matrix.average_distance_ = NodeId(NodeId::kRandomId);
  average = matrix.average_distance_;
  for (auto index(0); index < 512; ++index) {
    matrix.AverageDistance(node_id);
    average = NodeId("0" + average.ToStringEncoded(NodeId::kBinary).substr(0, 511),
                     NodeId::kBinary);
    EXPECT_EQ(matrix.average_distance_.ToStringEncoded(NodeId::kBinary).substr(0, 511),
              average.ToStringEncoded(NodeId::kBinary).substr(0, 511));
  }
}

TEST(GroupMatrixTest, BEH_OneRowOnly) {
  NodeInfo node_0;
  node_0.node_id = NodeId(NodeId::kRandomId);
  GroupMatrix matrix(node_0.node_id);

  EXPECT_EQ(0, matrix.GetConnectedPeers().size());

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
  matrix.AddConnectedPeer(row_1);
  EXPECT_EQ(1, matrix.GetConnectedPeers().size());
  matrix.UpdateFromConnectedPeer(row_1.node_id, row_entries_1);
  EXPECT_EQ(1, matrix.GetConnectedPeers().size());

  // Check row contents
  std::vector<NodeInfo> row_result;
  EXPECT_TRUE(matrix.GetRow(row_1.node_id, row_result));
  EXPECT_TRUE(CompareListOfNodeInfos(row_entries_1, row_result));

  // Check GetConnectedPeerFor
  for (auto target_id : row_entries_1) {
    EXPECT_EQ(row_1.node_id, matrix.GetConnectedPeerFor(target_id.node_id).node_id);
  }

  // Check IsThisNodeGroupMemberFor
  const NodeId target_id_1(NodeId::kRandomId);
  EXPECT_TRUE(matrix.IsNodeInGroupRange(target_id_1));

  // Fully populate row
  while (row_entries_1.size() < size_t(Parameters::closest_nodes_size - 1)) {
    node_info.node_id = NodeId(NodeId::kRandomId);
    row_entries_1.push_back(node_info);
  }
  matrix.UpdateFromConnectedPeer(row_1.node_id, row_entries_1);
  EXPECT_EQ(1, matrix.GetConnectedPeers().size());

  // Check row contents
  EXPECT_TRUE(matrix.GetRow(row_1.node_id, row_result));
  EXPECT_TRUE(CompareListOfNodeInfos(row_entries_1, row_result));

  // Check GetConnectedPeerFor
  for (auto target_id : row_entries_1) {
    EXPECT_EQ(row_1.node_id, matrix.GetConnectedPeerFor(target_id.node_id).node_id);
  }

  // Check IsThisNodeGroupMemberFor
  NodeInfo target_id_2;
  target_id_2.node_id = NodeId(NodeId::kRandomId);
  std::vector<NodeInfo> node_ids(row_entries_1);
  node_ids.push_back(node_0);
  node_ids.push_back(row_1);
  SortNodeInfosFromTarget(node_0.node_id , node_ids);
  bool is_group_member(!NodeId::CloserToTarget(node_ids.at(Parameters::node_group_size - 1).node_id,
                                               target_id_2.node_id,
                                               node_0.node_id));
  EXPECT_EQ(is_group_member, matrix.IsNodeInGroupRange(target_id_2.node_id));
}

TEST(GroupMatrixTest, BEH_OneColumnOnly) {
  NodeInfo node_0;
  node_0.node_id = NodeId(NodeId::kRandomId);
  GroupMatrix matrix(node_0.node_id);

  EXPECT_EQ(0, matrix.GetConnectedPeers().size());

  // Populate matrix
  std::vector<NodeInfo> row_ids;
  uint32_t i(0);
  while (i < Parameters::closest_nodes_size) {
    NodeInfo new_node_id;
    new_node_id.node_id = NodeId(NodeId::kRandomId);
    row_ids.push_back(new_node_id);
    matrix.AddConnectedPeer(new_node_id);
    ++i;
    EXPECT_EQ(i, matrix.GetConnectedPeers().size());
  }

  // Check rows
  std::vector<NodeInfo> row_result;
  for (auto row_id : row_ids) {
    EXPECT_TRUE(matrix.GetRow(row_id.node_id, row_result));
    EXPECT_EQ(0, row_result.size());
  }

  // Check GetUniqueNodes
  row_ids.push_back(node_0);
  SortNodeInfosFromTarget(node_0.node_id, row_ids);
  EXPECT_TRUE(CompareListOfNodeInfos(row_ids, matrix.GetUniqueNodes()));

  // Check GetConnectedPeerFor
  const NodeId target_id(NodeId::kRandomId);
  EXPECT_EQ(NodeId(), matrix.GetConnectedPeerFor(target_id).node_id);
}

TEST(GroupMatrixTest, BEH_RowsContainSameNodes) {
  NodeInfo node_0;
  node_0.node_id = NodeId(NodeId::kRandomId);
  GroupMatrix matrix(node_0.node_id);

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

  SortNodeInfosFromTarget(node_0.node_id, row_entries);
  std::vector<NodeInfo> row_result;
  for (auto row_id : row_ids) {
    matrix.AddConnectedPeer(row_id);
    matrix.UpdateFromConnectedPeer(row_id.node_id, row_entries);
    EXPECT_TRUE(matrix.GetRow(row_id.node_id, row_result));
    EXPECT_EQ(row_result.size(), row_entries.size());
    EXPECT_TRUE(CompareListOfNodeInfos(row_result, row_entries));
    row_result.clear();
  }

  std::vector<NodeInfo> node_ids;
  node_ids.push_back(node_0);
  for (auto row_id : row_ids)
    node_ids.push_back(row_id);
  for (auto row_entry : row_entries)
    node_ids.push_back(row_entry);

  // Check size of unique_nodes_
  uint32_t expected_size(2 * Parameters::closest_nodes_size);
  EXPECT_EQ(expected_size, matrix.GetUniqueNodes().size());

  // Check IsThisNodeGroupMemberFor to verify that entries of unique_nodes_ are deduplicated
  NodeId target_id;
  bool expect_is_group_member;
  bool expect_is_group_leader;
  for (uint32_t i(0); i < 20; ++i) {
    target_id = NodeId(NodeId::kRandomId);
    SortNodeInfosFromTarget(node_0.node_id, node_ids);
    expect_is_group_member = !NodeId::CloserToTarget(
                                 node_ids[Parameters::node_group_size - 1].node_id,
                                 target_id,
                                 node_0.node_id);
    expect_is_group_leader = (std::find_if(node_ids.begin(), node_ids.end(),
                                          [&](const NodeInfo& node_info)->bool {
                                            return ((node_info.node_id != target_id) &&
                                                    (NodeId::CloserToTarget(node_info.node_id,
                                                                            node_0.node_id,
                                                                            target_id)));
                                          }) == node_ids.end());
    EXPECT_EQ(expect_is_group_member, matrix.IsNodeInGroupRange(target_id));
    NodeId leader;
    EXPECT_EQ(expect_is_group_leader, matrix.IsThisNodeGroupLeader(target_id, leader));
  }

  // Check GetConnectedPeerFor gives identifier of the first row added to the matrix
  for (auto row_entry : row_entries)
    EXPECT_EQ(row_ids.at(0).node_id, matrix.GetConnectedPeerFor(row_entry.node_id).node_id);
}

TEST(GroupMatrixTest, BEH_UpdateFromNonPeer) {
  NodeInfo node_0;
  node_0.node_id = NodeId(NodeId::kRandomId);
  GroupMatrix matrix(node_0.node_id);

  EXPECT_EQ(0, matrix.GetConnectedPeers().size());
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

  matrix.UpdateFromConnectedPeer(node_id_1.node_id, row_entries);
  EXPECT_EQ(0, matrix.GetConnectedPeers().size());
}

TEST(GroupMatrixTest, BEH_AddUpdateGetRemovePeers) {
  NodeInfo node_0;
  node_0.node_id = NodeId(NodeId::kRandomId);
  GroupMatrix matrix(node_0.node_id);

  EXPECT_EQ(0, matrix.GetConnectedPeers().size());

  // Add peers
  std::vector<NodeInfo> row_ids;
  uint32_t i(0);
  NodeInfo node_info;
  while (i < Parameters::closest_nodes_size) {
    node_info.node_id = NodeId(NodeId::kRandomId);
    row_ids.push_back(node_info);
    matrix.AddConnectedPeer(node_info);
    ++i;
    EXPECT_EQ(i, matrix.GetConnectedPeers().size());
    SortNodeInfosFromTarget(node_0.node_id, row_ids);
    EXPECT_TRUE(CompareListOfNodeInfos(row_ids, matrix.GetConnectedPeers()));
  }

  SortNodeInfosFromTarget(node_0.node_id, row_ids);
  EXPECT_TRUE(CompareListOfNodeInfos(row_ids, matrix.GetConnectedPeers()));

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
    matrix.UpdateFromConnectedPeer(row_id.node_id, row_entries);
    EXPECT_TRUE(matrix.GetRow(row_id.node_id, row_result));
    EXPECT_TRUE(CompareListOfNodeInfos(row_result, row_entries));
  }

  SortNodeInfosFromTarget(node_0.node_id, row_ids);
  EXPECT_EQ(row_ids.size(), matrix.GetConnectedPeers().size());

  // Remove peers
  SortNodeInfosFromTarget(node_0.node_id, row_ids);
  while (!row_ids.empty()) {
    uint32_t index(RandomUint32() % row_ids.size());
    matrix.RemoveConnectedPeer(row_ids.at(index));
    row_ids.erase(row_ids.begin() + index);
    EXPECT_TRUE(CompareListOfNodeInfos(row_ids, matrix.GetConnectedPeers()));
  }

  EXPECT_EQ(0, matrix.GetConnectedPeers().size());
}

TEST(GroupMatrixTest, BEH_GetConnectedPeerFor) {
  NodeInfo node_0;
  node_0.node_id = NodeId(NodeId::kRandomId);
  GroupMatrix matrix(node_0.node_id);

  EXPECT_EQ(0, matrix.GetConnectedPeers().size());

  // Populate matrix
  NodeInfo row_1;
  row_1.node_id = NodeId(NodeId::kRandomId);
  NodeInfo row_2;
  row_2.node_id = NodeId(NodeId::kRandomId);
  NodeInfo row_3;
  row_3.node_id = NodeId(NodeId::kRandomId);
  std::vector<NodeInfo> row_ids;
  matrix.AddConnectedPeer(row_1);
  row_ids.push_back(row_1);
  EXPECT_EQ(1, matrix.GetConnectedPeers().size());
  matrix.AddConnectedPeer(row_2);
  row_ids.push_back(row_2);
  EXPECT_EQ(2, matrix.GetConnectedPeers().size());
  matrix.AddConnectedPeer(row_3);
  row_ids.push_back(row_3);
  EXPECT_EQ(3, matrix.GetConnectedPeers().size());
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
  matrix.UpdateFromConnectedPeer(row_1.node_id, row_entries_1);
  matrix.UpdateFromConnectedPeer(row_2.node_id, row_entries_2);
  matrix.UpdateFromConnectedPeer(row_3.node_id, row_entries_3);
  std::vector<NodeInfo> row_result;
  EXPECT_TRUE(matrix.GetRow(row_1.node_id, row_result));
  EXPECT_TRUE(CompareListOfNodeInfos(row_result, row_entries_1));
  EXPECT_TRUE(matrix.GetRow(row_2.node_id, row_result));
  EXPECT_TRUE(CompareListOfNodeInfos(row_result, row_entries_2));
  EXPECT_TRUE(matrix.GetRow(row_3.node_id, row_result));
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
    matrix.AddConnectedPeer(node_info);
    matrix.UpdateFromConnectedPeer(row_id, row_entries);
    EXPECT_TRUE(matrix.GetRow(row_id, row_result));
    EXPECT_TRUE(CompareListOfNodeInfos(row_result, row_entries));
  }

  // Verify matrix row ids
  SortNodeInfosFromTarget(node_0.node_id, row_ids);
  EXPECT_TRUE(CompareListOfNodeInfos(row_ids, matrix.GetConnectedPeers()));


  // GetConnectedPeersFor
  for (auto target_id : row_entries_1)
    EXPECT_EQ(row_1.node_id, (matrix.GetConnectedPeerFor(target_id.node_id)).node_id);
  for (auto target_id : row_entries_2)
    EXPECT_EQ(row_2.node_id, matrix.GetConnectedPeerFor(target_id.node_id).node_id);
  for (auto target_id : row_entries_3)
    EXPECT_EQ(row_3.node_id, matrix.GetConnectedPeerFor(target_id.node_id).node_id);
  const NodeId target_id(NodeId::kRandomId);
  EXPECT_EQ(NodeId(), matrix.GetConnectedPeerFor(target_id).node_id);
}

TEST(GroupMatrixTest, BEH_IsNodeInGroupRange) {
  NodeInfo node_0;
  node_0.node_id = NodeId(NodeId::kRandomId);
  GroupMatrix matrix(node_0.node_id);

  std::vector<NodeInfo> node_ids(1, node_0);

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
    matrix.AddConnectedPeer(row_entry);
    matrix.UpdateFromConnectedPeer(row_entry.node_id, row_entries);
    node_ids.push_back(row_entry);
    for (auto node_id : row_entries)
      node_ids.push_back(node_id);
  }

  // Sort and deduplicate node_ids
  SortNodeInfosFromTarget(node_0.node_id, node_ids);
  // Check if this node is group leader for different target NodeIds
  NodeId group_leader_id;
  EXPECT_TRUE(matrix.IsThisNodeGroupLeader(node_0.node_id, group_leader_id));

  NodeId target_id;
  bool expect_is_group_member;
  bool expect_is_group_leader;
  for (int i(0); i < 100; ++i) {
    target_id = NodeId(NodeId::kRandomId);
    SortNodeInfosFromTarget(node_0.node_id, node_ids);
    expect_is_group_member = NodeId::CloserToTarget(target_id,
                                 node_ids.at(Parameters::node_group_size - 1).node_id,
                                 node_0.node_id);
    expect_is_group_leader =
        (std::find_if(node_ids.begin(), node_ids.end(),
                      [target_id, node_0] (const NodeInfo& node)->bool {
                        return ((node.node_id != target_id) &&
                                (NodeId::CloserToTarget(node.node_id,
                                                        node_0.node_id,
                                                        target_id)));
                      }) == node_ids.end());
    EXPECT_EQ(expect_is_group_member, matrix.IsNodeInGroupRange(target_id));
    EXPECT_EQ(expect_is_group_leader, matrix.IsThisNodeGroupLeader(target_id, group_leader_id));
  }
}

TEST(GroupMatrixTest, BEH_UpdateFromConnectedPeer) {
  const NodeId node_0(NodeId::kRandomId);
  GroupMatrix matrix(node_0);

  EXPECT_EQ(0, matrix.GetConnectedPeers().size());

  // Populate matrix
  NodeInfo row_1;
  row_1.node_id = NodeId(NodeId::kRandomId);

  std::vector<NodeInfo> row_ids;
  matrix.AddConnectedPeer(row_1);
  row_ids.push_back(row_1);
  EXPECT_EQ(1, matrix.GetConnectedPeers().size());
  std::vector<NodeInfo> row_entries_1;
  uint32_t i(1);
  NodeInfo node_info;
  while (i < Parameters::closest_nodes_size) {
    node_info.node_id = NodeId(NodeId::kRandomId);
    row_entries_1.push_back(node_info);
    ++i;
  }
  matrix.UpdateFromConnectedPeer(row_1.node_id, row_entries_1);
  std::vector<NodeInfo> row_result;
  EXPECT_TRUE(matrix.GetRow(row_1.node_id, row_result));
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
    matrix.AddConnectedPeer(row_id);
    matrix.UpdateFromConnectedPeer(row_id.node_id, row_entries);
    EXPECT_TRUE(matrix.GetRow(row_id.node_id, row_result));
    EXPECT_TRUE(CompareListOfNodeInfos(row_result, row_entries));
  }

  // Verify matrix row ids
  SortNodeInfosFromTarget(node_0, row_ids);
  EXPECT_TRUE(CompareListOfNodeInfos(row_ids, matrix.GetConnectedPeers()));

  // Update matrix row
  std::vector<NodeInfo> row_entries_2;
  uint32_t j(1);
  while (j < Parameters::closest_nodes_size) {
    NodeInfo node;
    node.node_id = NodeId(NodeId::kRandomId);
    row_entries_2.push_back(node);
    ++j;
  }
  matrix.UpdateFromConnectedPeer(row_1.node_id, row_entries_2);

  // Check matrix row contains all the new nodes and none of the old ones
  EXPECT_TRUE(matrix.GetRow(row_1.node_id, row_result));
  EXPECT_TRUE(CompareListOfNodeInfos(row_result, row_entries_2));
  for (auto old_node_id : row_entries_1)
    EXPECT_EQ(NodeId(), matrix.GetConnectedPeerFor(old_node_id.node_id).node_id);
  for (auto new_node_id : row_entries_2)
    EXPECT_EQ(row_1.node_id, matrix.GetConnectedPeerFor(new_node_id.node_id).node_id);
}

TEST(GroupMatrixTest, BEH_CheckUniqueNodeList) {
  NodeInfo node_0;
  node_0.node_id = NodeId(NodeId::kRandomId);
  GroupMatrix matrix(node_0.node_id);

  // Add rows to matrix and check GetUniqueNodes
  std::vector<NodeInfo> row_ids;
  for (uint32_t i(0); i < Parameters::closest_nodes_size; ++i) {
    NodeInfo node;
    node.node_id = NodeId(NodeId::kRandomId);
    row_ids.push_back(node);
  }
  EXPECT_EQ(0, matrix.GetUniqueNodes().size());
  std::vector<NodeInfo> node_ids;
  node_ids.push_back(node_0);
  for (auto row_id : row_ids) {
    node_ids.push_back(row_id);
    SortNodeInfosFromTarget(node_0.node_id, node_ids);
    matrix.AddConnectedPeer(row_id);
    EXPECT_TRUE(CompareListOfNodeInfos(node_ids, matrix.GetUniqueNodes()));
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
    matrix.UpdateFromConnectedPeer(row_id.node_id, row_entries);
    SortNodeInfosFromTarget(node_0.node_id, node_ids);
    EXPECT_TRUE(CompareListOfNodeInfos(node_ids, matrix.GetUniqueNodes()));
  }
}

}  // namespace test
}  // namespace routing
}  // namespace maidsafe

