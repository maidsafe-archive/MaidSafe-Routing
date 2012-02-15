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
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

RoutingTable::RoutingTable()
    : kThisId_(this_id),
      kDebugId_(DebugId(kThisId_)),
      shared_mutex_(),
      bucket_of_holder_(0) {}

RoutingTable::~RoutingTable() {
  UniqueLock unique_lock(shared_mutex_);
  unvalidated_contacts_.clear();
  contacts_.clear();
}

int RoutingTable::AddContact(const Contact &contact, RankInfoPtr rank_info) {
  const NodeId &node_id = contact.node_id();

  // If the contact has the same ID as the holder, return directly
  if (node_id == kThisId_) {
    DLOG(WARNING) << kDebugId_ << ": Can't add own ID to routing table.";
    return kOwnIdNotIncludable;
  }


}

void RoutingTable::InsertContact(const Contact &contact,
                                 RankInfoPtr rank_info,
                                 std::shared_ptr<UpgradeLock> upgrade_lock) {
  uint16_t common_leading_bits = KDistanceTo(contact.node_id());
  uint16_t target_kbucket_index = KBucketIndex(common_leading_bits);
  uint16_t k_bucket_size = KBucketSizeForKey(target_kbucket_index);

  // if the corresponding bucket is full
  if (k_bucket_size == k_) {
    // try to split the bucket if the new contact appears to be in the same
    // bucket as the holder
    if (target_kbucket_index == bucket_of_holder_) {
      SplitKbucket(upgrade_lock);
      InsertContact(contact, rank_info, upgrade_lock);
    } else {
      // try to apply ForceK, otherwise fire the signal
      int force_k_result(ForceKAcceptNewPeer(contact, target_kbucket_index,
                                             rank_info, upgrade_lock));
      if (force_k_result != kSuccess &&
          force_k_result != kFailedToInsertNewContact) {
        Contact oldest_contact = GetLastSeenContact(target_kbucket_index);
        // fire a signal here to notify
        (*ping_oldest_contact_)(oldest_contact, contact, rank_info);
      }
    }
  } else {
    // bucket not full, insert the contact into routing table
    RoutingTableContact new_routing_table_contact(contact, kThisId_,
                                                  rank_info,
                                                  common_leading_bits);
    new_routing_table_contact.kbucket_index = target_kbucket_index;
    UpgradeToUniqueLock unique_lock(*upgrade_lock);
    auto result = contacts_.insert(new_routing_table_contact);
    if (result.second) {
      DLOG(INFO) << kDebugId_ << ": Added node " << DebugId(contact) << ".  "
                 << contacts_.size() << " contacts.";
    } else {
      DLOG(WARNING) << kDebugId_ << ": Failed to insert node "
                    << DebugId(contact);
    }
  }
}

void RoutingTable::GetCloseContacts(
    const NodeId &target_id,
    const size_t &count,
    const std::vector<NodeId> &exclude_contacts,
    std::vector<NodeId> *close_contacts) {
  if (!close_contacts) {
    DLOG(WARNING) << kDebugId_ << ": Null pointer passed.";
    return;
  }
  SharedLock shared_lock(shared_mutex_);

  return;
}

int RoutingTable::SetPublicKey(const NodeId &node_id,
                               const std::string &new_public_key) {
  UpgradeLock upgrade_lock(shared_mutex_);
  ContactsById key_indx = contacts_.get<NodeIdTag>();
  auto it = key_indx.find(node_id);
  if (it == key_indx.end()) {
    DLOG(WARNING) << kDebugId_ << ": Failed to find node " << DebugId(node_id);
    return kFailedToFindContact;
  }
  UpgradeToUniqueLock unique_lock(upgrade_lock);
  if (key_indx.modify(it, ChangePublicKey(new_public_key))) {
    return kSuccess;
  } else {
    DLOG(WARNING) << kDebugId_ << ": Failed to set public key for node "
                  << DebugId(node_id);
    return kFailedToSetPublicKey;
  }
}


int RoutingTable::SetPreferredEndpoint(const NodeId &node_id, const IP &ip) {
  UpgradeLock upgrade_lock(shared_mutex_);
  ContactsById key_indx = contacts_.get<NodeIdTag>();
  auto it = key_indx.find(node_id);
  if (it == key_indx.end()) {
    DLOG(WARNING) << kDebugId_ << ": Failed to find node " << DebugId(node_id);
    return kFailedToFindContact;
  }
  Contact new_local_contact((*it).contact);
  new_local_contact.SetPreferredEndpoint(ip);
  UpgradeToUniqueLock unique_lock(upgrade_lock);
  if (key_indx.modify(it, ChangeContact(new_local_contact))) {
    return kSuccess;
  } else {
    DLOG(WARNING) << kDebugId_ << ": Failed to set preferred endpt for node "
                  << DebugId(node_id);
    return kFailedToSetPreferredEndpoint;
  }
}

int RoutingTable::SetValidated(const NodeId &node_id, bool validated) {

  return kSuccess;
}

void RoutingTable::GetBootstrapContacts(std::vector<Contact> *contacts) {
  if (!contacts)
    return;

  SharedLock shared_lock(shared_mutex_);
  auto it = contacts_.get<BootstrapTag>().equal_range(true);
  contacts->clear();
  while (it.first != it.second)
    contacts->push_back((*it.first++).contact);

  if (contacts->size() < kMinBootstrapContacts) {
    it = contacts_.get<BootstrapTag>().equal_range(false);
    while (it.first != it.second)
      contacts->push_back((*it.first++).contact);
  }
}

void RoutingTable::GetAllContacts(std::vector<Contact> *contacts) {
  if (!contacts) {
    DLOG(WARNING) << kDebugId_ << ": Null pointer passed.";
    return;
  }
  SharedLock shared_lock(shared_mutex_);
  ContactsById key_indx = contacts_.get<NodeIdTag>();
  auto it = key_indx.begin();
  auto it_end = key_indx.end();
  contacts->clear();
  contacts->reserve(distance(it, it_end));
  while (it != it_end) {
    contacts->push_back((*it).contact);
    ++it;
  }
}

uint16_t RoutingTable::KBucketIndex(const NodeId &key) {
//   if (key > NodeId::kMaxId)
//     return -1;
  uint16_t common_leading_bits = KDistanceTo(key);
  if (common_leading_bits > bucket_of_holder_)
    common_leading_bits = bucket_of_holder_;
  return common_leading_bits;
}

uint16_t RoutingTable::KBucketIndex(const uint16_t &common_leading_bits) {
  if (common_leading_bits > bucket_of_holder_)
    return bucket_of_holder_;
  return common_leading_bits;
}

uint16_t RoutingTable::KBucketCount() const {
  return bucket_of_holder_+1;
}

uint16_t RoutingTable::KBucketSizeForKey(const uint16_t &key) {
  if (key > bucket_of_holder_) {
    auto pit = contacts_.get<KBucketTag>().equal_range(bucket_of_holder_);
    return static_cast<uint16_t>(distance(pit.first, pit.second));
  } else {
    auto pit = contacts_.get<KBucketTag>().equal_range(key);
    return static_cast<uint16_t>(distance(pit.first, pit.second));
  }
}

}  // namespace routing

}  // namespace maidsafe
