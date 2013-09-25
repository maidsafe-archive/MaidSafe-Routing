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

#ifndef MAIDSAFE_ROUTING_GROUP_CHANGE_HANDLER_H_
#define MAIDSAFE_ROUTING_GROUP_CHANGE_HANDLER_H_

#include <vector>

#include "maidsafe/common/node_id.h"

#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/routing.pb.h"

namespace maidsafe {

namespace routing {

namespace test {
class GenericNode;
}

class RoutingTable;
class ClientRoutingTable;

namespace protobuf {
class Message;
}

class GroupChangeHandler {
 public:
  GroupChangeHandler(RoutingTable& routing_table, ClientRoutingTable& client_routing_table,
                     NetworkUtils& network);
  ~GroupChangeHandler();
  void SendClosestNodesUpdateRpcs(std::vector<NodeInfo> new_close_nodes);
  void UpdateGroupChange(const NodeId& node_id, std::vector<NodeInfo> close_nodes);
  void ClosestNodesUpdate(protobuf::Message& message);
  void SendSubscribeRpc(const bool& subscribe, const NodeInfo& node_info);

  friend class test::GenericNode;

 private:
  GroupChangeHandler(const GroupChangeHandler&);
  GroupChangeHandler& operator=(const GroupChangeHandler&);

  void Subscribe(const NodeId& node_id, const NodeId& connection_id);
  bool GetNodeInfo(const NodeId& node_id, const NodeId& connection_id, NodeInfo& out_node_info);

  RoutingTable& routing_table_;
  ClientRoutingTable& client_routing_table_;
  NetworkUtils& network_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_GROUP_CHANGE_HANDLER_H_
