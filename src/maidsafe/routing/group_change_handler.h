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
  void SendClosestNodesUpdateRpcs(const std::vector<NodeInfo>& new_close_nodes);
  void UpdateGroupChange(const NodeId& node_id, std::vector<NodeInfo> close_nodes);
  void ClosestNodesUpdate(protobuf::Message& message);
  void SendSubscribeRpc(const bool& subscribe, const NodeInfo& node_info);
  void ClosestNodesUpdateSubscribe(protobuf::Message& message);
  void Unsubscribe(const NodeId& connection_id);

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
