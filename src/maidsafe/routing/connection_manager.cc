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
    Software.                                                                 */

#include "maidsafe/routing/connection_manager.h"

#include <vector>
#include <mutex>
#include <algorithm>
#include "maidsafe/common/node_id.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/rudp/managed_connections.h"


namespace maidsafe {
namespace routing {


bool connection_manager::suggest_node(NodeId node_to_add) {
  return routing_table_.check_node(node_to_add);
}

// always return close group even if no change
std::vector<node_info> connection_manager::lost_network_connection(NodeId connection_id) {
  auto found = connections_.find(connection_id);

  if (found != std::end(connections_)) {
    for (const auto& node : found->second)
      routing_table_.drop_node(node);
    connections_.erase(found);
  }

  return routing_table_.our_close_group();
}

// always return close group even if no change
std::vector<node_info> connection_manager::add_node(node_info node_to_add, NodeId connection_id) {
  auto added(routing_table_.add_node(node_to_add));
  if (added.first) {  // node was added
    auto found = connections_.find(connection_id);
    if (found != std::end(connections_)) {
      found->second.push_back({node_to_add.id});
    } else {
      connections_.insert({connection_id, {node_to_add.id}});
    }
  }

  if (added.second)  // this means that we removed a connection to add this one
    for (auto& connection : connections_) {
      auto nodes = std::find_if(
          std::begin(connection.second), std::end(connection.second),
          [&added](const NodeId& node_found) { return added.second->id == node_found; });
      if (nodes != std::end(connection.second))
        connection.second.erase(nodes);
    }

  for (auto i = std::begin(connections_); i != std::end(connections_); ++i) {
    if (i->second.empty())
      rudp_.Remove(i->first);
    connections_.erase(i);
  }


  return routing_table_.our_close_group();
}

}  // namespace routing

}  // namespace maidsafe
