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

#include "maidsafe/routing/non_routing_table.h"

#include "maidsafe/common/log.h"

#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/parameters.h"


namespace maidsafe {

namespace routing {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // unnamed namespace

NonRoutingTable::NonRoutingTable(const NodeId& node_id)
    : kNodeId_(node_id),
      nodes_(),
      mutex_() {}

void NonRoutingTable::InitialiseFunctors(UnsubscribeGroupUpdate unsubscribe_group_update) {
  unsubscribe_group_update_ = unsubscribe_group_update;
}

bool NonRoutingTable::AddNode(NodeInfo& node, const NodeId& furthest_close_node_id) {
  return AddOrCheckNode(node, furthest_close_node_id, true);
}

bool NonRoutingTable::CheckNode(NodeInfo& node, const NodeId& furthest_close_node_id) {
  return AddOrCheckNode(node, furthest_close_node_id, false);
}

bool NonRoutingTable::AddOrCheckNode(NodeInfo& node,
                                     const NodeId& furthest_close_node_id,
                                     const bool& add) {
  if (node.node_id == kNodeId_)
    return false;
  std::lock_guard<std::mutex> lock(mutex_);
  if (CheckRangeForNodeToBeAdded(node, furthest_close_node_id, add)) {
    if (add) {
      nodes_.push_back(node);
      LOG(kInfo) << "Added to non routing table :" << DebugId(node.node_id);
      LOG(kVerbose) << PrintNonRoutingTable();
    }
    return true;
  }
  return false;
}

std::vector<NodeInfo> NonRoutingTable::DropNodes(const NodeId &node_to_drop) {
  std::vector<NodeInfo> nodes_info;
  std::lock_guard<std::mutex> lock(mutex_);
  uint16_t i(0);
  while (i < nodes_.size()) {
    if (nodes_.at(i).node_id == node_to_drop) {
      nodes_info.push_back(nodes_.at(i));
      nodes_.erase(nodes_.begin() + i);
    } else {
      ++i;
    }
  }
  return nodes_info;
}

NodeInfo NonRoutingTable::DropConnection(const NodeId& connection_to_drop) {
  NodeInfo node_info;
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
    if ((*it).connection_id == connection_to_drop) {
      node_info = *it;
      nodes_.erase(it);
      if (unsubscribe_group_update_)
        unsubscribe_group_update_(connection_to_drop);
      break;
    }
  }
  return node_info;
}

std::vector<NodeInfo> NonRoutingTable::GetNodesInfo(const NodeId& node_id) const {
  std::vector<NodeInfo> nodes_info;
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
    if ((*it).node_id == node_id)
      nodes_info.push_back(*it);
  }
  return nodes_info;
}

bool NonRoutingTable::Contains(const NodeId& node_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return std::find_if(nodes_.begin(),
                      nodes_.end(),
                      [node_id](const NodeInfo& node_info) {
                        return node_info.node_id == node_id;
                      }) != nodes_.end();
}

bool NonRoutingTable::IsConnected(const NodeId& node_id) const {
  return Contains(node_id);
}

size_t NonRoutingTable::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return nodes_.size();
}

// TODO(Prakash): re-order checks to increase performance if needed
bool NonRoutingTable::CheckValidParameters(const NodeInfo& node) const {
  if (!asymm::ValidateKey(node.public_key)) {
    LOG(kInfo) << "Invalid public key.";
    return false;
  }
  // bucket index is not used in non routing table
  if (node.bucket != NodeInfo::kInvalidBucket) {
    LOG(kInfo) << "Invalid bucket index.";
    return false;
  }
  return CheckParametersAreUnique(node);
}

bool NonRoutingTable::CheckParametersAreUnique(const NodeInfo& node) const {
  // If we already have a duplicate endpoint return false
  if (std::find_if(nodes_.begin(),
                   nodes_.end(),
                   [node](const NodeInfo& node_info) {
                     return (node_info.connection_id == node.connection_id);
                   }) != nodes_.end()) {
    LOG(kInfo) << "Already have node with this connection_id.";
    return false;
  }

  // If we already have a duplicate public key under different node ID return false
  if (std::find_if(nodes_.begin(),
                   nodes_.end(),
                   [node](const NodeInfo& node_info) {
                     return (asymm::MatchingKeys(node_info.public_key, node.public_key) &&
                             (node_info.node_id != node.node_id));
                   }) != nodes_.end()) {
    LOG(kInfo) << "Already have a different node ID with this public key.";
    return false;
  }
  return true;
}

bool NonRoutingTable::CheckRangeForNodeToBeAdded(NodeInfo& node,
                                                 const NodeId& furthest_close_node_id,
                                                 const bool& add) const {
  if (nodes_.size() >= Parameters::max_non_routing_table_size) {
    LOG(kInfo) << "Non Routing Table full.";
    return false;
  }

  if (add && !CheckValidParameters(node)) {
    LOG(kInfo) << "Invalid Parameters.";
    return false;
  }

  return IsThisNodeInRange(node.node_id, furthest_close_node_id);
}

bool NonRoutingTable::IsThisNodeInRange(const NodeId& node_id,
                                        const NodeId& furthest_close_node_id) const {
  if (furthest_close_node_id == node_id) {
    assert(false && "node_id (client) and furthest_close_node_id (vault) should not be equal.");
    return false;
  }
  return (furthest_close_node_id ^ kNodeId_) > (node_id ^ kNodeId_);
}

std::string NonRoutingTable::PrintNonRoutingTable() {
  auto rt(nodes_);
  std::string s = "\n\n[" + DebugId(kNodeId_) +
      "] This node's own non routing table and peer connections:\n";
  for (auto node : rt) {
    s += std::string("\tPeer ") + "[" + DebugId(node.node_id) + "]"+ "-->";
    s += DebugId(node.connection_id)+ "\n";
  }
  s += "\n\n";
  return s;
}

}  // namespace routing

}  // namespace maidsafe
