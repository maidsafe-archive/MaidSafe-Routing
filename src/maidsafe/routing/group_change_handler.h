/* Copyright 2012 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

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

namespace protobuf { class Message; }

class GroupChangeHandler {
 public:
  GroupChangeHandler(RoutingTable& routing_table,
                     ClientRoutingTable& client_routing_table,
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
