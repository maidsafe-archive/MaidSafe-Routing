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
#include <utility>
#include "maidsafe/common/node_id.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/rudp/managed_connections.h"

namespace maidsafe {
namespace routing {

bool connection_manager::suggest_node_to_add(const NodeId& node_to_add) {
  return routing_table_.check_node(node_to_add);
}

std::vector<node_info> connection_manager::get_target(const NodeId& target_node) {
  auto targets(routing_table_.target_nodes(target_node));
  // remove any nodes we are not connected to
  targets.erase(std::remove_if(std::begin(targets), std::end(targets),
                               [](const node_info& node) { return !node.connected; }),
                std::end(targets));
  return targets;
}

void connection_manager::lost_network_connection(const NodeId& node) {
  routing_table_.drop_node(node);
  group_changed();
}

void connection_manager::drop_node(const NodeId& their_id) {
  routing_table_.drop_node(their_id);
  group_changed();
}

void connection_manager::add_node(node_info node_to_add,
                                          rudp::EndpointPair their_endpoint_pair) {
  rudp::Contact contact;
  contact.node_id = node_to_add.id;
  contact.endpoint_pair = endpoint_pair;
  contact.public_key = node_to_add.public_key;


  rudp_.Add(std::move(contact), [node_to_add, this](const NodeId& node, int result) {
    if (result == rudp::kSucess)
      bool added = routing_table_.add_node(node_to_add);
    if (!added.first) {
      rudp::drop_connection(node);
    } else if (added.second) {
      rudp.drop_connection(added->second.id);
    }
   group_changed();
  });
}
//################### private #############################

void connection_manager::group_changed() {
  auto new_group(routing_table_.our_close_group());
  std::vector<NodeId> empty_group;
  group_change changes;
  std::lock_guard<sd::mutex> lock(mutex_);
  if (new_group != current_close_group_) {
    group_changed_functor_(std::make_pair(new_group, current_close_group_));
    current_close_group_ = new_group;
  }
}

}  // namespace routing
}  // namespace maidsafe
