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
#include "maidsafe/routing/network_statistics.h"
#include "maidsafe/routing/parameters.h"

namespace maidsafe {

namespace routing {

namespace test {
class GenericNode;
class RoutingTableTest;
class RoutingTableTest_BEH_CheckMockSendGroupChangeRpcs_Test;
class NetworkStatisticsTest_BEH_IsIdInGroupRange_Test;
class RoutingTableTest_FUNC_IsNodeIdInGroupRange_Test;
}

namespace protobuf {
class Contact;
}

struct NodeInfo;

class RoutingTable {
 public:
  RoutingTable(bool client_mode, const NodeId& node_id, const asymm::Keys& keys,
               NetworkStatistics& network_statistics);
  virtual ~RoutingTable();
  void InitialiseFunctors(NetworkStatusFunctor network_status_functor,
                          std::function<void(const NodeInfo&, bool)> remove_node_functor,
                          MatrixChangedFunctor matrix_change_functor);
  bool AddNode(const NodeInfo& peer);
  bool CheckNode(const NodeInfo& peer);
  NodeInfo DropNode(const NodeId& node_to_drop, bool routing_only);

  GroupRangeStatus IsNodeIdInGroupRange(const NodeId& group_id) const;
  GroupRangeStatus IsNodeIdInGroupRange(const NodeId& group_id, const NodeId& node_id) const;
  bool IsThisNodeInRange(const NodeId& target_id, uint16_t range);
  bool IsThisNodeClosestTo(const NodeId& target_id, bool ignore_exact_match = false);
  bool Contains(const NodeId& node_id) const;
  bool ConfirmGroupMembers(const NodeId& node1, const NodeId& node2);

  bool GetNodeInfo(const NodeId& node_id, NodeInfo& node_info) const;
  // Returns default-constructed NodeId if routing table size is zero
  NodeInfo GetClosestNode(const NodeId& target_id,
                          bool ignore_exact_match = false,
                          const std::vector<std::string>& exclude = std::vector<std::string>());
  std::vector<NodeInfo> GetClosestNodes(const NodeId& target_id, uint16_t number_to_get,
                                        bool ignore_exact_match = false);
  NodeInfo GetNthClosestNode(const NodeId& target_id, uint16_t index);
  NodeId RandomConnectedNode();

  size_t size() const;
  uint16_t kThresholdSize() const { return kThresholdSize_; }
  uint16_t kMaxSize() const { return kMaxSize_; }
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

 private:
  RoutingTable(const RoutingTable&);
  RoutingTable& operator=(const RoutingTable&);
  bool AddOrCheckNode(NodeInfo node, bool remove);
  void SetBucketIndex(NodeInfo& node_info) const;
  bool CheckPublicKeyIsUnique(const NodeInfo& node, std::unique_lock<std::mutex>& lock) const;
  bool MakeSpaceForNodeToBeAdded(const NodeInfo& node, bool remove, NodeInfo& removed_node,
                                 std::unique_lock<std::mutex>& lock);

  uint16_t PartialSortFromTarget(const NodeId& target, uint16_t number,
                                 std::unique_lock<std::mutex>& lock);
  void NthElementSortFromTarget(const NodeId& target, uint16_t nth_element,
                                std::unique_lock<std::mutex>& lock);
  std::pair<bool, std::vector<NodeInfo>::iterator> Find(const NodeId& node_id,
                                                        std::unique_lock<std::mutex>& lock);
  std::pair<bool, std::vector<NodeInfo>::const_iterator> Find(
      const NodeId& node_id, std::unique_lock<std::mutex>& lock) const;

  void UpdateNetworkStatus(uint16_t size) const;

/* remove group matrix
  void IpcSendGroupMatrix() const;
*/
  std::string PrintRoutingTable();

  const bool kClientMode_;
  const NodeId kNodeId_;
  const NodeId kConnectionId_;
  const asymm::Keys kKeys_;
  const uint16_t kMaxSize_;
  const uint16_t kThresholdSize_;
  mutable std::mutex mutex_;
  std::function<void(const NodeInfo&, bool)> remove_node_functor_;
  NetworkStatusFunctor network_status_functor_;
  MatrixChangedFunctor matrix_change_functor_;
  std::vector<NodeInfo> nodes_;
  std::unique_ptr<boost::interprocess::message_queue> ipc_message_queue_;
  NetworkStatistics& network_statistics_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_TABLE_H_
