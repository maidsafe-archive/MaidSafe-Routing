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

#include "maidsafe/routing/remove_furthest_node.h"

#include <string>
#include <vector>

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/routing.pb.h"

namespace maidsafe {

namespace routing {

RemoveFurthestNode::RemoveFurthestNode(RoutingTable& routing_table, NetworkUtils& network)
    : routing_table_(routing_table), network_(network) {}

void RemoveFurthestNode::RemoveRequest(protobuf::Message& message) {
  LOG(kVerbose) << "[" << HexSubstr(routing_table_.kNodeId().string())
                << "] Request to drop, id: " << message.id();
  protobuf::RemoveResponse remove_response;
  if (message.destination_id() != routing_table_.kNodeId().string()) {
    // Message not for this node and we should not pass it on.
    LOG(kError) << "Message not for this node.";
    message.Clear();
    return;
  }
  if (!IsRemovable(NodeId(message.source_id()))) {
    LOG(kVerbose) << "[" << HexSubstr(routing_table_.kNodeId().string())
                  << "] failed to qualify to drop, id: " << message.id();
    RejectRemoval(message);
  } else {
    LOG(kVerbose) << "[" << HexSubstr(routing_table_.kNodeId().string())
                  << "] attempt to drop, id: " << message.id();
    HandleRemoveRequest(NodeId(message.source_id()));
    message.Clear();
  }
}

void RemoveFurthestNode::HandleRemoveRequest(const NodeId& node_id) {
  LOG(kVerbose) << "[" << HexSubstr(routing_table_.kNodeId().string()) << "] drops "
                << HexSubstr(node_id.string());
  routing_table_.DropNode(node_id, false);
}

bool RemoveFurthestNode::IsRemovable(const NodeId& node_id) {
  return (routing_table_.size() > Parameters::closest_nodes_size) &&
      !routing_table_.IsThisNodeInRange(node_id, Parameters::closest_nodes_size);
}

void RemoveFurthestNode::RejectRemoval(protobuf::Message& message) {
  protobuf::RemoveResponse remove_response;
  remove_response.set_original_request(message.data(0));
  message.clear_data();
  message.clear_route_history();
  message.set_request(false);
  remove_response.set_success(false);
  remove_response.set_peer_id(routing_table_.kNodeId().string());
  message.set_hops_to_live(Parameters::hops_to_live);
  message.set_destination_id(message.source_id());
  message.set_source_id(routing_table_.kNodeId().string());
  assert(remove_response.IsInitialized() && "Remove Response is not initialised");
  message.add_data(remove_response.SerializeAsString());
  assert(message.IsInitialized() && "Message is not initialised");
}

void RemoveFurthestNode::RemoveResponse(protobuf::Message& message) {
  protobuf::RemoveResponse remove_response;
  protobuf::RemoveRequest remove_request;
  if (!remove_response.ParseFromString(message.data(0))) {
    LOG(kError) << "Could not parse remove node response";
    return;
  }
  if (!remove_response.success()) {
    LOG(kInfo) << "Request to remove " << HexSubstr(message.source_id())
               << " failed, another node will be tried";
    if (!remove_request.ParseFromString(remove_response.original_request())) {
      LOG(kError) << "Could not parse remove node request";
      return;
    }
    NodeInfo next_node;
    std::vector<std::string> attempted_nodes(remove_request.attempted_nodes().begin(),
                                             remove_request.attempted_nodes().end());
    next_node = routing_table_.GetRemovableNode(attempted_nodes);
    if (next_node.node_id != NodeInfo().node_id) {
      attempted_nodes.push_back(remove_response.peer_id());
      protobuf::Message remove_request(rpcs::Remove(next_node.node_id, routing_table_.kNodeId(),
                                                    routing_table_.kConnectionId(),
                                                    attempted_nodes));
      LOG(kInfo) << "Request to remove " << HexSubstr(remove_request.destination_id())
                 << " is re-prepared, message id:" << message.id();
      remove_request.set_id(message.id());
      network_.SendToDirect(remove_request, next_node.node_id, next_node.connection_id);
    } else {
      LOG(kInfo) << "Request to remove " << HexSubstr(message.source_id()) << " succeeded";
    }
  }
}

void RemoveFurthestNode::RemoveNodeRequest() {
  NodeInfo furthest_node(routing_table_.GetRemovableNode(std::vector<std::string>()));
  if (furthest_node.node_id == NodeInfo().node_id)
    return;
  protobuf::Message message(rpcs::Remove(furthest_node.node_id, routing_table_.kNodeId(),
                                         routing_table_.kConnectionId(),
                                         std::vector<std::string>()));
  LOG(kInfo) << "[" << DebugId(routing_table_.kNodeId()) << "] Request to remove "
             << HexSubstr(message.destination_id()) << " is prepared, message id: " << message.id();
  network_.SendToDirect(message, furthest_node.node_id, furthest_node.connection_id);
}

}  // namespace routing

}  // namespace maidsafe
