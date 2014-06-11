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
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "maidsafe/common/crypto.h"
#include "maidsafe/common/node_id.h"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/node_info.h"

namespace maidsafe {

namespace routing {

namespace test {
//class GenericNode;
//class NetworkStatisticsTest_BEH_IsIdInGroupRange_Test;
//class GroupMatrixTest_BEH_Prune_Test;
}

class GroupMatrix {
 public:
  typedef std::set<NodeInfo, std::function<bool(const NodeInfo&, const NodeInfo&)>> SortedGroup;

  // If 'client_mode' is false, this node is added to 'unique_nodes_', and there may be "forced
  // connections", i.e. 'matrix_' size could exceed 'Parameters::closest_nodes_size'.
  GroupMatrix(const NodeId& this_node_id, bool client_mode);

  // 'connected_peer' should not already have been added.
  std::shared_ptr<MatrixChange> AddConnectedPeer(
      const NodeInfo& connected_peer,
      const std::vector<NodeInfo>& peers_close_connections = std::vector<NodeInfo>());

  // 'connected_peer' should already have been added.
  std::shared_ptr<MatrixChange> UpdateConnectedPeer(const NodeId& connected_peer_id,
      const std::vector<NodeInfo>& peers_close_connections,
      const std::vector<NodeId>& old_unique_ids);

  // No failure if 'connected_peer' is not in matrix.
  std::shared_ptr<MatrixChange> RemoveConnectedPeer(const NodeInfo& connected_peer);

  // Returns the connected peers' info sorted by XOR distance from kNodeId_, excluding this node's
  // info.
  std::vector<NodeInfo> GetConnectedPeers() const;

  // Returns the connected peers' info, their connected peers' info and this node's info if it's not
  // "client mode".  Returns collection sorted by XOR distance from kNodeId_.
  std::vector<NodeInfo> GetUniqueNodes() const;

  // Returns the connected peers' IDs, their connected peers' IDs and this node's ID if it's not
  // "client mode".  Returns collection sorted by XOR distance from kNodeId_.
  std::vector<NodeId> GetUniqueNodeIds() const;

  // Tries to find a peer which is closer to 'target_id' than 'current_closest_peer'.  If found, and
  // this node is connected to the closest peer, the function returns the peer's info.  If this node
  // is *not* connected to the closest peer, the function returns a peer which is connected to both
  // this node and the closest.  If no closer peer is found, the function returns a
  // default-constructed unique_ptr.  This node is not considered, only peers.
  std::unique_ptr<NodeInfo> GetBetterNodeForSendingMessage(const NodeId& target_id,
      bool ignore_exact_match, const NodeInfo& current_closest_peer,
      std::vector<NodeId> exclusions = std::vector<NodeId>()) const;




  bool IsThisNodeClosestToId(const NodeId& target_id) const;
  GroupRangeStatus IsNodeIdInGroupRange(const NodeId& group_id, const NodeId& node_id) const;

//#ifdef TESTING
//  // Returns the peer which has target_info in its row (1st occurrence).
//  NodeInfo GetConnectedPeerFor(const NodeId& target_node_id) const;                                                 //Do we *really* need this?
//  bool GetRow(const NodeId& row_id, std::vector<NodeInfo>& row_entries) const;                                //Do we *really* need this?
//#endif

  //friend class test::GenericNode;
  //friend class test::NetworkStatisticsTest_BEH_IsIdInGroupRange_Test;
  //friend class test::GroupMatrixTest_BEH_Prune_Test;

 private:
  typedef std::map<NodeInfo, SortedGroup,
                   std::function<bool(const NodeInfo&, const NodeInfo&)>> Matrix;
  GroupMatrix(const GroupMatrix&) = delete;
  GroupMatrix(GroupMatrix&&) = delete;
  GroupMatrix& operator=(const GroupMatrix&) = delete;

  void ValidatePeersCloseConnections(const NodeId& connected_peer_id,
                                     const std::vector<NodeInfo>& peers_close_connections) const;
  void Prune();
  void UpdateUniqueNodeList();
  void PrintGroupMatrix() const;

  const NodeId kNodeId_;
  SortedGroup unique_nodes_;
  crypto::BigInt radius_;
  const bool kClientMode_;
  Matrix matrix_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_GROUP_MATRIX_H_
