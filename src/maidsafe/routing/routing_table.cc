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
  routing_table_nodes_.clear();
}

bool RoutingTable::AddNode(const NodeId &node_id) {
  boost::mutex::scoped_lock lock(mutex_);
  if (node_id == kMyNodeId_) {
    return false;
  }
  // if we already have node return true
  if (std::find(routing_table_nodes_.begin(),
                routing_table_nodes_.end(), node_id)
      != routing_table_nodes_.end())
    return true;
  if (Size() < Parameters::kMaxRoutingTableSize) {
    routing_table_nodes_.push_back(node_id);
    return true;
  } else if (MakeSpaceForNodeToBeAdded(node_id)) {
      routing_table_nodes_.push_back(node_id);
      return true;
  }
  return false;
}

bool RoutingTable::AmIClosestNode(const NodeId& node_id) {
  boost::mutex::scoped_lock lock(mutex_);
  return ((kMyNodeId_ ^ node_id) <
          (node_id ^ routing_table_nodes_[0]));
}

bool RoutingTable::MakeSpaceForNodeToBeAdded(const NodeId &node_id) {
  if (Parameters::kMaxRoutingTableSize < routing_table_nodes_.size())
    return true;
  SortFromThisNode(kMyNodeId_);
  NodeId furthest_close_node = GetClosestNode(kMyNodeId_, Parameters::kClosestNodesSize);
  if ((furthest_close_node ^ kMyNodeId_) > (kMyNodeId_ ^ node_id)) {
    auto delete_this_node = std::find(routing_table_nodes_.begin(),
                                      routing_table_nodes_.end(),
                                      furthest_close_node);
    if (delete_this_node == routing_table_nodes_.end())
      return false;
    routing_table_nodes_.erase(delete_this_node);
    return true;
  }

//  int i = 0;
//  int16_t node_id_index = BucketIndex(node_id);

  // TODO(Fraser#5#): 2012-03-14 - Protect closest nodes
  for (auto it = routing_table_nodes_.begin();
       it != routing_table_nodes_.end();
       ++it) {
    auto found = routing_table_nodes_.end();

    if (((it + Parameters::kBucketTargetSize + 1) < found) &&
        (BucketIndex(*it) == BucketIndex(*(it + Parameters::kBucketTargetSize + 1))))
      found = it;  // bucket too full

    if (found == routing_table_nodes_.end())
      return false;

    if ((it + Parameters::kBucketTargetSize < routing_table_nodes_.end()) &&
        (BucketIndex(*it) != BucketIndex(*(it + Parameters::kBucketTargetSize))) &&
        (BucketIndex(*it) == BucketIndex(node_id))) {
      routing_table_nodes_.erase(found);
      return true;
//    } else {
//      return false;
    }
  }
  return false;
}

void RoutingTable::SortFromThisNode(const NodeId &from) {
  if ((!sorted_)  || (from != kMyNodeId_))
      std::sort(routing_table_nodes_.begin(),
            routing_table_nodes_.end(),
            [this, from](const NodeId &i, const NodeId &j)
            { return (i ^ from) < (j ^ from); } ); // NOLINT (dirvine)
  if (kMyNodeId_ == from)
    sorted_ = true;
  else
    sorted_ = false;
}

bool RoutingTable::IsMyNodeInRange(const NodeId& node_id, uint16_t range) {
  if (routing_table_nodes_.size() < range)
    return true;
  SortFromThisNode(kMyNodeId_);
  return (routing_table_nodes_[range] ^ kMyNodeId_) > (node_id ^ kMyNodeId_);
}

int16_t RoutingTable::BucketIndex(const NodeId &rhs) const {
  uint16_t distance = 0;
  std::string this_id_binary = kMyNodeId_.ToStringEncoded(NodeId::kBinary);
  std::string rhs_id_binary = rhs.ToStringEncoded(NodeId::kBinary);
  auto this_it = this_id_binary.begin();
  auto rhs_it = rhs_id_binary.begin();
  for (; ((this_it != this_id_binary.end()) && (*this_it == *rhs_it));
      ++this_it, ++rhs_it)
    ++distance;
  return distance;
}

NodeId RoutingTable::GetClosestNode(const NodeId &from,
                                    unsigned int node_number) {
  SortFromThisNode(from);
  return routing_table_nodes_[node_number];
}

std::vector<NodeId> RoutingTable::GetClosestNodes(const NodeId &from,
                                                  unsigned int number_to_get) {
  std::vector<NodeId>close_nodes;
  boost::mutex::scoped_lock lock(mutex_);
  unsigned int count = std::min(number_to_get, Size());
  SortFromThisNode(from);
  close_nodes.resize(count);
  std::copy(routing_table_nodes_.begin(),
            routing_table_nodes_.begin() + count,
            close_nodes.begin());
  return close_nodes;
}

}  // namespace routing
}  // namespace maidsafe
