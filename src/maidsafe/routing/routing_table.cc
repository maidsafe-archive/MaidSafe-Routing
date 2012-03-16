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

#include <algorithm>


#include "boost/thread/locks.hpp"
#include "boost/assert.hpp"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/log.h"

namespace maidsafe {
namespace routing {
  
RoutingTable::RoutingTable(const Contact &my_contact)
    : sorted_(false),
    kMyNodeId_(NodeId(my_contact.node_id())),
    routing_table_nodes_(),
    mutex_() {}

RoutingTable::~RoutingTable() {
  boost::mutex::scoped_lock lock(mutex_);
// TODO (dirvine) Delete this when finiished and happy
//   SortFromThisNode(kMyNodeId_);
//   for (auto it = routing_table_nodes_.begin();
//        it != routing_table_nodes_.end(); ++it)
//     DLOG(INFO) << "bucket of node " << /*((*it).node_id).ToStringEncoded(NodeId::kBinary) << */" is " << (*it).bucket;
  routing_table_nodes_.clear();
}

bool RoutingTable::AddNode(NodeInfo &node, bool node_is_known_valid) {
  boost::mutex::scoped_lock lock(mutex_);

  if (node.node_id == kMyNodeId_) {
    return false;
  }

  /// if we already have node return true
  if (std::find_if(routing_table_nodes_.begin(),
                   routing_table_nodes_.end(),
                   [&node](const NodeInfo &i)->bool
                   { return i.node_id ==  node.node_id; })
                 != routing_table_nodes_.end())
    return false;
  if (MakeSpaceForNodeToBeAdded(node, node_is_known_valid)) {
      if (node_is_known_valid)
        routing_table_nodes_.push_back(node);
      return true;
  }
  return false;
}

bool RoutingTable::AmIClosestNode(const NodeId& node_id) {
  boost::mutex::scoped_lock lock(mutex_);
  return ((kMyNodeId_ ^ node_id) <
          (node_id ^ routing_table_nodes_[0].node_id));
}

/// checks paramters are real
bool RoutingTable::CheckValidParameters(const NodeInfo& node)
{
  if ((!asymm::ValidateKey(node.public_key, 0))) {
    DLOG(INFO) << "invalid public key";
    return false;
  }
          /* TODO FIXME (dirvine) needs uncommented &&
          (!node.endpoint.ip.is_v4()) &&
          (node.endpoint.port > 1500) &&
          (node.endpoint.port < 35000))*/
  if (node.bucket == 99999) {
        DLOG(INFO) << "bad bucket index";
    return false;
  }
  return CheckarametersAreUnique(node);
}

bool RoutingTable::CheckarametersAreUnique(const NodeInfo& node) {

    /// if we already have a duplicate public key return false
  if (std::find_if(routing_table_nodes_.begin(),
                   routing_table_nodes_.end(),
                   [&node](const NodeInfo &i)->bool
                   { return  asymm::MatchingPublicKeys(i.public_key,
                                                       node.public_key);})
                 != routing_table_nodes_.end()) {
    DLOG(INFO) << "Already have node with this public key";
    return false;
  }

  /// if we already have a duplicate endpoint return false
    if (std::find_if(routing_table_nodes_.begin(),
                   routing_table_nodes_.end(),
                   [&node](const NodeInfo &i)->bool
                   { return (i.endpoint.ip.to_string() ==
                            node.endpoint.ip.to_string()) &&
                            (i.endpoint.port == node.endpoint.port ); })
                 != routing_table_nodes_.end()) {
     DLOG(INFO) << "Already have node with this endpoint";
     return false;
    }
    /// node_id was checked in AddNode() so if were here then were unique
  return true;
}

bool RoutingTable::MakeSpaceForNodeToBeAdded(NodeInfo &node, bool remove) {
  node.bucket = BucketIndex(node.node_id);
  if ((remove) && (!CheckValidParameters(node))) {
    DLOG(INFO) << "Invalid Parameters";
    return false;
  }

  if (Size() < Parameters::kMaxRoutingTableSize)
    return true;

  SortFromThisNode(kMyNodeId_);
  NodeInfo furthest_close_node =
           routing_table_nodes_[Parameters::kClosestNodesSize];
  auto not_found = routing_table_nodes_.end();
  auto furthest_close_node_iter =
       routing_table_nodes_.begin() + Parameters::kClosestNodesSize;
  BOOST_ASSERT_MSG(furthest_close_node_iter <= not_found,
                   "cannot find a close node");

  if ((furthest_close_node.node_id ^ kMyNodeId_) >
     (kMyNodeId_ ^ node.node_id)) {
     BOOST_ASSERT_MSG(node.bucket <= furthest_close_node.bucket,
                       "close node replacement to a larger bucket");

     if (remove)
      routing_table_nodes_.erase(furthest_close_node_iter);
    return true;
  }

  for (auto it = furthest_close_node_iter; it != not_found; ++it) {
    if (node.bucket >= (*it).bucket) {
      /// stop searching as it's worthless
      return false;
    }
    /// safety net
    if ((not_found - it) < (Parameters::kBucketTargetSize + 1)) {
      /// reached end of checkable area
      return false;
    }

    if ((*it).bucket == (*(it + Parameters::kBucketTargetSize + 1)).bucket) {
      /// here we know the node should fit into a bucket if
      /// the bucket has too many nodes AND node to add
      /// has a lower bucketindex
      BOOST_ASSERT_MSG(node.bucket < (*it).bucket,
                       "node replacement to a larger bucket");
      if (remove) {
        routing_table_nodes_.erase(it);
      }
      return true;
    }
  }
  return false;
}

void RoutingTable::SortFromThisNode(const NodeId &from) {
  if ((!sorted_)  || (from != kMyNodeId_)) {
    std::sort(routing_table_nodes_.begin(),
              routing_table_nodes_.end(),
    [this, from](const NodeInfo &i, const NodeInfo &j) {
    return (i.node_id ^ from) < (j.node_id ^ from);
    } ); // NOLINT
  }
}

bool RoutingTable::IsMyNodeInRange(const NodeId& node_id, uint16_t range) {
  if (routing_table_nodes_.size() < range)
    return true;

  SortFromThisNode(kMyNodeId_);

  return (routing_table_nodes_[range].node_id ^ kMyNodeId_) >
         (node_id ^ kMyNodeId_);
}

/// bucket 0 is us, 511 is furthest bucket (should fill first)
int16_t RoutingTable::BucketIndex(const NodeId &rhs) const {
  uint16_t bucket = kKeySizeBits - 1;  // (n-1)
  std::string this_id_binary = kMyNodeId_.ToStringEncoded(NodeId::kBinary);
  std::string rhs_id_binary = rhs.ToStringEncoded(NodeId::kBinary);
  auto this_it = this_id_binary.begin();
  auto rhs_it = rhs_id_binary.begin();

  for (;this_it != this_id_binary.end(); ++this_it, ++rhs_it) {
    if (*this_it != *rhs_it)
      return bucket;
    --bucket;
  }
  return bucket;
}

NodeId RoutingTable::GetClosestNode(const NodeId &from,
                                    unsigned int node_number) {
  SortFromThisNode(from);
  sorted_ = false;
  return routing_table_nodes_[node_number].node_id;
}

std::vector<NodeId> RoutingTable::GetClosestNodes(const NodeId &from,
    unsigned int number_to_get) {
  std::vector<NodeId>close_nodes;
  boost::mutex::scoped_lock lock(mutex_);
  unsigned int count = std::min(number_to_get, Size());
  SortFromThisNode(from);
  close_nodes.resize(count);

  for (uint i = 0; i < count; ++i) {
    close_nodes.push_back(routing_table_nodes_[i].node_id);
  }
  return close_nodes;
}

}  // namespace routing
}  // namespace maidsafe
