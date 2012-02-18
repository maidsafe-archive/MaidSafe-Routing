/* Copyright (c) 2009 maidsafe.net limited
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    * Neither the name of the maidsafe.net limited nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "boost/thread/locks.hpp"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/log.h"

namespace maidsafe {

namespace routing {

  class ManagedConnections;
  
RoutingTable::RoutingTable(const NodeId &this_node_id/*, ManagedConnections &MC*/)
    : ThisId_(this_node_id),
    furthest_closest_node_(),
      closest_contacts_(),
      routing_table_nodes_(),
      unvalidated_contacts_(),
      shared_mutex_(),
      closest_contacts_mutex_(),
      routing_table_nodes_mutex_() {}

RoutingTable::~RoutingTable() {
  boost::unique_lock<boost::shared_mutex> unique_lock(shared_mutex_);
  unvalidated_contacts_.clear();
  closest_contacts_.clear();
  routing_table_nodes_.clear();
}

int RoutingTable::AddContact(const Contact &contact) {
  const NodeId node_id = NodeId(contact.node_id());

  // If the contact has the same ID as the holder, return directly
  if (node_id == ThisId_) {
//    DLOG(WARNING) << kDebugId_ << ": Can't add own ID to routing table.";
    return kOwnIdNotIncludable;
  }
  /* TODO implement this
  CheckValidID // get public key and check signature
  CheckValidDistance // test algorithm
  */
  if (Size() < kRoutingTableSize) {
    routing_table_nodes_.push_back(node_id);
  } else if (Size() < kClosestNodes) {
    routing_table_nodes_.push_back(node_id);    
  } else if (isClose(node_id)) {
    // drop furthest_closest_node_ ???
    //update closest nodes
  }
  return kSuccess;
}



bool RoutingTable::isClose(const NodeId& node_id) { 
  return DistanceTo(node_id) < DistanceTo(furthest_closest_node_);
}

void RoutingTable::UpdateClosestNode(NodeId &node_id) {
  if (furthest_closest_node_ > node_id) {
//     MC.Add(node_id);
//     MC.Drop(furthest_closest_node_);
    furthest_closest_node_ = node_id;
  }
}

int16_t RoutingTable::BucketIndex(const NodeId &rhs) const {
  uint16_t distance = 0;
  std::string this_id_binary = ThisId_.ToStringEncoded(NodeId::kBinary);
  std::string rhs_id_binary = rhs.ToStringEncoded(NodeId::kBinary);
  std::string::const_iterator this_it = this_id_binary.begin();
  std::string::const_iterator rhs_it = rhs_id_binary.begin();
  for (; ((this_it != this_id_binary.end()) && (*this_it == *rhs_it));
      ++this_it, ++rhs_it)
    ++distance;
  return (distance + 511) % 511;
}

NodeId RoutingTable::DistanceTo(const NodeId &rhs) const {
  std::string distance;
  std::string this_id_binary = ThisId_.ToStringEncoded(NodeId::kBinary);
  std::string rhs_id_binary = rhs.ToStringEncoded(NodeId::kBinary);
  std::string::const_iterator this_it = this_id_binary.begin();
  std::string::const_iterator rhs_it = rhs_id_binary.begin();
  for (int i = 0; (this_it != this_id_binary.end());++i, ++this_it, ++rhs_it)
    distance[i] = (*this_it ^ *rhs_it);
  NodeId node_dist(distance, NodeId::kBinary);
  return node_dist;
}

protobuf::ClosestContacts RoutingTable::GetMyClosestContacts() {
  protobuf::ClosestContacts pbcontact;
  boost::mutex::scoped_lock lock(closest_contacts_mutex_);
  for (auto it = closest_contacts_.begin(); it != closest_contacts_.end(); ++it)
    *pbcontact.add_close_contacts() = (*it);
  return pbcontact;
}


int16_t RoutingTable::BucketSizeForNode(const NodeId &key) {
  int16_t bucket = BucketIndex(key);
  return bucket;
}

}  // namespace routing

}  // namespace maidsafe
