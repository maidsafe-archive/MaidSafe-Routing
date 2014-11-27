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

#include "maidsafe/routing/client_routing_table.h"

#include "maidsafe/common/log.h"

#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/parameters.h"

namespace maidsafe {

namespace routing {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // unnamed namespace

ClientRoutingTable::ClientRoutingTable(NodeId node_id)
    : kNodeId_(std::move(node_id)), nodes_(), mutex_() {}

bool ClientRoutingTable::AddNode(NodeInfo& node, const NodeId& furthest_close_node_id) {
  return AddOrCheckNode(node, furthest_close_node_id, true);
}

bool ClientRoutingTable::CheckNode(NodeInfo& node, const NodeId& furthest_close_node_id) {
  return AddOrCheckNode(node, furthest_close_node_id, false);
}

bool ClientRoutingTable::AddOrCheckNode(NodeInfo& node, const NodeId& furthest_close_node_id,
                                        bool add) {
  if (node.id == kNodeId_)
    return false;
  std::lock_guard<std::mutex> lock(mutex_);
  if (CheckRangeForNodeToBeAdded(node, furthest_close_node_id, add)) {
    if (add) {
      nodes_.push_back(node);
    }
    return true;
  }
  return false;
}

std::vector<NodeInfo> ClientRoutingTable::DropNodes(const NodeId& node_to_drop) {
  std::vector<NodeInfo> nodes_info;
  std::lock_guard<std::mutex> lock(mutex_);
  unsigned int i(0);
  while (i < nodes_.size()) {
    if (nodes_.at(i).id == node_to_drop) {
      nodes_info.push_back(nodes_.at(i));
      nodes_.erase(nodes_.begin() + i);
    } else {
      ++i;
    }
  }
  return nodes_info;
}

NodeInfo ClientRoutingTable::DropConnection(const NodeId& connection_to_drop) {
  NodeInfo node_info;
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
    if ((*it).connection_id == connection_to_drop) {
      node_info = *it;
      nodes_.erase(it);
      break;
    }
  }
  return node_info;
}

std::vector<NodeInfo> ClientRoutingTable::GetNodesInfo(const NodeId& node_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (node_id == NodeId())
    return nodes_;

  std::vector<NodeInfo> nodes_info;
  std::copy_if(std::begin(nodes_), std::end(nodes_), std::back_inserter(nodes_info),
               [node_id](const NodeInfo& node) { return node.id == node_id; });
  return nodes_info;
}

bool ClientRoutingTable::Contains(const NodeId& node_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return std::find_if(std::begin(nodes_), std::end(nodes_),
                      [node_id](const NodeInfo& node_info) {
                        return node_info.id == node_id;
                      }) != std::end(nodes_);
}

bool ClientRoutingTable::IsConnected(const NodeId& node_id) const { return Contains(node_id); }

size_t ClientRoutingTable::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return nodes_.size();
}

// TODO(Prakash): re-order checks to increase performance if needed
bool ClientRoutingTable::CheckValidParameters(const NodeInfo& node) const {
  // bucket index is not used in ClientRoutingTable
  if (node.bucket != NodeInfo::kInvalidBucket) {
    return false;
  }
  return CheckParametersAreUnique(node);
}

bool ClientRoutingTable::CheckParametersAreUnique(const NodeInfo& node) const {
  // If we already have a duplicate endpoint return false
  if (std::find_if(nodes_.begin(), nodes_.end(), [node](const NodeInfo & node_info) {
        return (node_info.connection_id == node.connection_id);
      }) != nodes_.end()) {
    return false;
  }

  // If we already have a duplicate public key under different node ID return false
  //  if (std::find_if(nodes_.begin(),
  //                   nodes_.end(),
  //                   [node](const NodeInfo& node_info) {
  //                     return (asymm::MatchingKeys(node_info.public_key, node.public_key) &&
  //                             (node_info.id != node.id));
  //                   }) != nodes_.end()) {
  //    return false;
  //  }
  return true;
}

bool ClientRoutingTable::CheckRangeForNodeToBeAdded(NodeInfo& node,
                                                    const NodeId& furthest_close_node_id,
                                                    bool add) const {
  if (nodes_.size() >= Parameters::max_client_routing_table_size) {
    return false;
  }

  if (add && !CheckValidParameters(node)) {
    return false;
  }

  return IsThisNodeInRange(node.id, furthest_close_node_id);
}

bool ClientRoutingTable::IsThisNodeInRange(const NodeId& node_id,
                                           const NodeId& furthest_close_node_id) const {
  if (furthest_close_node_id == node_id) {
    assert(false && "node_id (client) and furthest_close_node_id (vault) should not be equal.");
    return false;
  }
  return NodeId::CloserToTarget(node_id, furthest_close_node_id, kNodeId_);
}

std::string ClientRoutingTable::PrintClientRoutingTable() {
  auto rt(nodes_);
  std::string s =
      "\n\n[" + DebugId(kNodeId_) + "] This node's own ClientRoutingTable and peer connections:\n";
  for (const auto& node : rt) {
    s += std::string("\tPeer ") + "[" + DebugId(node.id) + "]" + "-->";
    s += DebugId(node.connection_id) + "\n";
  }
  s += "\n\n";
  return s;
}

}  // namespace routing

}  // namespace maidsafe
