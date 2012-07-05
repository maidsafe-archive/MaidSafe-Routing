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

#include "maidsafe/routing/log.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/parameters.h"

namespace bs2 = boost::signals2;

namespace maidsafe {

namespace routing {

NonRoutingTable::NonRoutingTable(const asymm::Keys &keys)
    : keys_(keys),
      kNodeId_(NodeId(keys_.identity)),
      non_routing_table_nodes_(),
      mutex_(),
      close_node_from_to_signal_() {}

void NonRoutingTable::set_keys(asymm::Keys keys) {
  keys_ = keys;
}

bool NonRoutingTable::AddNode(NodeInfo &node, const NodeId &furthest_close_node_id) {
  return AddOrCheckNode(node, furthest_close_node_id, true);
}

bool NonRoutingTable::CheckNode(NodeInfo &node, const NodeId &furthest_close_node_id) {
  return AddOrCheckNode(node, furthest_close_node_id, false);
}

bool NonRoutingTable::AddOrCheckNode(NodeInfo &node, const NodeId &furthest_close_node_id,
                                     const bool &add) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (node.node_id == kNodeId_) {
    return false;
  }
  if (CheckRangeForNodeToBeAdded(node, furthest_close_node_id, add)) {
    if (add)
      non_routing_table_nodes_.push_back(node);
    return true;
  }
  return false;
}

uint16_t NonRoutingTable::Size() {
  std::lock_guard<std::mutex> lock(mutex_);
  return NonRoutingTableSize();
}

uint16_t NonRoutingTable::NonRoutingTableSize() {
  return static_cast<uint16_t>(non_routing_table_nodes_.size());
}

asymm::Keys NonRoutingTable::kKeys() const {
  return keys_;
}

bool NonRoutingTable::DropNode(const Endpoint &endpoint) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = non_routing_table_nodes_.begin(); it != non_routing_table_nodes_.end(); ++it) {
    if (((*it).endpoint ==  endpoint)) {
      non_routing_table_nodes_.erase(it);
      return true;
    }
  }
  return false;
}

// Note: std::remove algorithm does not eliminate elements from the container.
// Erase-Remove idiom is used below to really eliminate data elements from a container.
// http://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Erase-Remove

int16_t NonRoutingTable::DropNodes(const NodeId &node_id) {
  int16_t count(0);
  std::lock_guard<std::mutex> lock(mutex_);
  non_routing_table_nodes_.erase(
      std::remove_if(non_routing_table_nodes_.begin(), non_routing_table_nodes_.end(),
                     [node_id, &count](const NodeInfo i)->bool {
                       if (i.node_id == node_id) {
                         ++count;
                         return true;
                       }
                       return false;
                     }), non_routing_table_nodes_.end());
  return count;
}

NodeInfo NonRoutingTable::GetNodeInfo(const Endpoint &endpoint) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = non_routing_table_nodes_.begin(); it != non_routing_table_nodes_.end(); ++it) {
    if (((*it).endpoint ==  endpoint))
      return (*it);
  }
  return NodeInfo();
}

std::vector<NodeInfo> NonRoutingTable::GetNodesInfo(const NodeId &node_id) {
  std::vector<NodeInfo> nodes_info;
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = non_routing_table_nodes_.begin(); it != non_routing_table_nodes_.end(); ++it) {
    if (((*it).node_id ==  node_id))
      nodes_info.push_back(*it);
  }
  return nodes_info;
}

bool NonRoutingTable::AmIConnectedToEndpoint(const Endpoint& endpoint) {
  std::lock_guard<std::mutex> lock(mutex_);
  return (std::find_if(non_routing_table_nodes_.begin(), non_routing_table_nodes_.end(),
                       [endpoint](const NodeInfo &i)->bool { return i.endpoint == endpoint; })
                     != non_routing_table_nodes_.end());
}
// TODO(Prakash): re-order checks to increase performance if needed
// checks paramters are real
bool NonRoutingTable::CheckValidParameters(const NodeInfo& node) const {
  if ((!asymm::ValidateKey(node.public_key, 0))) {
    LOG(kInfo) << "invalid public key";
    return false;
  }
  // bucket index is not used in non routing table
  if (node.bucket != 99999) {
    LOG(kInfo) << "invalid bucket index";
    return false;
  }
  return CheckParametersAreUnique(node);
}

bool NonRoutingTable::CheckParametersAreUnique(const NodeInfo& node) const {
  // if we already have a duplicate endpoint return false
  if (std::find_if(non_routing_table_nodes_.begin(), non_routing_table_nodes_.end(),
                   [node](const NodeInfo &i)->bool
                   { return (i.endpoint == node.endpoint); })
                 != non_routing_table_nodes_.end()) {
    LOG(kInfo) << "Already have node with this endpoint";
    return false;
  }

  // if we already have a duplicate public key under different node id return false
  if (std::find_if(non_routing_table_nodes_.begin(),
                   non_routing_table_nodes_.end(),
                   [node](const NodeInfo &i)->bool {
                       return (asymm::MatchingPublicKeys(i.public_key, node.public_key) &&
                           (i.node_id != node.node_id));})
                 != non_routing_table_nodes_.end()) {
    LOG(kInfo) << "Already have a different node id with this public key";
    return false;
  }
  return true;
}

bool NonRoutingTable::CheckRangeForNodeToBeAdded(NodeInfo& node,
                                                 const NodeId &furthest_close_node_id,
                                                 const bool &add) {
  if (NonRoutingTableSize() > Parameters::max_non_routing_table_size) {
    LOG(kInfo) << "Non Routing Table full";
    return false;
  }

  if (add && !CheckValidParameters(node)) {
    LOG(kInfo) << "Invalid Parameters";
    return false;
  }
  return IsMyNodeInRange(kNodeId_, furthest_close_node_id);
}

bool NonRoutingTable::IsMyNodeInRange(const NodeId& node_id,
                                      const NodeId &furthest_close_node_id)  {
  return (furthest_close_node_id ^ kNodeId_) > (node_id ^ kNodeId_);
}

}  // namespace routing

}  // namespace maidsafe
