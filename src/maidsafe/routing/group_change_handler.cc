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
  : mutex_(),
    routing_table_(routing_table),
    network_(network),
    pending_notifications_(),
    update_subscribers_() {}

GroupChangeHandler::~GroupChangeHandler() {
  LOG(kVerbose) << "GroupChangeHandler::~GroupChangeHandler() "
                << DebugId(routing_table_.kNodeId());
  std::lock_guard<std::mutex> lock(mutex_);
}

GroupChangeHandler::PendingNotification::PendingNotification(const NodeId& node_id_in,
                                                             std::vector<NodeInfo> close_nodes_in)
    : node_id(node_id_in),
      close_nodes(close_nodes_in) {}

void GroupChangeHandler::ClosestNodesUpdate(protobuf::Message& message) {
  if (message.destination_id() != routing_table_.kFob().identity.string()) {
    // Message not for this node and we should not pass it on.
    LOG(kError) << "Message not for this node.";
    message.Clear();
    return;
  }
  protobuf::ClosestNodesUpdate closest_node_update;
  if (!closest_node_update.ParseFromString(message.data(0))) {
    LOG(kError) << "No Data.";
    return;
  }

  if (closest_node_update.node().empty() || !CheckId(closest_node_update.node())) {
    LOG(kError) << "Invalid node id provided.";
    return;
  }

  std::vector<NodeInfo> closest_nodes;
  NodeInfo node_info;
  for (const protobuf::BasicNodeInfo& basic_info : closest_node_update.nodes_info()) {
    if (CheckId(basic_info.node_id())) {
      node_info.node_id = NodeId(basic_info.node_id());
      node_info.rank = basic_info.rank();
      closest_nodes.push_back(node_info);
    }
  }
  if (!closest_nodes.empty())
    UpdateGroupChange(NodeId(closest_node_update.node()), closest_nodes);
  message.Clear();  // No response
}

void GroupChangeHandler::ClosestNodesUpdateSubscribe(protobuf::Message& message) {
  if (message.destination_id() != routing_table_.kFob().identity.string()) {
    // Message not for this node and we should not pass it on.
    LOG(kError) << "Message not for this node.";
    message.Clear();
    return;
  }
  protobuf::ClosestNodesUpdateSubscrirbe closest_node_update_subscribe;
  if (!closest_node_update_subscribe.ParseFromString(message.data(0))) {
    LOG(kError) << "No Data.";
    return;
  }

  if (closest_node_update_subscribe.peer().empty() ||
      !CheckId(closest_node_update_subscribe.peer())) {
    LOG(kError) << "Invalid node id provided.";
    return;
  }

  if (closest_node_update_subscribe.subscribe())
    Subscribe(NodeId(closest_node_update_subscribe.peer()));
  else
    Unsubscribe(NodeId(closest_node_update_subscribe.peer()));
  message.Clear();  // No response
}

void GroupChangeHandler::Unsubscribe(NodeId node_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  update_subscribers_.erase(std::remove_if(update_subscribers_.begin(),
                                           update_subscribers_.end(),
                                           [&](const NodeInfo& node_info) {
                                             return node_info.node_id == node_id;
                                           }));
}

void GroupChangeHandler::Subscribe(NodeId node_id) {
  NodeInfo node_info;
  std::vector<NodeInfo> connected_closest_nodes;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    connected_closest_nodes = routing_table_.GetClosestNodeInfo(routing_table_.kNodeId(),
                                                                Parameters::closest_nodes_size);
    for (auto node : update_subscribers_) {
      LOG(kVerbose) << DebugId(node.node_id) << ", " << DebugId(routing_table_.kNodeId());
    }
    size_t size(update_subscribers_.size());
    LOG(kVerbose) << "write size: " << size << ", " << DebugId(node_id);
    if (routing_table_.GetNodeInfo(node_id, node_info)) {
      LOG(kVerbose) << "Subscribe 1: " << DebugId(node_info.node_id);
      if (std::find_if(update_subscribers_.begin(),
                       update_subscribers_.end(),
                       [=](const NodeInfo& node)->bool {
                         LOG(kVerbose) << "Subscribe 2: " <<DebugId(node.node_id);
                         return node.node_id == node_id;
                       }) == update_subscribers_.end())
        update_subscribers_.push_back(node_info);
    }
  }
  if (node_info.node_id != NodeId()) {
    assert (connected_closest_nodes.size() <= Parameters::closest_nodes_size);
    protobuf::Message closest_nodes_update_rpc(
        rpcs::ClosestNodesUpdateRequest(node_info.node_id,
                                        routing_table_.kNodeId(),
                                        connected_closest_nodes));
    network_.SendToDirect(closest_nodes_update_rpc, node_info.node_id, node_info.connection_id);
  }
}


void GroupChangeHandler::UpdateGroupChange(const NodeId& node_id,
                                           std::vector<NodeInfo> close_nodes) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (routing_table_.IsConnected(node_id)) {
    routing_table_.GroupUpdateFromConnectedPeer(node_id, close_nodes);
  }/* else {
    AddPendingNotification(node_id, close_nodes);
  }*/
  SendSubscribeRpc(true, NodeInfo());
}

//void GroupChangeHandler::UpdatePendingGroupChange(const NodeId& node_id) {
//  std::lock_guard<std::mutex> lock(mutex_);
//  assert(routing_table_.IsConnected(node_id));
//  std::vector<NodeInfo> close_nodes(GetAndRemovePendingNotification(node_id));
//  if (!close_nodes.empty()) {
//    routing_table_.GroupUpdateFromConnectedPeer(node_id, close_nodes);
//  }
//}

void GroupChangeHandler::SendClosestNodesUpdateRpcs(const std::vector<NodeInfo>& closest_nodes) {
  if (closest_nodes.size() < Parameters::closest_nodes_size)
    return;
  assert (closest_nodes.size() <= Parameters::closest_nodes_size);
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto itr(update_subscribers_.begin()); itr != update_subscribers_.end(); ++itr) {
    protobuf::Message closest_nodes_update_rpc(
        rpcs::ClosestNodesUpdateRequest(itr->node_id, routing_table_.kNodeId(), closest_nodes));
    network_.SendToDirect(closest_nodes_update_rpc, itr->node_id, itr->connection_id);
  }
}

//void GroupChangeHandler::AddPendingNotification(const NodeId& node_id,
//                                                std::vector<NodeInfo> close_nodes) {
//  auto itr(std::find_if(pending_notifications_.begin(),
//                        pending_notifications_.end(),
//                        [node_id](const PendingNotification& pending_notification) {
//                          return (node_id == pending_notification.node_id);
//                        }));
//  if (itr != pending_notifications_.end()) {
//    itr->close_nodes.swap(close_nodes);
//  } else {
//    pending_notifications_.push_back(PendingNotification(node_id, close_nodes));
//  }
//}

//std::vector<NodeInfo> GroupChangeHandler::GetAndRemovePendingNotification(const NodeId& node_id) {
//  auto itr(std::find_if(pending_notifications_.begin(),
//                        pending_notifications_.end(),
//                        [node_id](const PendingNotification& pending_notification) {
//                          return (node_id == pending_notification.node_id);
//                        }));
//  if (itr != pending_notifications_.end()) {
//    std::vector<NodeInfo> close_nodes(itr->close_nodes);
//    pending_notifications_.erase(itr);
//    return close_nodes;
//  } else {
//    return std::vector<NodeInfo>();
//  }
//}

void GroupChangeHandler::SendSubscribeRpc(const bool& subscribe,
                                          const NodeInfo node_info) {
  std::vector<NodeInfo> nodes_needing_update;
  if (subscribe) {
    routing_table_.GetNodesNeedingGroupUpdates(nodes_needing_update);
  } else {
    nodes_needing_update.push_back(node_info);
  }
  for (auto& node : nodes_needing_update) {
    protobuf::Message closest_nodes_update_rpc(
        rpcs::ClosestNodesUpdateSubscrirbe(node.node_id, routing_table_.kNodeId(), subscribe));
    network_.SendToDirect(closest_nodes_update_rpc, node.node_id, node.connection_id);
  }
}

}  // namespace routing

}  // namespace maidsafe
