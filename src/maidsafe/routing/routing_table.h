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

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/rsa.h"

#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

class routing_table {
 public:
  static size_t bucket_size() { return 1; }
  static size_t parallelism() { return 4; }
  static size_t optimal_size() { return 64; }
  explicit routing_table(NodeId our_id);
  routing_table(const routing_table&) = delete;
  routing_table(routing_table&&) = delete;
  routing_table& operator=(const routing_table&) = delete;
  routing_table& operator=(routing_table&&) MAIDSAFE_NOEXCEPT = delete;
  ~routing_table() = default;
  std::pair<bool, boost::optional<node_info>> add_node(node_info their_info);
  bool check_node(const NodeId& their_id) const;
  void drop_node(const NodeId& node_to_drop);
  // our close group or at least as much of it as we currently know
  std::vector<node_info> our_close_group() const;
  // If more than 1 node returned then we are in close group so send to all
  std::vector<node_info> target_nodes(const NodeId& their_id) const;
  NodeId our_id() const { return our_id_; }
  size_t size() const;

 private:
  class comparison {
   public:
    explicit comparison(NodeId our_id) : our_id_(std::move(our_id)) {}
    bool operator()(const node_info& lhs, const node_info& rhs) const {
      return NodeId::CloserToTarget(lhs.id, rhs.id, our_id_);
    }
   private:
    const NodeId our_id_;
  };
  int32_t bucket_index(const NodeId& node_id) const;
  bool have_node(const node_info& their_info) const;
  bool new_node_is_better_than_existing(
      const NodeId& their_id, std::vector<node_info>::const_iterator removal_candidate) const;
  void push_back_then_sort(node_info&& their_info);
  std::vector<node_info>::const_iterator find_candidate_for_removal() const;
  unsigned int network_status(size_t size) const;

  const NodeId our_id_;
  const comparison comparison_;
  mutable std::mutex mutex_;
  std::vector<node_info> nodes_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_TABLE_H_
