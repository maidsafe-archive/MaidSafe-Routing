/*  Copyright 2012 MaidSafe.net limited

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

#ifndef MAIDSAFE_ROUTING_GROUP_MATRIX_H_
#define MAIDSAFE_ROUTING_GROUP_MATRIX_H_

#include <cstdint>
#include <mutex>
#include <vector>
#include <string>

#include "maidsafe/common/crypto.h"
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

  std::shared_ptr<MatrixChange> AddConnectedPeer(
      const NodeInfo& node_info,
      const std::vector<NodeInfo>& matrix_update = std::vector<NodeInfo>());

  std::shared_ptr<MatrixChange> RemoveConnectedPeer(const NodeInfo& node_info);

  // Returns the connected peers sorted to node ids from kNodeId_
  std::vector<NodeInfo> GetConnectedPeers() const;

  // Returns the peer which has target_info in its row (1st occurrence).
  NodeInfo GetConnectedPeerFor(const NodeId& target_node_id);

  // Returns the peer which has node closest to target_id in its row (1st occurrence).
  void GetBetterNodeForSendingMessage(const NodeId& target_node_id,
                                      const std::vector<std::string>& exclude,
                                      bool ignore_exact_match, NodeInfo& current_closest_peer);
  void GetBetterNodeForSendingMessage(const NodeId& target_node_id, bool ignore_exact_match,
                                      NodeId& current_closest_peer_id);
  std::vector<NodeInfo> GetAllConnectedPeersFor(const NodeId& target_id);
  bool IsThisNodeGroupLeader(const NodeId& target_id, NodeId& connected_peer);

  bool ClosestToId(const NodeId& target_id);
  //  bool IsNodeIdInGroupRange(const NodeId& group_id, const NodeId& node_id);
  GroupRangeStatus IsNodeIdInGroupRange(const NodeId& group_id, const NodeId& node_id) const;
  // Updates group matrix if peer is present in 1st column of matrix
  std::shared_ptr<MatrixChange> UpdateFromConnectedPeer(const NodeId& peer,
                                                        const std::vector<NodeInfo>& nodes,
                                                        const std::vector<NodeId>& old_unique_ids);
  void UpdateFromUnvalidatedPeer(const NodeId& peer, const std::vector<NodeInfo>& nodes);

  bool IsRowEmpty(const NodeInfo& node_info);
  bool GetRow(const NodeId& row_id, std::vector<NodeInfo>& row_entries);
  std::vector<NodeInfo> GetUniqueNodes() const;
  std::vector<NodeId> GetUniqueNodeIds() const;
  std::vector<NodeInfo> GetClosestNodes(uint16_t size);
  bool Contains(const NodeId& node_id);
  void Prune();

  friend class RoutingTable;
  friend class test::GenericNode;
  friend class test::NetworkStatisticsTest_BEH_IsIdInGroupRange_Test;
  friend class test::GroupMatrixTest_BEH_Prune_Test;

 private:
  GroupMatrix(const GroupMatrix&);
  GroupMatrix& operator=(const GroupMatrix&);
  void UpdateUniqueNodeList();
  void PartialSortFromTarget(const NodeId& target, uint16_t number,
                             std::vector<NodeInfo>& nodes);
  void PrintGroupMatrix() const;

  const NodeId& kNodeId_;
  std::vector<NodeInfo> unique_nodes_;
  crypto::BigInt radius_;
  bool client_mode_;
  std::vector<std::vector<NodeInfo>> matrix_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_GROUP_MATRIX_H_
