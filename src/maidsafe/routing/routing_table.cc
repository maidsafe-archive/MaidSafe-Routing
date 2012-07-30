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

#include "maidsafe/common/utils.h"

#include "maidsafe/routing/log.h"

namespace bs2 = boost::signals2;

namespace maidsafe {

namespace routing {


RoutingTable::RoutingTable(const asymm::Keys &keys, const bool &client_mode,
                           CloseNodeReplacedFunctor /*close_node_replaced_functor*/)
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
      routing_table_nodes_() {}

void RoutingTable::set_keys(asymm::Keys keys) {
  keys_ = keys;
}

bool RoutingTable::CheckNode(NodeInfo& node) {
  return AddOrCheckNode(node, false);
}

bool RoutingTable::AddNode(NodeInfo& node) {
  return AddOrCheckNode(node, true);
}

bool RoutingTable::AddOrCheckNode(NodeInfo& node, const bool &remove) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (node.node_id == kNodeId_) {
    LOG(kInfo) << "tried to add own node !!";
    return false;
  }
  // if we already have node return false
  if (std::find_if(routing_table_nodes_.begin(), routing_table_nodes_.end(),
                   [node](const NodeInfo &i)->bool
                   { return i.node_id ==  node.node_id; })
                 != routing_table_nodes_.end()) {
    LOG(kInfo) << "Node already there in RT!!" << HexSubstr(node.node_id.String());
    return false;
  }
  if (MakeSpaceForNodeToBeAdded(node, remove)) {
    if (remove) {
      routing_table_nodes_.push_back(node);
      update_network_status();
      UpdateGroupChangeAndNotify();
    }
    return true;
  }
  return false;
}

uint16_t RoutingTable::Size() {
  std::lock_guard<std::mutex> lock(mutex_);
  return RoutingTableSize();
}

uint16_t RoutingTable::RoutingTableSize() {
  return static_cast<uint16_t>(routing_table_nodes_.size());
}

asymm::Keys RoutingTable::kKeys() const {
  return keys_;
}

void RoutingTable::set_close_node_replaced_functor(CloseNodeReplacedFunctor close_node_replaced) {
  close_node_replaced_functor_ = close_node_replaced;
}

void RoutingTable::set_network_status_functor(NetworkStatusFunctor network_status_functor) {
  network_status_functor_ = network_status_functor;
}

void RoutingTable::update_network_status() {
  if (network_status_functor_)
    network_status_functor_(RoutingTableSize() * 100 / max_size_);
}

NodeInfo RoutingTable::DropNode(const Endpoint &endpoint) {
  NodeInfo dropped_node;
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = routing_table_nodes_.begin(); it != routing_table_nodes_.end(); ++it) {
    if (((*it).endpoint ==  endpoint)) {
      dropped_node = (*it);
      routing_table_nodes_.erase(it);
      update_network_status();
      UpdateGroupChangeAndNotify();
      break;
    }
  }
  return dropped_node;
}

bool RoutingTable::GetNodeInfo(const Endpoint &endpoint, NodeInfo *node_info) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = routing_table_nodes_.begin(); it != routing_table_nodes_.end(); ++it) {
    if (((*it).endpoint ==  endpoint)) {
      *node_info = (*it);
      return true;
    }
  }
  return false;
}

bool RoutingTable::AmIClosestNode(const NodeId& node_id) {
  if (!node_id.IsValid()) {
    LOG(kError) << "Invalid node_id passed";
    return false;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (routing_table_nodes_.empty())
    return true;
  NthElementSortFromThisNode(node_id, 1);
  return ((kNodeId_ ^ node_id) <
          (node_id ^ routing_table_nodes_[0].node_id));
}

bool RoutingTable::AmIConnectedToEndpoint(const Endpoint& endpoint) {
  std::lock_guard<std::mutex> lock(mutex_);
  return (std::find_if(routing_table_nodes_.begin(), routing_table_nodes_.end(),
                       [endpoint](const NodeInfo &i)->bool
                       { return i.endpoint == endpoint; })
                     != routing_table_nodes_.end());
}

bool RoutingTable::AmIConnectedToNode(const NodeId& node_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  return (std::find_if(routing_table_nodes_.begin(), routing_table_nodes_.end(),
                       [node_id](const NodeInfo &i)->bool
                       { return i.node_id == node_id; })
                     != routing_table_nodes_.end());
}

bool RoutingTable::ConfirmGroupMembers(const NodeId& node1, const NodeId& node2) {
  NodeId difference = NodeId(kKeys().identity) ^ FurthestCloseNode();
  return (node1 ^ node2) < difference;
}

NodeId RoutingTable::FurthestCloseNode() {
  return GetNthClosestNode(NodeId(kKeys().identity), Parameters::closest_nodes_size).node_id;
}

// checks paramters are real
bool RoutingTable::CheckValidParameters(const NodeInfo& node) const {
  if ((!asymm::ValidateKey(node.public_key, 0))) {
    LOG(kInfo) << "invalid public key";
    return false;
  }

  if (node.bucket == 99999) {
    LOG(kInfo) << "invalid bucket index";
    return false;
  }
  return CheckParametersAreUnique(node);
}

bool RoutingTable::CheckParametersAreUnique(const NodeInfo& node) const {
  // if we already have a duplicate public key return false
  if (std::find_if(routing_table_nodes_.begin(),
                   routing_table_nodes_.end(),
                   [node](const NodeInfo &i)->bool
                   { return  asymm::MatchingPublicKeys(i.public_key, node.public_key);})
                 != routing_table_nodes_.end()) {
    LOG(kInfo) << "Already have node with this public key";
    return false;
  }

  // if we already have a duplicate endpoint return false
  if (std::find_if(routing_table_nodes_.begin(), routing_table_nodes_.end(),
                   [node](const NodeInfo &i)->bool
                   { return (i.endpoint == node.endpoint); })
                 != routing_table_nodes_.end()) {
    LOG(kInfo) << "Already have node with this endpoint";
    return false;
  }
  // node_id was checked in AddNode() so if were here then were unique
  return true;
}

bool RoutingTable::MakeSpaceForNodeToBeAdded(NodeInfo &node, const bool &remove) {
  node.bucket = BucketIndex(node.node_id);
  if ((remove) && (!CheckValidParameters(node))) {
    LOG(kInfo) << "Invalid Parameters";
    return false;
  }

  if (RoutingTableSize() < max_size_) {
    return true;
  }

  PartialSortFromThisNode(kNodeId_, Parameters::closest_nodes_size);
  NodeInfo furthest_close_node = routing_table_nodes_[Parameters::closest_nodes_size - 1];
  auto const not_found = routing_table_nodes_.end();
  auto const furthest_close_node_iter =
      routing_table_nodes_.begin() + (Parameters::closest_nodes_size - 1);

  if ((furthest_close_node.node_id ^ kNodeId_) > (kNodeId_ ^ node.node_id)) {
    BOOST_ASSERT_MSG(node.bucket <= furthest_close_node.bucket,
                     "close node replacement to a larger bucket");

    if (remove) {
      routing_table_nodes_.erase(furthest_close_node_iter);
    }
    return true;
  }

  uint16_t size(Parameters::bucket_target_size + 1);
  for (auto it = furthest_close_node_iter; it != not_found; ++it) {
    if (node.bucket >= (*it).bucket) {
      // stop searching as it's worthless
      return false;
    }
    // safety net
    if ((not_found - it) < size) {
      // reached end of checkable area
      return false;
    }

    if ((*it).bucket == (*(it + size)).bucket) {
      // here we know the node should fit into a bucket if
      // the bucket has too many nodes AND node to add
      // has a lower bucketindex
      BOOST_ASSERT(node.bucket < (*it).bucket);  // , "node replacement to a larger bucket");
      if (remove) {
        routing_table_nodes_.erase(it);
      }
      return true;
    }
  }
  return false;
}

void RoutingTable::UpdateGroupChangeAndNotify() {
  if (close_node_replaced_functor_) {
    if (RoutingTableSize() >= Parameters::node_group_size) {
      NthElementSortFromThisNode(kNodeId_, Parameters::node_group_size);
      NodeId new_furthest_group_node_id =
          routing_table_nodes_[Parameters::node_group_size - 1].node_id;
      if (furthest_group_node_id_ != new_furthest_group_node_id) {
        std::vector<NodeInfo> new_close_nodes(GetClosestNodeInfo(kNodeId_,
            Parameters::node_group_size));
        furthest_group_node_id_ = new_close_nodes[Parameters::node_group_size - 1].node_id;
        close_node_replaced_functor_(new_close_nodes);
      }
    } else {
       std::vector<NodeInfo> new_close_nodes(GetClosestNodeInfo(kNodeId_,
           Parameters::node_group_size));
       furthest_group_node_id_ = new_close_nodes[RoutingTableSize() - 1].node_id;
       close_node_replaced_functor_(new_close_nodes);
    }
  }
}

void RoutingTable::SortFromThisNode(const NodeId &from) {
  if ((!sorted_)  || (from != kNodeId_)) {
    std::sort(routing_table_nodes_.begin(), routing_table_nodes_.end(),
              [this, from](const NodeInfo &i, const NodeInfo &j) {
                return (i.node_id ^ from) < (j.node_id ^ from);
              } ); // NOLINT
  }
  if (from != kNodeId_)
    sorted_ = false;
}

void RoutingTable::PartialSortFromThisNode(const NodeId &from, const uint16_t &number) {
  uint16_t count = std::min(number, RoutingTableSize());
  std::partial_sort(routing_table_nodes_.begin(), routing_table_nodes_.begin() + count,
                    routing_table_nodes_.end(),
                    [this, from](const NodeInfo &i, const NodeInfo &j) {
                      return (i.node_id ^ from) < (j.node_id ^ from);
                    } ); // NOLINT
}

void RoutingTable::NthElementSortFromThisNode(const NodeId &from, const uint16_t &nth_element) {
  assert((routing_table_nodes_.size() >= nth_element) &&
         "This should only be called when n is at max the size of RT");
  std::nth_element(routing_table_nodes_.begin(), routing_table_nodes_.begin() + nth_element,
                   routing_table_nodes_.end(),
                   [this, from](const NodeInfo &i, const NodeInfo &j) {
                     return (i.node_id ^ from) < (j.node_id ^ from);
                   } ); // NOLINT
}

bool RoutingTable::IsMyNodeInRange(const NodeId& node_id, const uint16_t range)  {
  std::lock_guard<std::mutex> lock(mutex_);
  if (routing_table_nodes_.size() < range)
    return true;

  PartialSortFromThisNode(kNodeId_, range);

  return (routing_table_nodes_[range - 1].node_id ^ kNodeId_) > (node_id ^ kNodeId_);
}

// bucket 0 is us, 511 is furthest bucket (should fill first)
int16_t RoutingTable::BucketIndex(const NodeId &rhs) const {
  uint16_t bucket = kKeySizeBits - 1;  // (n-1losestNode(my_closest_node
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

NodeInfo RoutingTable::GetNthClosestNode(const NodeId &from, const uint16_t &node_number) {
  assert((node_number > 0) && "Node number starts with position 1");
  std::lock_guard<std::mutex> lock(mutex_);
  if (RoutingTableSize() < node_number) {
    NodeInfo node_info;
    node_info.node_id = NodeId(NodeId::kMaxId);
    return node_info;
  }
  NthElementSortFromThisNode(from, node_number);
  return routing_table_nodes_[node_number - 1];
}

NodeInfo RoutingTable::GetClosestNode(const NodeId &from, bool ignore_exact_match) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (RoutingTableSize() == 0) {
    return NodeInfo();
  }
  NthElementSortFromThisNode(from, 1);
  if ((routing_table_nodes_[0].node_id == from) && (ignore_exact_match))
    NthElementSortFromThisNode(from, 2);
  return routing_table_nodes_[0];
}

std::vector<NodeId> RoutingTable::GetClosestNodes(const NodeId &from,
                                                  const uint16_t &number_to_get) {
  std::vector<NodeId>close_nodes;
  std::lock_guard<std::mutex> lock(mutex_);
  uint16_t count = std::min(number_to_get, RoutingTableSize());
  PartialSortFromThisNode(from, count);
  close_nodes.reserve(count);

  for (unsigned int i = 0; i < count; ++i) {
    close_nodes.push_back(routing_table_nodes_[i].node_id);
  }
  return close_nodes;
}

std::vector<NodeInfo> RoutingTable::GetClosestNodeInfo(const NodeId &from,
                                                       const uint16_t &number_to_get) {
  std::vector<NodeInfo>close_nodes;
  unsigned int count = std::min(number_to_get, RoutingTableSize());
  PartialSortFromThisNode(from, number_to_get);
  close_nodes.resize(count);

  for (unsigned int i = 0; i < count; ++i) {
    close_nodes.push_back(routing_table_nodes_[i]);
  }
  return close_nodes;
}

}  // namespace routing

}  // namespace maidsafe
