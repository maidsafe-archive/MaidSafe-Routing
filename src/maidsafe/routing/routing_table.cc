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
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/log.h"

namespace maidsafe {

namespace routing {

RoutingTable::RoutingTable(NodeId &this_node_id)
    : ThisId_(this_node_id),
      kDebugId_(),
      shared_mutex_() {}

RoutingTable::~RoutingTable() {
  UniqueLock unique_lock(shared_mutex_);
  unvalidated_contacts_.clear();
  closest_contacts_.clear();
  routing_table_nodes_.clear();
}

int RoutingTable::AddContact(const Contact &contact) {
  const NodeId node_id = NodeId(contact.node_id());

  // If the contact has the same ID as the holder, return directly
  if (node_id == ThisId_) {
    DLOG(WARNING) << kDebugId_ << ": Can't add own ID to routing table.";
    return kOwnIdNotIncludable;
  }
  /* TODO implement this
  CheckValidID // get public key and check signature
  CheckValidDistance // test algorithm
  */
}

// Done
int16_t RoutingTable::DistanceTo(const NodeId &rhs) const {
  uint16_t distance = 0;
  std::string this_id_binary = ThisId_.ToStringEncoded(NodeId::kBinary);
  std::string rhs_id_binary = rhs.ToStringEncoded(NodeId::kBinary);
  std::string::const_iterator this_it = this_id_binary.begin();
  std::string::const_iterator rhs_it = rhs_id_binary.begin();
  for (; ((this_it != this_id_binary.end()) && (*this_it == *rhs_it));
      ++this_it, ++rhs_it)
    ++distance;
  return distance;
}

// TODO
void RoutingTable::GetCloseContacts(
    const NodeId &target_id,
    const size_t &count,
    const std::vector<Contact> &exclude_contacts,
    std::vector<Contact> *close_contacts) {
  if (!close_contacts) {
    DLOG(WARNING) << kDebugId_ << ": Null pointer passed.";
    return;
  }
}

// done
std::string RoutingTable::GetClosestContacts() {
 
  protobuf::ClosestContacts pbcontact;
  SharedLock shared_lock(shared_mutex_);
  // TODO FIXME
//   for (auto it = closest_contacts_.begin(); it != closest_contacts_.end(); ++it)
//      pbcontact.add_close_contacts((*it));
    
}

int16_t RoutingTable::BucketIndex(const NodeId &key) {
  return (DistanceTo(key) + 511) % 511;
}

int16_t RoutingTable::BucketSizeForNode(const NodeId &key) {
  int16_t bucket = BucketIndex(key);
  
}

}  // namespace routing

}  // namespace maidsafe
