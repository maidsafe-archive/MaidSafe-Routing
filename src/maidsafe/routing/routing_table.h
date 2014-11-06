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

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/rsa.h"

#include "maidsafe/passport/types.h"

#include "maidsafe/routing/types.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/close_nodes_change.h"
#include "maidsafe/routing/routing_table_change.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {


typedef std::function<void(const RoutingTableChange& /*routing_table_change*/)>
    RoutingTableChangeFunctor;

class RoutingTable {
 public:
  RoutingTable(const NodeId& our_id, const asymm::Keys& keys);
  RoutingTable(const RoutingTable&) = default;
  RoutingTable(RoutingTable&&) = default;
  RoutingTable& operator=(const RoutingTable&) = delete;
  RoutingTable& operator=(const RoutingTable&&) = delete;
  virtual ~RoutingTable() = default;
  void InitialiseFunctors(RoutingTableChangeFunctor routing_table_change_functor);
  bool AddNode(const NodeInfo& their_id);
  bool CheckNode(const NodeInfo& their_id);
  NodeInfo DropNode(const NodeId& node_to_drop, bool routing_only);
  // If more than 1 node returned then we are in close group so send to all !!
  std::vector<NodeInfo> GetTargetNodes(NodeId their_id);
  // our close group or at least as much of it as we currently know
  std::vector<NodeInfo> GetGroupNodes();

  size_t size() const;
  NodeId kNodeId() const { return kNodeId_; }
  asymm::PrivateKey kPrivateKey() const { return kKeys_.private_key; }
  asymm::PublicKey kPublicKey() const { return kKeys_.public_key; }

 private:
  bool AddOrCheckNode(NodeInfo node, bool remove);
  void SetBucketIndex(NodeInfo& node_info) const;

  /** Attempts to find or allocate space for an incomming connect request, returning true
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

  std::pair<bool, std::vector<NodeInfo>::iterator> Find(const NodeId& node_id,
                                                        std::unique_lock<std::mutex>& lock);
  std::pair<bool, std::vector<NodeInfo>::const_iterator> Find(
      const NodeId& node_id, std::unique_lock<std::mutex>& lock) const;

  unsigned int NetworkStatus(unsigned int size) const;

  std::string PrintRoutingTable();
  const NodeId kNodeId_;
  const NodeId kConnectionId_;
  const asymm::Keys kKeys_;
  mutable std::mutex mutex_;
  RoutingTableChangeFunctor routing_table_change_functor_;
  std::vector<NodeInfo> nodes_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_TABLE_H_
