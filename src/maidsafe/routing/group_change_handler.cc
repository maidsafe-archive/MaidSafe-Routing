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

#include "maidsafe/routing/group_change_handler.h"

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/utils.h"


namespace maidsafe {

namespace routing {

GroupChangeHandler::GroupChangeHandler(RoutingTable& routing_table, NetworkUtils& network)
  : routing_table_(routing_table),
    network_(network),
    mutex_(),
    pending_notifications_() {}

GroupChangeHandler::PendingNotification::PendingNotification(const NodeId& node_id_in,
                                                             std::vector<NodeId> close_nodes_in)
    : node_id(node_id_in),
      close_nodes(close_nodes_in) {}

void GroupChangeHandler::UpdateGroupChange(const NodeId& node_id, std::vector<NodeId> close_nodes) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (routing_table_.IsConnected(node_id)) {
//M    routing_table_.GroupUpdateFromConnectedPeer(node_id, close_nodes);
  } else {
    AddPendingNotification(node_id, close_nodes);
  }
}

void GroupChangeHandler::UpdatePendingGroupChange(const NodeId& node_id) {
  std::lock_guard<std::mutex> lock(mutex_);
//  assert(routing_table_.IsConnected(node_id));
  std::vector<NodeId> close_nodes(GetAndRemovePendingNotification(node_id));
  if (!close_nodes.empty()) {
//M    routing_table_.GroupUpdateFromConnectedPeer(node_id, close_nodes);
  }
}

void GroupChangeHandler::SendCloseNodeChangeRpcs(std::vector<NodeInfo> new_close_nodes) {
  for (auto itr(new_close_nodes.begin()); itr != new_close_nodes.end(); ++itr) {
    std::vector<NodeId> close_nodes;
    for (auto i : new_close_nodes) {
      if (i.node_id != (*itr).node_id)
        close_nodes.push_back(i.node_id);
      protobuf::Message close_node_change_rpc(
          rpcs::CloseNodeChange(i.node_id, routing_table_.kNodeId(), close_nodes));
      network_.SendToDirect(close_node_change_rpc, i.node_id, i.connection_id);
    }
  }
}

void GroupChangeHandler::AddPendingNotification(const NodeId& node_id,
                                                std::vector<NodeId> close_nodes) {
  auto itr(std::find_if(pending_notifications_.begin(),
                        pending_notifications_.end(),
                        [node_id](const PendingNotification& pending_notification) {
                          return (node_id == pending_notification.node_id);
                        }));
  if (itr != pending_notifications_.end()) {
    itr->close_nodes.swap(close_nodes);
  } else {
    pending_notifications_.push_back(PendingNotification(node_id, close_nodes));
  }
}

std::vector<NodeId> GroupChangeHandler::GetAndRemovePendingNotification(const NodeId& node_id) {
  auto itr(std::find_if(pending_notifications_.begin(),
                        pending_notifications_.end(),
                        [node_id](const PendingNotification& pending_notification) {
                          return (node_id == pending_notification.node_id);
                        }));
  if (itr != pending_notifications_.end()) {
    std::vector<NodeId> close_nodes(itr->close_nodes);
    pending_notifications_.erase(itr);
    return close_nodes;
  } else {
    return std::vector<NodeId>();
  }
}

}  // namespace routing

}  // namespace maidsafe
