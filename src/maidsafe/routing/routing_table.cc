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
#include <limits>

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/return_codes.h"


namespace maidsafe {

namespace routing {

RoutingTable::RoutingTable(const asymm::Keys& keys, const bool& client_mode)
    : max_size_(client_mode ? Parameters::max_client_routing_table_size :
                              Parameters::max_routing_table_size),
      client_mode_(client_mode),
      keys_(keys),
      sorted_(false),
      kNodeId_(NodeId(keys_.identity)),
      kConnectionId_((client_mode ? NodeId(NodeId::kRandomId) : kNodeId_)),
      furthest_group_node_id_(),
      mutex_(),
      remove_node_functor_(),
      network_status_functor_(),
      close_node_replaced_functor_(),
      nodes_(),
      pending_nodes_() {}

bool RoutingTable::AddNode(NodeInfo& peer) {
  return AddOrCheckNode(peer, true);
}

bool RoutingTable::CheckNode(NodeInfo& peer) {
  return AddOrCheckNode(peer, false);
}

bool RoutingTable::AddOrCheckNode(NodeInfo& peer, const bool& remove) {
  bool return_value(false);
  std::vector<NodeInfo> new_close_nodes;
  NodeInfo removed_node, duplicate_removed_node;
  uint16_t routing_table_size;
  {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!peer.node_id.IsValid() || peer.node_id.Empty()) {
      LOG(kError) << "Attempt to add an invalid node";
      return false;
    }

    if (peer.node_id == kNodeId_) {
      LOG(kError) << "Tried to add this node";
      return false;
    }

    // If we already have node, return false.
    auto itr(std::find_if(nodes_.begin(),
                          nodes_.end(),
                          [peer](const NodeInfo& node_info) {
                              return node_info.node_id == peer.node_id;
                          }));
    if (itr != nodes_.end()) {
      LOG(kVerbose) << "Node " << HexSubstr(peer.node_id.String()) << " already in routing table.";
      return false;
    }

    if (MakeSpaceForNodeToBeAdded(peer, remove, removed_node)) {
      if (remove) {
        nodes_.push_back(peer);
        routing_table_size = static_cast<uint16_t>(nodes_.size());
        new_close_nodes = CheckGroupChange();
      }
      return_value = true;
    }
  }

  if (return_value && remove) {  // Firing functors on Add only
    UpdateNetworkStatus(routing_table_size);

    if (!removed_node.node_id.Empty()) {
      LOG(kVerbose) << "Routing table removed node id : " << DebugId(removed_node.node_id)
                    << ", connection id : " << DebugId(removed_node.connection_id);
      if (remove_node_functor_)
        remove_node_functor_(removed_node, false);
    }

    if (!new_close_nodes.empty()) {
      if (close_node_replaced_functor_)
        close_node_replaced_functor_(new_close_nodes);
    }

    if (peer.nat_type == rudp::NatType::kOther) {  // Usable as bootstrap endpoint
//      if (new_bootstrap_endpoint_)
//        new_bootstrap_endpoint_(peer.endpoint);
    }
    LOG(kInfo) << PrintRoutingTable();
  }
  return return_value;
}

NodeInfo RoutingTable::DropNode(const NodeId& node_to_drop, const bool& routing_only) {
  std::vector<NodeInfo> new_close_nodes;
  NodeInfo dropped_node;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
      if ((*it).node_id == node_to_drop) {
        dropped_node = (*it);
        nodes_.erase(it);
        new_close_nodes = CheckGroupChange();
        break;
      }
    }
  }

  if (!dropped_node.node_id.Empty()) {
    assert(nodes_.size() <= std::numeric_limits<uint16_t>::max());
    UpdateNetworkStatus(static_cast<uint16_t>(nodes_.size()));
  }

  if (!new_close_nodes.empty()) {
    if (close_node_replaced_functor_)
      close_node_replaced_functor_(new_close_nodes);
  }

  if (!dropped_node.node_id.Empty()) {
    LOG(kVerbose) << "Routing table dropped node id : " << DebugId(dropped_node.node_id)
                  << ", connection id : " << DebugId(dropped_node.connection_id);
    if (remove_node_functor_  && !routing_only)
      remove_node_functor_(dropped_node, false);
  }
  LOG(kInfo) << PrintRoutingTable();
  return dropped_node;
}

bool RoutingTable::GetNodeInfo(const NodeId& node_id, NodeInfo& peer) const {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
    if ((*it).node_id == node_id) {
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

// bool RoutingTable::IsThisNodeClosestTo(const NodeId& target_id) {
//  if (!target_id.IsValid() || target_id.Empty()) {
//    LOG(kError) << "Invalid target_id passed.";
//    return false;
//  }
//  std::lock_guard<std::mutex> lock(mutex_);
//  if (nodes_.empty())
//    return true;
//  NthElementSortFromTarget(target_id, 1);
//  return (kNodeId_ ^ target_id) < (target_id ^ nodes_[0].node_id);
// }

bool RoutingTable::IsThisNodeClosestTo(const NodeId& target_id, bool ignore_exact_match) {
  if (!target_id.IsValid() || target_id.Empty()) {
    LOG(kError) << "Invalid target_id passed.";
    return false;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (nodes_.empty())
    return true;

  if (nodes_.size() == 1) {
    if (ignore_exact_match && (nodes_[0].node_id == target_id))
      return true;
    else
      return (kNodeId_ ^ target_id) < (target_id ^ nodes_[0].node_id);
  }

  PartialSortFromTarget(target_id, 2);
  int index(0);
  if (ignore_exact_match && (nodes_[0].node_id == target_id))
    index = 1;
  return (kNodeId_ ^ target_id) < (target_id ^ nodes_[index].node_id);
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

std::vector<NodeInfo> RoutingTable::CheckGroupChange() {
  PartialSortFromTarget(kNodeId_, std::min(Parameters::closest_nodes_size,
                                           static_cast<uint16_t>(nodes_.size())));
  if (nodes_.size() > Parameters::closest_nodes_size) {
    NodeId new_furthest_group_node_id = nodes_[Parameters::closest_nodes_size - 1].node_id;
    if (furthest_group_node_id_ != new_furthest_group_node_id) {
      LOG(kVerbose) << "Group change !. old furthest_close_node : "
                    << DebugId(furthest_group_node_id_)
                    << "new_furthest_group_node_id : "
                    << DebugId(new_furthest_group_node_id);
      furthest_group_node_id_ = nodes_[Parameters::closest_nodes_size - 1].node_id;
      return (std::vector<NodeInfo>(nodes_.begin(), nodes_.begin() +
                Parameters::closest_nodes_size));
    } else {  // No change
      return std::vector<NodeInfo>();
    }
  } else {
    if (!nodes_.empty())
       furthest_group_node_id_ = nodes_.back().node_id;
    return nodes_;
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

  // If the endpoint is kNonRoutable then no need to check for endpoint duplication.
//  if (node.endpoint == rudp::kNonRoutable)
//    return true;
  // If we already have a duplicate endpoint return false
//  if (std::find_if(nodes_.begin(),
//                   nodes_.end(),
//                   [node](const NodeInfo& node_info) {
//                     return (node_info.endpoint == node.endpoint);
//                   }) != nodes_.end()) {
//    LOG(kInfo) << "Already have node with this endpoint";
//    return false;
//  }

  return true;
}

bool RoutingTable::MakeSpaceForNodeToBeAdded(NodeInfo& node, const bool& remove,
                                             NodeInfo& removed_node) {
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

    if (remove) {
      removed_node = *furthest_close_node_iter;
      nodes_.erase(furthest_close_node_iter);
    }
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
      if (remove) {
        removed_node = *it;
        nodes_.erase(it);
      }
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
    if (nodes_.size() >= 2)
      NthElementSortFromTarget(target_id, 2);
    else
      return NodeInfo();
    return nodes_[1];
  }
  return nodes_[0];
}

NodeInfo RoutingTable::GetClosestNode(const NodeId& target_id,
                                      const std::vector<std::string>& exclude,
                                      bool ignore_exact_match) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<NodeInfo> closest_nodes(GetClosestNodeInfo(target_id,
                                                         Parameters::closest_nodes_size,
                                                         ignore_exact_match));
  if (closest_nodes.empty())
    return NodeInfo();

  if (exclude.empty())
    return closest_nodes[0];

  for (auto node_info : closest_nodes) {
    if (std::find(exclude.begin(), exclude.end(), node_info.node_id.String()) == exclude.end())
      return node_info;
  }
  return NodeInfo();
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
                                                       const uint16_t& number_to_get,
                                                       bool ignore_exact_match) {
  std::vector<NodeInfo> close_nodes;
  bool exact_match_exist(false);
  uint16_t count = std::min(number_to_get, static_cast<uint16_t>(nodes_.size()));
  if (ignore_exact_match && (nodes_.size() >= count))
    if (std::find_if(nodes_.begin(), nodes_.end(),
                     [&](const NodeInfo& node_info) {
                       return (node_info.node_id == from);
                     }) != nodes_.end()) {
      count++;
      exact_match_exist = true;
    }
  count = std::min(static_cast<uint16_t>(nodes_.size()), count);
  PartialSortFromTarget(from, count);
  for (uint16_t i = static_cast<uint16_t>(ignore_exact_match && exact_match_exist);
         i < count; ++i)
    close_nodes.push_back(nodes_[i]);

  return close_nodes;
}

void RoutingTable::UpdateNetworkStatus(const uint16_t& size) const {
  if (network_status_functor_)
    network_status_functor_(static_cast<int>(size) * 100 / max_size_);
  LOG(kInfo) << DebugId(kNodeId_) << "Updating network status !!!" << (size * 100) / max_size_;
}

size_t RoutingTable::Size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return static_cast<uint16_t>(nodes_.size());
}

asymm::Keys RoutingTable::kKeys() const {
  return keys_;
}

NodeId RoutingTable::kNodeId() const {
  return kNodeId_;
}

NodeId RoutingTable::kConnectionId() const {
  return kConnectionId_;
}

void RoutingTable::set_remove_node_functor(
    std::function<void(const NodeInfo&, const bool&)> remove_node_functor) {
  remove_node_functor_ = remove_node_functor;
}

void RoutingTable::set_network_status_functor(NetworkStatusFunctor network_status_functor) {
  network_status_functor_ = network_status_functor;
}

void RoutingTable::set_close_node_replaced_functor(
    CloseNodeReplacedFunctor close_node_replaced_functor) {
  close_node_replaced_functor_ = close_node_replaced_functor;
}

std::string RoutingTable::PrintRoutingTable() {
  std::vector<NodeInfo> rt;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    rt = nodes_;
  }
  std::string s = "\n\n[" + DebugId(kNodeId_) +
      "] This node's own routing table and peer connections:\n";
  for (auto node : rt) {
    s += std::string("\tPeer ") + "[" + DebugId(node.node_id) + "]"+ "-->";
    s += DebugId(node.connection_id)+ "\n";
  }
  s += "\n\n";
  return s;
}

}  // namespace routing

}  // namespace maidsafe
