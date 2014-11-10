/*  Copyright 2012 MaidSafe.net limited

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
#include <vector>

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/rsa.h"

#include "maidsafe/routing/routing_table_change.h"

namespace maidsafe {

namespace routing {

struct NodeInfo;

class routing_table {
 public:
  static const size_t kBucketSize_ = 1;
  routing_table(const NodeId& our_id, const asymm::Keys& keys);
  routing_table(const routing_table&) = default;
  routing_table(routing_table&&) = default;
  routing_table& operator=(const routing_table&) = delete;
  routing_table& operator=(routing_table&&) MAIDSAFE_NOEXCEPT = delete;
  virtual ~routing_table() = default;
  bool add_node(node_info their_info);
  bool check_node(node_info their_info);
  bool drop_node(const NodeId& node_to_drop);
  // If more than 1 node returned then we are in close group so send to all !!
  std::vector<node_info> target_nodes(const NodeId& their_id) const;
  // our close group or at least as much of it as we currently know
  std::vector<node_info> our_close_group() const;

  size_t size() const;
  NodeId our_id() const { return our_id_; }
  asymm::PrivateKey our_private_key() const { return kKeys_.private_key; }
  asymm::PublicKey our_public_key() const { return kKeys_.public_key; }

 private:
  int32_t bucket_index(const NodeId& node_id) const;

  /** Attempts to find or allocate space for an incomming connect request, returning true
   * indicates approval
   * returns true if routing table is not full, otherwise, performs the following process to
   * possibly evict an existing node:
   * - sorts the nodes according to their distance from self-node-id
   * - a candidate for eviction must have an index > Parameters::unidirectional_interest_range
   * - count the number of nodes in each bucket for nodes with
   *    index > Parameters::unidirectional_interest_range
   * - choose the furthest node among the nodes with maximum bucket index
   * - in case more than one bucket have similar maximum bucket size, the furthest node in higher
   *    bucket will be evicted
   * - remove the selected node and return true **/
  std::vector<node_info>::reverse_iterator is_node_viable_for_routing_table();

  unsigned int network_status(size_t size) const;

  const NodeId our_id_;
  const asymm::Keys kKeys_;
  mutable std::mutex mutex_;
  std::vector<node_info> nodes_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_TABLE_H_
