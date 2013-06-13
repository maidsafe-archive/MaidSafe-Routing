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

#include "maidsafe/common/node_id.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/api_config.h"

namespace maidsafe {

namespace routing {

namespace test {
  class GenericNode;
  class NetworkStatisticsTest_BEH_IsIdInGroupRange_Test;
  class GroupMatrixTest_BEH_Prune_Test;
}

class RoutingTable;


struct NodeInfo;

class GroupMatrix {
 public:
  explicit GroupMatrix(const NodeId& this_node_id, bool client_mode);

  void AddConnectedPeer(const NodeInfo& node_info);

  void RemoveConnectedPeer(const NodeInfo& node_info, MatrixChange& matrix_change);

  // Returns the connected peers sorted to node ids from kNodeId_
  std::vector<NodeInfo> GetConnectedPeers() const;

  // Returns the peer which has target_info in its row (1st occurrence).
  NodeInfo GetConnectedPeerFor(const NodeId& target_node_id);

  // Returns the peer which has node closest to target_id in its row (1st occurrence).
  void GetBetterNodeForSendingMessage(const NodeId& target_node_id,
                                      const std::vector<std::string>& exclude,
                                      bool ignore_exact_match,
                                      NodeInfo& current_closest_peer);
  void GetBetterNodeForSendingMessage(const NodeId& target_node_id,
                                      bool ignore_exact_match,
                                      NodeId& current_closest_peer_id);
  std::vector<NodeInfo> GetAllConnectedPeersFor(const NodeId& target_id);
  bool IsThisNodeGroupLeader(const NodeId& target_id, NodeId& connected_peer);
  bool ClosestToId(const NodeId& node_id);
  bool IsNodeIdInGroupRange(const NodeId& target_id);

  // Updates group matrix if peer is present in 1st column of matrix
  void UpdateFromConnectedPeer(const NodeId& peer, const std::vector<NodeInfo>& nodes);
  bool IsRowEmpty(const NodeInfo& node_info);
  bool GetRow(const NodeId& row_id, std::vector<NodeInfo>& row_entries);
  std::vector<NodeInfo> GetUniqueNodes() const;
  std::vector<NodeId> GetUniqueNodeIds() const;
  std::vector<NodeInfo> GetClosestNodes(const uint16_t& size);
  bool Contains(const NodeId& node_id);
  void Prune();

  friend class test::GenericNode;
  friend class test::NetworkStatisticsTest_BEH_IsIdInGroupRange_Test;
  friend class test::GroupMatrixTest_BEH_Prune_Test;

 private:
  GroupMatrix(const GroupMatrix&);
  GroupMatrix& operator=(const GroupMatrix&);
  void UpdateUniqueNodeList();
  void PartialSortFromTarget(const NodeId& target, const uint16_t& number,
                             std::vector<NodeInfo> &nodes);
  void PrintGroupMatrix();

  const NodeId& kNodeId_;
  std::vector<NodeInfo> unique_nodes_;
  bool client_mode_;
  std::vector<std::vector<NodeInfo>> matrix_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_GROUP_MATRIX_H_
