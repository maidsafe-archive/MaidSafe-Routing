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
    use of the MaidSafe
    Software.
 */

#ifndef MAIDSAFE_ROUTING_CONNECTION_MANAGER_
#define MAIDSAFE_ROUTING_CONNECTION_MANAGER_

#include <vector>
#include <mutex>
#include <tuple>
#include <map>
#include <vector>

#include "maidsafe/common/node_id.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/nat_type.h"

namespace maidsafe {
namespace routing {

struct connection_manager {
 public:
  connection_manager(rudp::ManagedConnections& rudp, NodeId our_id, rudp::NatType our_nat_type)
      : routing_table_(our_id), rudp_(rudp), our_nat_type_(our_nat_type), current_close_group_() {}
  connection_manager(connection_manager const&) = delete;
  connection_manager(connection_manager&&) = delete;
  ~connection_manager() = default;
  connection_manager& operator=(connection_manager const&) = delete;
  connection_manager& operator=(connection_manager&&) = delete;

  bool suggest_node(NodeId node_to_add);
  std::vector<node_info> get_target(NodeId target_node);
  // always return close group even if no change to close nodes
  group_change lost_network_connection(endpoint their_endpoint);
  // routing wishes to drop a specific node (may be a node we cannot connect to)
  group_change drop_node(NodeId their_id);
  // always return close group even if no change to close nodes
  group_change add_node(node_info node_to_add, endpoint their_endpoint, rudp::NatType nat_type);
  std::vector<node_info> our_close_group() { return routing_table_.our_close_group(); }

 private:
  group_change group_changed();
  // connections_[index].[0] == connection id
  // if connectoins_[index].size() > 1 the remaining nodes share this connection_id
  routing_table routing_table_;
  rudp::ManagedConnections& rudp_;
  rudp::NatType our_nat_type_;
  std::vector<node_info> current_close_group_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_CONNECTION_MANAGER_
