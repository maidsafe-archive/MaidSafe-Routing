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

#ifndef MAIDSAFE_ROUTING_ROUTING_TABLE_H_
#define MAIDSAFE_ROUTING_ROUTING_TABLE_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "boost/asio/ip/udp.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/interprocess/ipc/message_queue.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/rsa.h"

#include "maidsafe/passport/types.h"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/group_matrix.h"
#include "maidsafe/routing/network_statistics.h"
#include "maidsafe/routing/parameters.h"


namespace maidsafe {

namespace routing {

class GroupChangeHandler;

namespace test {
  class GenericNode;
  class RoutingTableTest;
  class RoutingTableTest_BEH_OrderedGroupChange_Test;
  class RoutingTableTest_BEH_ReverseOrderedGroupChange_Test;
  class RoutingTableTest_BEH_CheckMockSendGroupChangeRpcs_Test;
  class RoutingTableTest_BEH_GroupUpdateFromConnectedPeer_Test;
  class NetworkStatisticsTest_BEH_IsIdInGroupRange_Test;
}

namespace protobuf { class Contact; }

struct NodeInfo;

typedef std::function<void(std::vector<NodeInfo> /*new_group*/)>
    ConnectedGroupChangeFunctor;


class RoutingTable {
 public:
  RoutingTable(bool client_mode, const NodeId& node_id, const asymm::Keys& keys,
               NetworkStatistics& network_statistics);
  virtual ~RoutingTable();
  void InitialiseFunctors(NetworkStatusFunctor network_status_functor,
                          std::function<void(const NodeInfo&, bool)> remove_node_functor,
                          RemoveFurthestUnnecessaryNode remove_furthest_node,
                          ConnectedGroupChangeFunctor connected_group_change_functor,
                          CloseNodeReplacedFunctor close_node_replaced_functor);
  bool AddNode(const NodeInfo& peer);
  bool CheckNode(const NodeInfo& peer);
  NodeInfo DropNode(const NodeId &node_to_drop, bool routing_only);
  bool ClosestToId(const NodeId& node_id);
  GroupRangeStatus IsNodeIdInGroupRange(const NodeId& target_id);
  bool IsThisNodeGroupLeader(const NodeId& target_id, NodeInfo& connected_peer);
  bool IsThisNodeGroupLeader(const NodeId& target_id,
                             NodeInfo& connected_peer,
                             const std::vector<std::string>& exclude);
  bool GetNodeInfo(const NodeId& node_id, NodeInfo& node_info) const;
  bool IsThisNodeInRange(const NodeId& target_id, uint16_t range);
  bool IsThisNodeClosestTo(const NodeId& target_id, bool ignore_exact_match = false);
  bool IsThisNodeClosestToIncludingMatrix(const NodeId& target_id, bool ignore_exact_match = false);
  bool Contains(const NodeId& node_id) const;
  bool ConfirmGroupMembers(const NodeId& node1, const NodeId& node2);
  void GroupUpdateFromConnectedPeer(const NodeId& peer, const std::vector<NodeInfo>& nodes);
  NodeId RandomConnectedNode();
  std::vector<NodeInfo> GetMatrixNodes();
  bool IsConnected(const NodeId& node_id);
  // Returns default-constructed NodeId if routing table size is zero
  NodeInfo GetClosestNode(const NodeId& target_id, bool ignore_exact_match = false);
  NodeInfo GetClosestNode(const NodeId& target_id,
                          const std::vector<std::string>& exclude,
                          bool ignore_exact_match = false);
//  NodeInfo GetNodeForSendingMessage(const NodeId& target_id, bool ignore_exact_match = false);
  NodeInfo GetNodeForSendingMessage(const NodeId& target_id,
                                    const std::vector<std::string>& exclude,
                                    bool ignore_exact_match = false);
  // Returns max NodeId if routing table size is less than requested node_number
  NodeInfo GetNthClosestNode(const NodeId& target_id, uint16_t node_number);
  std::vector<NodeId> GetClosestNodes(const NodeId& target_id, uint16_t number_to_get);
  std::vector<NodeInfo> GetClosestMatrixNodes(const NodeId& target_id, uint16_t number_to_get);
  std::vector<NodeId> GetGroup(const NodeId& target_id);
  NodeInfo GetRemovableNode(std::vector<std::string> attempted = std::vector<std::string>());
  void GetNodesNeedingGroupUpdates(std::vector<NodeInfo>& nodes_needing_update);
  bool UnsubscribeToReceivingGroupUpdate(const NodeId& node_id);
  size_t size() const;
  uint16_t kThresholdSize() const { return kThresholdSize_; }
  NodeId kNodeId() const { return kNodeId_; }
  asymm::PrivateKey kPrivateKey() const { return kKeys_.private_key; }
  asymm::PublicKey kPublicKey() const { return kKeys_.public_key; }
  NodeId kConnectionId() const { return kConnectionId_; }
  bool client_mode() const { return kClientMode_; }

  friend class test::GenericNode;
  friend class GroupChangeHandler;
  friend class test::RoutingTableTest;
  friend class test::RoutingTableTest_BEH_OrderedGroupChange_Test;
  friend class test::RoutingTableTest_BEH_ReverseOrderedGroupChange_Test;
  friend class test::RoutingTableTest_BEH_CheckMockSendGroupChangeRpcs_Test;
  friend class test::RoutingTableTest_BEH_GroupUpdateFromConnectedPeer_Test;
  friend class test::NetworkStatisticsTest_BEH_IsIdInGroupRange_Test;

 private:
  RoutingTable(const RoutingTable&);
  RoutingTable& operator=(const RoutingTable&);
  bool AddOrCheckNode(NodeInfo node, bool remove);
  void SetBucketIndex(NodeInfo& node_info) const;
  bool CheckPublicKeyIsUnique(const NodeInfo& node, std::unique_lock<std::mutex>& lock) const;
  NodeInfo ResolveConnectionDuplication(const NodeInfo& new_duplicate_node,
                                        bool local_endpoint,
                                        NodeInfo& existing_node);
  void UpdateCloseNodeChange(std::unique_lock<std::mutex>& lock, const NodeInfo& peer,
                             std::vector<NodeInfo>& new_connected_nodes);
  bool MakeSpaceForNodeToBeAdded(const NodeInfo& node,
                                 bool remove,
                                 NodeInfo& removed_node,
                                 std::unique_lock<std::mutex>& lock);
  uint16_t PartialSortFromTarget(const NodeId& target,
                                 uint16_t number,
                                 std::unique_lock<std::mutex>& lock);
  void NthElementSortFromTarget(const NodeId& target,
                                uint16_t nth_element,
                                std::unique_lock<std::mutex>& lock);
  NodeId FurthestCloseNode();
  std::vector<NodeInfo> GetClosestNodeInfo(const NodeId& target_id,
                                           uint16_t number_to_get,
                                           bool ignore_exact_match = false);
  std::pair<bool, std::vector<NodeInfo>::iterator> Find(const NodeId& node_id,
                                                        std::unique_lock<std::mutex>& lock);
  std::pair<bool, std::vector<NodeInfo>::const_iterator> Find(
      const NodeId& node_id,
      std::unique_lock<std::mutex>& lock) const;
  void UpdateNetworkStatus(uint16_t size) const;

  void IpcSendGroupMatrix() const;
  std::string PrintRoutingTable();
  void PrintGroupMatrix();

  const bool kClientMode_;
  const NodeId kNodeId_;
  const NodeId kConnectionId_;
  const asymm::Keys kKeys_;
  const uint16_t kMaxSize_;
  const uint16_t kThresholdSize_;
  mutable std::mutex mutex_;
  NodeId furthest_group_node_id_;
  std::function<void(const NodeInfo&, bool)> remove_node_functor_;
  NetworkStatusFunctor network_status_functor_;
  RemoveFurthestUnnecessaryNode remove_furthest_node_;
  ConnectedGroupChangeFunctor connected_group_change_functor_;
  CloseNodeReplacedFunctor close_node_replaced_functor_;
  std::vector<NodeInfo> nodes_;
  GroupMatrix group_matrix_;
  std::unique_ptr<boost::interprocess::message_queue> ipc_message_queue_;
  NetworkStatistics& network_statistics_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_TABLE_H_
