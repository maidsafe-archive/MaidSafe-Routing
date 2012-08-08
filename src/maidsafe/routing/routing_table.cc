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

#include "maidsafe/routing/routing_table.h"

#include <thread>
#include <algorithm>
#include <string>

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/bootstrap_file_handler.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/node_info.h"


namespace maidsafe {

namespace routing {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // unnamed namespace

RoutingTable::RoutingTable(const asymm::Keys& keys, const bool& client_mode)
    : max_size_(client_mode ? Parameters::max_client_routing_table_size :
                              Parameters::max_routing_table_size),
      client_mode_(client_mode),
      keys_(keys),
      sorted_(false),
      kNodeId_(NodeId(keys_.identity)),
      furthest_group_node_id_(),
      mutex_(),
      network_status_functor_(),
      close_node_replaced_functor_(),
      bootstrap_file_path_(),
      nodes_() {}

bool RoutingTable::AddNode(NodeInfo& peer) {
  return AddOrCheckNode(peer, true);
}

bool RoutingTable::CheckNode(NodeInfo& peer) {
  return AddOrCheckNode(peer, false);
}

bool RoutingTable::AddOrCheckNode(NodeInfo& peer, const bool& remove) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (peer.node_id == kNodeId_) {
    LOG(kError) << "Tried to add this node";
    return false;
  }

  // If we already have node, return false.
  if (std::find_if(nodes_.begin(),
                   nodes_.end(),
                   [peer](const NodeInfo& node_info) { return node_info.node_id == peer.node_id; })
          != nodes_.end()) {
    LOG(kInfo) << "Node " << HexSubstr(peer.node_id.String()) << " already in routing table.";
    return false;
  }

  if (MakeSpaceForNodeToBeAdded(peer, remove)) {
    if (remove) {
      nodes_.push_back(peer);
      update_network_status();
      UpdateGroupChangeAndNotify();
      UpdateBootstrapFile(bootstrap_file_path_, peer.endpoint, false);
    }
    return true;
  }
  return false;
}

NodeInfo RoutingTable::DropNode(const Endpoint& endpoint) {
  NodeInfo dropped_node;
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
    if ((*it).endpoint == endpoint) {
      dropped_node = (*it);
      nodes_.erase(it);
      update_network_status();
      UpdateGroupChangeAndNotify();
      break;
    }
  }
  return dropped_node;
}

bool RoutingTable::GetNodeInfo(const Endpoint& endpoint, NodeInfo& peer) const {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
    if ((*it).endpoint == endpoint) {
      peer = *it;
      return true;
    }
  }
  return false;
}

bool RoutingTable::IsThisNodeInRange(const NodeId& target_id, const uint16_t range) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (nodes_.size() < range)
    return true;

  PartialSortFromTarget(kNodeId_, range);

  return (nodes_[range - 1].node_id ^ kNodeId_) > (target_id ^ kNodeId_);
}

bool RoutingTable::IsThisNodeClosestTo(const NodeId& target_id) {
  if (!target_id.IsValid()) {
    LOG(kError) << "Invalid target_id passed.";
    return false;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (nodes_.empty())
    return true;
  NthElementSortFromTarget(target_id, 1);
  return (kNodeId_ ^ target_id) < (target_id ^ nodes_[0].node_id);
}

bool RoutingTable::IsConnected(const Endpoint& endpoint) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return (std::find_if(nodes_.begin(),
                       nodes_.end(),
                       [endpoint](const NodeInfo& node_info) {
                         return node_info.endpoint == endpoint;
                       }) != nodes_.end());
}

bool RoutingTable::IsConnected(const NodeId& node_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return std::find_if(nodes_.begin(),
                      nodes_.end(),
                      [node_id](const NodeInfo& node_info) {
                        return node_info.node_id == node_id;
                      }) != nodes_.end();
}

bool RoutingTable::ConfirmGroupMembers(const NodeId& node1, const NodeId& node2) {
  NodeId difference = NodeId(kKeys().identity) ^ FurthestCloseNode();
  return (node1 ^ node2) < difference;
}

void RoutingTable::UpdateGroupChangeAndNotify() {
  if (close_node_replaced_functor_) {
    if (nodes_.size() >= Parameters::node_group_size) {
      NthElementSortFromTarget(kNodeId_, Parameters::node_group_size - 1);
      NodeId new_furthest_group_node_id = nodes_[Parameters::node_group_size - 2].node_id;
      if (furthest_group_node_id_ != new_furthest_group_node_id) {
        std::vector<NodeInfo> new_close_nodes(GetClosestNodeInfo(kNodeId_,
            Parameters::node_group_size));
        furthest_group_node_id_ = new_close_nodes[Parameters::node_group_size - 2].node_id;
        close_node_replaced_functor_(new_close_nodes);
      }
    } else {
       std::vector<NodeInfo> new_close_nodes(GetClosestNodeInfo(kNodeId_,
                                                                Parameters::node_group_size - 1));
       furthest_group_node_id_ = new_close_nodes[nodes_.size() - 1].node_id;
       close_node_replaced_functor_(new_close_nodes);
    }
  }
}

// bucket 0 is us, 511 is furthest bucket (should fill first)
int16_t RoutingTable::BucketIndex(const NodeId& rhs) const {
  uint16_t bucket = kKeySizeBits - 1;
  std::string this_id_binary = kNodeId_.ToStringEncoded(NodeId::kBinary);
  std::string rhs_id_binary = rhs.ToStringEncoded(NodeId::kBinary);
  auto this_it = this_id_binary.begin();
  auto rhs_it = rhs_id_binary.begin();

  for (; this_it != this_id_binary.end(); ++this_it, ++rhs_it) {
    if (*this_it != *rhs_it)
      return bucket;
    --bucket;
  }
  return bucket;
}

bool RoutingTable::CheckValidParameters(const NodeInfo& node) const {
  if (!asymm::ValidateKey(node.public_key, 0)) {
    LOG(kInfo) << "Invalid public key";
    return false;
  }
  if (node.bucket == NodeInfo::kInvalidBucket) {
    LOG(kInfo) << "Invalid bucket index";
    return false;
  }
  return CheckParametersAreUnique(node);
}

bool RoutingTable::CheckParametersAreUnique(const NodeInfo& node) const {
  // If we already have a duplicate public key return false
  if (std::find_if(nodes_.begin(),
                   nodes_.end(),
                   [node](const NodeInfo& node_info) {
                     return asymm::MatchingPublicKeys(node_info.public_key, node.public_key);
                   }) != nodes_.end()) {
    LOG(kInfo) << "Already have node with this public key";
    return false;
  }

  // If we already have a duplicate endpoint return false
  if (std::find_if(nodes_.begin(),
                   nodes_.end(),
                   [node](const NodeInfo& node_info) {
                     return (node_info.endpoint == node.endpoint);
                   }) != nodes_.end()) {
    LOG(kInfo) << "Already have node with this endpoint";
    return false;
  }

  return true;
}

bool RoutingTable::MakeSpaceForNodeToBeAdded(NodeInfo& node, const bool& remove) {
  node.bucket = BucketIndex(node.node_id);
  if (remove && !CheckValidParameters(node)) {
    LOG(kInfo) << "Invalid Parameters";
    return false;
  }

  if (nodes_.size() < max_size_)
    return true;

  PartialSortFromTarget(kNodeId_, Parameters::closest_nodes_size);
  NodeInfo furthest_close_node = nodes_[Parameters::closest_nodes_size - 1];
  auto const not_found = nodes_.end();
  auto const furthest_close_node_iter = nodes_.begin() + (Parameters::closest_nodes_size - 1);

  if ((furthest_close_node.node_id ^ kNodeId_) > (kNodeId_ ^ node.node_id)) {
    BOOST_ASSERT_MSG(node.bucket <= furthest_close_node.bucket,
                     "close node replacement to a larger bucket");

    if (remove)
      nodes_.erase(furthest_close_node_iter);
    return true;
  }

  uint16_t size(Parameters::bucket_target_size + 1);
  for (auto it = furthest_close_node_iter; it != not_found; ++it) {
    if (node.bucket >= (*it).bucket)  // Stop searching as it's worthless
      return false;
    // Safety net
    if ((not_found - it) < size)  // Reached end of checkable area
      return false;

    if ((*it).bucket == (*(it + size)).bucket) {
      // Here we know the node should fit into a bucket if the bucket has too many nodes AND node to
      // add has a lower bucket index
      BOOST_ASSERT(node.bucket < (*it).bucket);
      if (remove)
        nodes_.erase(it);
      return true;
    }
  }
  return false;
}

void RoutingTable::PartialSortFromTarget(const NodeId& target, const uint16_t& number) {
  uint16_t count = std::min(number, static_cast<uint16_t>(nodes_.size()));
  std::partial_sort(nodes_.begin(),
                    nodes_.begin() + count,
                    nodes_.end(),
                    [target](const NodeInfo& lhs, const NodeInfo& rhs) {
                      return (lhs.node_id ^ target) < (rhs.node_id ^ target);
                    });
}

void RoutingTable::NthElementSortFromTarget(const NodeId& target, const uint16_t& nth_element) {
  assert((nodes_.size() >= nth_element) &&
         "This should only be called when n is at max the size of RT");
  std::nth_element(nodes_.begin(),
                   nodes_.begin() + nth_element,
                   nodes_.end(),
                   [target](const NodeInfo& lhs, const NodeInfo& rhs) {
                     return (lhs.node_id ^ target) < (rhs.node_id ^ target);
                   });
}

NodeId RoutingTable::FurthestCloseNode() {
  return GetNthClosestNode(NodeId(kKeys().identity), Parameters::closest_nodes_size).node_id;
}

NodeInfo RoutingTable::GetClosestNode(const NodeId& target_id, bool ignore_exact_match) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (nodes_.empty())
    return NodeInfo();
  NthElementSortFromTarget(target_id, 1);
  if ((nodes_[0].node_id == target_id) && (ignore_exact_match)) {
    NthElementSortFromTarget(target_id, 2);
    return nodes_[1];
  }
  return nodes_[0];
}

NodeInfo RoutingTable::GetNthClosestNode(const NodeId& target_id, const uint16_t& node_number) {
  assert((node_number > 0) && "Node number starts with position 1");
  std::lock_guard<std::mutex> lock(mutex_);
  if (nodes_.size() < node_number) {
    NodeInfo node_info;
    node_info.node_id = NodeId(NodeId::kMaxId);
    return node_info;
  }
  NthElementSortFromTarget(target_id, node_number);
  return nodes_[node_number - 1];
}

std::vector<NodeId> RoutingTable::GetClosestNodes(const NodeId& target_id,
                                                  const uint16_t& number_to_get) {
  std::vector<NodeId>close_nodes;
  std::lock_guard<std::mutex> lock(mutex_);
  if (0 == nodes_.size())
    return std::vector<NodeId>();

  uint16_t count = std::min(number_to_get, static_cast<uint16_t>(nodes_.size()));
  PartialSortFromTarget(target_id, count);

  for (unsigned int i = 0; i < count; ++i)
    close_nodes.push_back(nodes_[i].node_id);
  return close_nodes;
}

std::vector<NodeInfo> RoutingTable::GetClosestNodeInfo(const NodeId& from,
                                                       const uint16_t& number_to_get) {
  std::vector<NodeInfo>close_nodes;
  unsigned int count = std::min(number_to_get, static_cast<uint16_t>(nodes_.size()));
  PartialSortFromTarget(from, count);
  for (unsigned int i = 0; i < count; ++i)
    close_nodes.push_back(nodes_[i]);

  return close_nodes;
}

void RoutingTable::update_network_status() const {
  if (network_status_functor_)
    network_status_functor_(static_cast<int>(nodes_.size()) * 100 / max_size_);
}

uint16_t RoutingTable::Size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return static_cast<uint16_t>(nodes_.size());
}

asymm::Keys RoutingTable::kKeys() const {
  return keys_;
}

void RoutingTable::set_network_status_functor(NetworkStatusFunctor network_status_functor) {
  network_status_functor_ = network_status_functor;
}

void RoutingTable::set_close_node_replaced_functor(
    CloseNodeReplacedFunctor close_node_replaced_functor) {
  close_node_replaced_functor_ = close_node_replaced_functor;
}

void RoutingTable::set_keys(const asymm::Keys& keys) {
  keys_ = keys;
}

void RoutingTable::set_bootstrap_file_path(const boost::filesystem::path& path) {
  LOG(kInfo) << "set bootstrap file path : " << path;
  bootstrap_file_path_ = path;
}

}  // namespace routing

}  // namespace maidsafe
