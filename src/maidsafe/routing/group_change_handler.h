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
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/routing_pb.h"


namespace maidsafe {

namespace routing {

namespace protobuf { class Message; }

class GroupChangeHandler {
 public:
  GroupChangeHandler(RoutingTable& routing_table, NetworkUtils& network);
  ~GroupChangeHandler();
  void SendClosestNodesUpdateRpcs(const std::vector<NodeInfo>& new_close_nodes);
  void UpdateGroupChange(const NodeId& node_id, std::vector<NodeInfo> close_nodes);
  void UpdatePendingGroupChange(const NodeId& node_id);
  void ClosestNodesUpdate(protobuf::Message& message);
  void SendSubscribeRpc(const bool& subscribe, const NodeInfo node_info);
  void ClosestNodesUpdateSubscribe(protobuf::Message& message);
 private:
  struct PendingNotification {
    PendingNotification(const NodeId& node_id_in, std::vector<NodeInfo> close_nodes_in);
    NodeId node_id;
    std::vector<NodeInfo> close_nodes;
  };

  GroupChangeHandler(const GroupChangeHandler&);
  GroupChangeHandler& operator=(const GroupChangeHandler&);

  void AddPendingNotification(const NodeId& node_id, std::vector<NodeInfo> close_nodes);
  std::vector<NodeInfo> GetAndRemovePendingNotification(const NodeId& node_from);
  void Subscribe(NodeId node_id);
  void Unsubscribe(NodeId node_id);

  std::mutex mutex_;
  RoutingTable& routing_table_;
  NetworkUtils& network_;
  std::vector<PendingNotification> pending_notifications_;
  std::vector<NodeInfo> update_subscribers_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_GROUP_CHANGE_HANDLER_H_
