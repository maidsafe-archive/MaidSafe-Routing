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

#include "maidsafe/routing/connection_manager.h"

#include <vector>
#include <mutex>
#include <algorithm>
#include <utility>
#include "maidsafe/common/node_id.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/contact.h"

namespace maidsafe {
namespace routing {

bool ConnectionManager::suggest_node_to_add(const NodeId& node_to_add) {
  return routing_table_.check_node(node_to_add);
}

std::vector<node_info> ConnectionManager::get_target(const NodeId& target_node) {
  auto targets(routing_table_.target_nodes(target_node));
  // remove any nodes we are not connected to
  targets.erase(std::remove_if(std::begin(targets), std::end(targets),
                               [](const node_info& node) { return !node.connected; }),
                std::end(targets));
  return targets;
}

void ConnectionManager::lost_network_connection(const NodeId& node) {
  routing_table_.drop_node(node);
  group_changed();
}

void ConnectionManager::drop_node(const NodeId& their_id) {
  routing_table_.drop_node(their_id);
  group_changed();
}

void ConnectionManager::add_node(node_info node_to_add, rudp::endpoint_pair their_endpoint_pair) {
  rudp::contact rudp_contact;
  rudp_contact.id = rudp::node_id(node_to_add.id);
  rudp_contact.endpoints = their_endpoint_pair;
  rudp_contact.public_key = node_to_add.public_key;


  rudp_.add(std::move(rudp_contact), [node_to_add, this](maidsafe_error error) {
    if (error.code() == make_error_code(CommonErrors::success)) {
      auto added = routing_table_.add_node(node_to_add);
      if (!added.first) {
        rudp_.remove(rudp::node_id(node_to_add.id), nullptr);  // become invalid for us
        group_changed();
      } else if (added.second) {
        rudp_.remove(rudp::node_id(added.second->id), nullptr);  // a sacrificlal node was found
        group_changed();
      }
    }
  });
}
//################### private #############################

void ConnectionManager::group_changed() {
  auto new_nodeinfo_group(routing_table_.our_close_group());
  std::vector<NodeId> new_group;
  for (auto const& nodes : new_nodeinfo_group)
    new_group.push_back(nodes.id);

  std::lock_guard<std::mutex> lock(mutex_);
  if (new_group != current_close_group_) {
    group_changed_functor_([new_group, this]() -> close_group_difference {
      return std::make_pair(new_group, current_close_group_);
    }());
    current_close_group_ = new_group;
  }
}

}  // namespace routing
}  // namespace maidsafe
