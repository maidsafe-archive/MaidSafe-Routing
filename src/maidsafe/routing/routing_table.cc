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

#include <algorithm>

#include "boost/thread/locks.hpp"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/log.h"

namespace maidsafe {
namespace routing {
  
RoutingTable::RoutingTable(const NodeId &this_node_id)
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
  if (node_id == ThisId_) {
    return kOwnIdNotIncludable;
  }
  /* TODO implement this
  CheckValidID // get public key and check signature
  or return kFalse_ID;
  */
  if (isClose(node_id)) {
    if (routing_table_nodes_.size() >= kRoutingTableSize) {
      // TODO drop furthest_closest_node_ connection in Transport ???
      // TODO try to Connect to node in Transport
      if (RemoveClosecontact(furthest_closest_node_) &&
          AddcloseContact(contact)) {
        routing_table_nodes_.push_back(NodeId(contact.node_id()));
        return kSuccess;
      } else
        return kFailedToInsertNewContact;
    }
  } else if (IsSpaceForNodeToBeAdded()) {
      // TODO try to Connect to node in Transport
      // Add node
      return kSuccess; // or fail if cannot connect 
  }
  return kSuccess;
}

bool RoutingTable::IsSpaceForNodeToBeAdded() {
  if (kRoutingTableSize < routing_table_nodes_.size())
    return true;

  auto nth = routing_table_nodes_.begin() + kBucketSize;
  std::nth_element(routing_table_nodes_.begin(),
                  nth,
                  routing_table_nodes_.end(),
                  [this](const NodeId &i, const NodeId &j)
                  { return BucketIndex(i) == BucketIndex(j);});

  if (nth != routing_table_nodes_.end()) {
    // TODO remove *nth from managed connection
    routing_table_nodes_.erase(nth);
    return true;
  }
  return false;
}

bool RoutingTable::AddcloseContact(const Contact& contact) {
  size_t sizebefore = closest_contacts_.size();
  closest_contacts_.push_back(contact);
  if (sizebefore < closest_contacts_.size())
    return true;
  return false;
}

bool RoutingTable::RemoveClosecontact(const NodeId& node_id){
  auto it = std::find_if(closest_contacts_.begin(),
                      closest_contacts_.end(),
                      [node_id](Contact &i)
                      { return i.node_id() == node_id.String(); });

  if (it != closest_contacts_.end()) {
    closest_contacts_.erase(it);
    return true;
  }
  return false;
}

bool RoutingTable::isClose(const NodeId& node_id) const { 
  return DistanceTo(node_id) < DistanceTo(furthest_closest_node_);
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

int16_t RoutingTable::BucketSizeForNode(const NodeId &key) const {
  int16_t bucket = BucketIndex(key);
  return bucket;
}

}  // namespace routing

}  // namespace maidsafe
