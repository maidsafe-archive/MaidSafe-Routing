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

#ifndef MAIDSAFE_ROUTING_CLIENT_ROUTING_TABLE_H_
#define MAIDSAFE_ROUTING_CLIENT_ROUTING_TABLE_H_

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "boost/asio/ip/udp.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/rsa.h"

#include "maidsafe/routing/api_config.h"

namespace maidsafe {

namespace routing {

struct NodeInfo;

namespace test {
class GenericNode;
class BasicClientRoutingTableTest;
class BasicClientRoutingTableTest_BEH_IsThisNodeInRange_Test;
}

class GroupChangeHandler;

namespace protobuf {
class Contact;
}

class ClientRoutingTable {
 public:
  explicit ClientRoutingTable(NodeId node_id);
  bool AddNode(NodeInfo& node, const NodeId& furthest_close_node_id);
  bool CheckNode(NodeInfo& node, const NodeId& furthest_close_node_id);
  std::vector<NodeInfo> DropNodes(const NodeId& node_to_drop);
  NodeInfo DropConnection(const NodeId& connection_to_drop);
  std::vector<NodeInfo> GetNodesInfo(const NodeId& node_id = NodeId()) const;
  bool Contains(const NodeId& node_id) const;
  bool IsConnected(const NodeId& node_id) const;
  size_t size() const;
  NodeId kNodeId() const { return kNodeId_; }

  friend class test::GenericNode;
  friend class GroupChangeHandler;

 private:
  ClientRoutingTable(const ClientRoutingTable&);
  ClientRoutingTable& operator=(const ClientRoutingTable&);
  bool AddOrCheckNode(NodeInfo& node, const NodeId& furthest_close_node_id, bool add);
  bool CheckValidParameters(const NodeInfo& node) const;
  bool CheckParametersAreUnique(const NodeInfo& node) const;
  bool CheckRangeForNodeToBeAdded(NodeInfo& node, const NodeId& furthest_close_node_id,
                                  bool add) const;
  bool IsThisNodeInRange(const NodeId& node_id, const NodeId& furthest_close_node_id) const;
  std::string PrintClientRoutingTable();

  friend class test::BasicClientRoutingTableTest;
  friend class test::BasicClientRoutingTableTest_BEH_IsThisNodeInRange_Test;

  const NodeId kNodeId_;
  std::vector<NodeInfo> nodes_;
  mutable std::mutex mutex_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_CLIENT_ROUTING_TABLE_H_
