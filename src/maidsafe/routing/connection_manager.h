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

#include "maidsafe/common/node_id.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/rudp/managed_connections.h"

// object to be held by routing object
struct connection_manager {
 public:
  using close_node_change = std::pair<std::vector<NodeId>, std::vector<NodeId>>;
  connection_manager(rudp::ManagedConnections& managed_connections) : connections_(), rudp_(rudp) {}
  connection_manager(connection_manager const&) = delete;
  connection_manager(connection_manager&&) = delete;
  ~connection_manager() = default;
  connection_manager& operator=(connection_manager const&) = delete;
  connection_manager& operator=(connection_manager&&) = delete;

  bool suggest_node(NodeId node_to_add);
  // always return close group even if no change
  std::vector<node_info> lost_network_connection(NodeId connection_id);
  // always return close group even if no change
  std::vector<NodeInoid> add_node(NodeId node_to_remove);


 private:
  std::vector<std::vector<NodeId>> connections_;
  rudp::ManagedConnections rudp_;
};


#endif  // MAIDSAFE_ROUTING_CONNECTION_MANAGER_
