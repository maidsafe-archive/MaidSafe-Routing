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

#include "maidsafe/routing/remove_furthest_node.h"

#include <string>

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/routing_pb.h"

namespace maidsafe {

namespace routing {

RemoveFurthestNode::RemoveFurthestNode(RoutingTable& routing_table, NetworkUtils& network)
  : routing_table_(routing_table),
    network_(network) {}

void RemoveFurthestNode::RemoveRequest(protobuf::Message& message) {
  protobuf::RemoveResponse remove_response;
  if (message.destination_id() != routing_table_.kFob().identity.string()) {
    // Message not for this node and we should not pass it on.
    LOG(kError) << "Message not for this node.";
    message.Clear();
    return;
  }
  if (!IsRemovable(NodeId(message.source_id()))) {
    RejectRemoval(message);
  } else {
    HandleRemoveRequest(NodeId(message.source_id()));
  }
}

void RemoveFurthestNode::HandleRemoveRequest(const NodeId& node_id) {
  routing_table_.DropNode(node_id, false);
}

bool RemoveFurthestNode::IsRemovable(const NodeId& node_id) {
  return !routing_table_.IsThisNodeInRange(node_id, Parameters::closest_nodes_size);
}

void RemoveFurthestNode::RejectRemoval(protobuf::Message& message) {
  protobuf::RemoveResponse remove_response;
  message.clear_data();
  message.set_request(false);
  remove_response.set_success(false);
  remove_response.set_peer_id(routing_table_.kFob().identity.string());
  message.add_data(remove_response.SerializeAsString());
}

void RemoveFurthestNode::RemoveResponse(protobuf::Message& message) {
  protobuf::RemoveResponse remove_response;
  if (remove_response.ParseFromString(message.data(0))) {
    LOG(kError) << "Could not parse remove node response";
    return;
  }
  if (!remove_response.success()) {
    LOG(kInfo) << "Request to remove " << HexSubstr(message.source_id())
               << " failed, another node will be tried";
    NodeInfo next_node;
    next_node = routing_table_.GetClosestTo(NodeId(remove_response.peer_id()), true);
    if (next_node.node_id == NodeInfo().node_id) {
      next_node = routing_table_.GetFurthestNode();
      if (next_node.node_id != NodeInfo().node_id) {
        protobuf::Message remove_request(rpcs::Remove(next_node.node_id,
                                                      routing_table_.kNodeId(),
                                                      routing_table_.kConnectionId()));
        LOG(kInfo) << "Request to remove " << HexSubstr(remove_request.destination_id())
                   << " is prepared";
        network_.SendToDirect(remove_request, next_node.node_id, next_node.connection_id);
      }
    }
  } else {
    LOG(kInfo) << "Request to remove " << HexSubstr(message.source_id()) << " succeeded";
  }
}

void RemoveFurthestNode::RemoveNodeRequest() {
  NodeInfo furthest_node(routing_table_.GetFurthestNode());
  protobuf::Message message(rpcs::Remove(furthest_node.node_id,
                                         routing_table_.kNodeId(),
                                         routing_table_.kConnectionId()));
  LOG(kInfo) << "Request to remove " << HexSubstr(message.destination_id())
             << " is prepared";
  network_.SendToDirect(message, furthest_node.node_id, furthest_node.connection_id);
  LOG(kVerbose) << "RemoveFurthestNode::RemoveNodeRequest()";
}

}  // namespace routing

}  // namespace maidsafe
