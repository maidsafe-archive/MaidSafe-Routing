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
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

namespace test {
class GenericNode;
class RoutingTableTest;
class RoutingTableTest_BEH_CheckMockSendGroupChangeRpcs_Test;
class NetworkStatisticsTest_BEH_IsIdInGroupRange_Test;
class RoutingTableTest_FUNC_IsNodeIdInGroupRange_Test;
struct RoutingTableInfo;
class RoutingTableNetwork;
}

namespace protobuf {
class Contact;
}

struct NodeInfo;

struct RoutingTableChange {
  struct Remove {
    Remove() : node(), routing_only_removal(true) {}
    Remove(NodeInfo& node_in, bool routing_only_removal_in)
        : node(node_in), routing_only_removal(routing_only_removal_in) {}
    NodeInfo node;
    bool routing_only_removal;
  };
  RoutingTableChange() : added_node(), removed(), insertion(false), close_nodes_change(),
                         health(0) {}
  RoutingTableChange(const NodeInfo& added_node_in, const Remove& removed_in,
                     bool insertion_in, std::shared_ptr<CloseNodesChange> close_nodes_change_in,
                     unsigned int health_in)
      : added_node(added_node_in), removed(removed_in), insertion(insertion_in),
        close_nodes_change(close_nodes_change_in), health(health_in) {}
  NodeInfo added_node;
  Remove removed;
  bool insertion;
  std::shared_ptr<CloseNodesChange> close_nodes_change;
  unsigned int health;
};

typedef std::function<void(const RoutingTableChange& /*routing_table_change*/)>
    RoutingTableChangeFunctor;

class RoutingTable {
 public:
  RoutingTable(bool client_mode, const NodeId& node_id, const asymm::Keys& keys);
  virtual ~RoutingTable();
  void InitialiseFunctors(RoutingTableChangeFunctor routing_table_change_functor);
  bool AddNode(const NodeInfo& peer);
  bool CheckNode(const NodeInfo& peer);
  NodeInfo DropNode(const NodeId& node_to_drop, bool routing_only);

  bool IsThisNodeInRange(const NodeId& target_id, unsigned int range);
  bool IsThisNodeClosestTo(const NodeId& target_id, bool ignore_exact_match = false);
  bool Contains(const NodeId& node_id) const;
  bool ConfirmGroupMembers(const NodeId& node1, const NodeId& node2);

  bool GetNodeInfo(const NodeId& node_id, NodeInfo& node_info) const;
  // Returns default-constructed NodeId if routing table size is zero
  NodeInfo GetClosestNode(const NodeId& target_id,
                          bool ignore_exact_match = false,
                          const std::vector<std::string>& exclude = std::vector<std::string>());
  std::vector<NodeInfo> GetClosestNodes(const NodeId& target_id, unsigned int number_to_get,
                                        bool ignore_exact_match = false);
  NodeInfo GetNthClosestNode(const NodeId& target_id, unsigned int index);
  NodeId RandomConnectedNode();

  size_t size() const;
  unsigned int kThresholdSize() const { return kThresholdSize_; }
  unsigned int kMaxSize() const { return kMaxSize_; }
  NodeId kNodeId() const { return kNodeId_; }
  asymm::PrivateKey kPrivateKey() const { return kKeys_.private_key; }
  asymm::PublicKey kPublicKey() const { return kKeys_.public_key; }
  NodeId kConnectionId() const { return kConnectionId_; }
  bool client_mode() const { return kClientMode_; }

  friend class test::GenericNode;
  friend class test::RoutingTableTest;
  friend class test::RoutingTableTest_BEH_CheckMockSendGroupChangeRpcs_Test;
  friend class test::NetworkStatisticsTest_BEH_IsIdInGroupRange_Test;
  friend class test::RoutingTableTest_FUNC_IsNodeIdInGroupRange_Test;
  friend struct test::RoutingTableInfo;
  friend class test::RoutingTableNetwork;

 private:
  RoutingTable(const RoutingTable&);
  RoutingTable& operator=(const RoutingTable&);
  bool AddOrCheckNode(NodeInfo node, bool remove);
  void SetBucketIndex(NodeInfo& node_info) const;
  bool CheckPublicKeyIsUnique(const NodeInfo& node, std::unique_lock<std::mutex>& lock) const;

  /** Attempts to find or allocate memory for an incomming connect request, returning true
   * indicates approval
   * returns true if routing table is not full, otherwise, performs the following process to
   * possibly evict an existing node:
   * - sorts the nodes according to their distance from self-node-id
   * - a candidate for eviction must have an index > Parameters::unidirectional_interest_range
   * - count the number of nodes in each bucket for nodes with
   *    index > Parameters::unidirectional_interest_range
   * - choose the furthest node among the nodes with maximum bucket index
   * - in case more than one bucket have similar maximum bucket size, the furthest node in higher
   *    bucket will be evicted
   * - remove the selected node and return true **/
  bool MakeSpaceForNodeToBeAdded(const NodeInfo& node, bool remove, NodeInfo& removed_node,
                                 std::unique_lock<std::mutex>& lock);

  unsigned int PartialSortFromTarget(const NodeId& target, unsigned int number,
                                     std::unique_lock<std::mutex>& lock);
  void NthElementSortFromTarget(const NodeId& target, unsigned int nth_element,
                                std::unique_lock<std::mutex>& lock);
  std::pair<bool, std::vector<NodeInfo>::iterator> Find(const NodeId& node_id,
                                                        std::unique_lock<std::mutex>& lock);
  std::pair<bool, std::vector<NodeInfo>::const_iterator> Find(
      const NodeId& node_id, std::unique_lock<std::mutex>& lock) const;

  unsigned int NetworkStatus(unsigned int size) const;

  void IpcSendCloseNodes();
  std::string Print();

  const bool kClientMode_;
  const NodeId kNodeId_;
  const NodeId kConnectionId_;
  const asymm::Keys kKeys_;
  const unsigned int kMaxSize_;
  const unsigned int kThresholdSize_;
  mutable std::mutex mutex_;
  RoutingTableChangeFunctor routing_table_change_functor_;
  std::vector<NodeInfo> nodes_;
  std::unique_ptr<boost::interprocess::message_queue> ipc_message_queue_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_TABLE_H_
