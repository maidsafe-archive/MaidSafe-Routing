/*  Copyright 2014 MaidSafe.net limited

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

#ifndef MAIDSAFE_ROUTING_ROUTING_TABLE_H_
#define MAIDSAFE_ROUTING_ROUTING_TABLE_H_

#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>

#include "boost/optional.hpp"

#include "maidsafe/common/rsa.h"

#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

struct NodeInfo;

// The RoutingTable class is used to maintain a list of contacts to which we are connected.  It is
// threadsafe and all public functions offer the strong exception guarantee.  Any public function
// having an Address or NodeInfo arg will throw if NDEBUG is defined and the passed ID is invalid.
// These functions assert that any such ID is valid, so it should be considered a bug if any such
// function throws.  Other than bad_allocs, there are no other exceptions thrown from this class.
class RoutingTable {
 public:
  static size_t BucketSize() { return 1; }
  static size_t Parallelism() { return 4; }
  static size_t OptimalSize() { return 64; }

  explicit RoutingTable(Address our_id);
  RoutingTable(const RoutingTable&) = delete;
  RoutingTable(RoutingTable&&) = delete;
  RoutingTable& operator=(const RoutingTable&) = delete;
  RoutingTable& operator=(RoutingTable&&) MAIDSAFE_NOEXCEPT = delete;
  ~RoutingTable() = default;

  // Potentially adds a contact to the routing table.  If the contact is added, the first return arg
  // is true, otherwise false.  If adding the contact caused another contact to be dropped, the
  // dropped one is returned in the second field, otherwise the optional field is empty.  The
  // following steps are used to determine whether to add the new contact or not:
  //
  // 1 - if the contact is ourself, or doesn't have a valid public key, or is already in the table,
  //     it will not be added
  // 2 - if the routing table is not full (size < OptimalSize()), the contact will be added
  // 3 - if the contact is within our close group, it will be added
  // 4 - if we can find a candidate for removal (a contact in a bucket with more than 'BucketSize()'
  //     contacts, which is also not within our close group), and if the new contact will fit in a
  //     bucket closer to our own bucket, then we add the new contact.
  std::pair<bool, boost::optional<NodeInfo>> AddNode(NodeInfo their_info);

  // This is used to see whether to bother retrieving a contact's public key from the PKI with a
  // view to adding the contact to our table.  The checking procedure is the same as for 'AddNode'
  // above, except for the lack of a public key to check in step 1.
  bool CheckNode(const Address& their_id) const;

  // This unconditionally removes the contact from the table.
  void DropNode(const Address& node_to_drop);

  // This returns a collection of contacts to which a message should be sent onwards.  It will
  // return all of our close group (comprising 'GroupSize' contacts) if the closest one to the
  // target is within our close group.  If not, it will return the 'Parallelism()' closest contacts
  // to the target.
  std::vector<NodeInfo> TargetNodes(const Address& target) const;

  // This returns our close group, i.e. the 'GroupSize' contacts closest to our ID (or the entire
  // table if we hold less than 'GroupSize' contacts in total).
  std::vector<NodeInfo> OurCloseGroup() const;

  // This returns the public key for the given node if the node is in our table.
  boost::optional<asymm::PublicKey> GetPublicKey(const Address& their_id) const;

  const Address& OurId() const { return our_id_; }

  size_t Size() const;

  int32_t BucketIndex(const Address& node_id) const;

 private:
  class Comparison {
   public:
    using NodesItr = std::vector<NodeInfo>::const_iterator;
    explicit Comparison(Address our_id) : our_id_(std::move(our_id)) {}
    bool operator()(const NodeInfo& lhs, const NodeInfo& rhs) const {
      return Address::CloserToTarget(lhs.id, rhs.id, our_id_);
    }
    bool operator()(NodesItr lhs, NodesItr rhs) const {
      return Address::CloserToTarget(lhs->id, rhs->id, our_id_);
    }

   private:
    const Address our_id_;
  };
  bool HaveNode(const NodeInfo& their_info) const;
  bool NewNodeIsBetterThanExisting(const Address& their_id,
                                   std::vector<NodeInfo>::const_iterator removal_candidate) const;
  void PushBackThenSort(NodeInfo their_info);
  std::vector<NodeInfo>::const_iterator FindCandidateForRemoval() const;

  const Address our_id_;
  const Comparison comparison_;
  mutable std::mutex mutex_;
  std::vector<NodeInfo> nodes_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_TABLE_H_
