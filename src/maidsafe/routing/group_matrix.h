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

#ifndef MAIDSAFE_ROUTING_GROUP_MATRIX_H_
#define MAIDSAFE_ROUTING_GROUP_MATRIX_H_

#include <cstdint>
#include <mutex>
#include <vector>
#include <string>

#include "maidsafe/common/crypto.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/routing/node_info.h"

namespace maidsafe {

namespace routing {

namespace test {
  class GenericNode;
  class RoutingTableTest_BEH_IsIdInGroupRange_Test;
  class GroupMatrixTest_BEH_AverageDistance_Test;
}

class RoutingTable;


struct NodeInfo;

class GroupMatrix {
 public:
  explicit GroupMatrix(const NodeId& this_node_id);

  void AddConnectedPeer(const NodeInfo& node_info);

  void RemoveConnectedPeer(const NodeInfo& node_info);

  // Returns the connected peers sorted to node ids from kNodeId_
  std::vector<NodeInfo> GetConnectedPeers();

  // Returns the peer which has target_info in its row (1st occurrence).
  NodeInfo GetConnectedPeerFor(const NodeId& target_node_id);

  // Returns the peer which has node closest to target_id in its row (1st occurrence).
  NodeInfo GetConnectedPeerClosestTo(const NodeId& target_node_id);

  bool IsNodeInGroupRange(const NodeId& target_id);

  bool IsIdInGroup(const NodeId& sender_id, const NodeId& info_id);

  bool IsThisNodeGroupLeader(const NodeId& target_id, NodeId& group_leader_id);

  // Updates group matrix if peer is present in 1st column of matrix
  void UpdateFromConnectedPeer(const NodeId& peer, const std::vector<NodeInfo>& nodes);

  bool IsRowEmpty(const NodeInfo& node_info);
  bool GetRow(const NodeId& row_id, std::vector<NodeInfo>& row_entries);

  std::vector<NodeInfo> GetUniqueNodes();

  std::vector<NodeInfo> GetClosestNodes(const uint32_t& size);

  void Clear();
  void Distance();
  void AverageDistance(const NodeId& distance);

  friend class test::GenericNode;
  friend class test::RoutingTableTest_BEH_IsIdInGroupRange_Test;
  friend class test::GroupMatrixTest_BEH_AverageDistance_Test;

 private:
  GroupMatrix(const GroupMatrix&);
  GroupMatrix& operator=(const GroupMatrix&);
  struct NetworkDistanceData {
    NetworkDistanceData() : contributors_count(), total_distance(), average_distance() {}
    crypto::BigInt contributors_count, total_distance;
    NodeId average_distance;
  };
  void UpdateUniqueNodeList();
  void PartialSortFromTarget(const NodeId& target, const uint16_t& number,
                             std::vector<NodeInfo> &nodes);
  void PrintGroupMatrix();

  const NodeId& kNodeId_;
  std::vector<NodeInfo> unique_nodes_;
  std::vector<std::vector<NodeInfo>> matrix_;
  NodeId distance_;
  NetworkDistanceData network_distance_data_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_GROUP_MATRIX_H_
