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


bool connection_manager::suggest_node(NodeId node_to_add) {
  return routing_table_.check_node(node_to_add);
}

std::vector<node_info> connection_manager::get_target(NodeId target_node) {
  auto targets(routing_table_.get_target_nodes(target_node));
  std::vector<endpoint> endpoints;
  if (targets.size() > 1) {
    // gather endpoints
    for (auto& target : targets) {
      if (!target.their_endpoint.address.is_unspecified())
        endpoints.push_back(target.their_endpoint);
    }
    // set unconnected nodes with a random endpoint
    for (auto& target : targets) {
      if (target.their_endpoint.address.is_unspecified()) {
        std::random_shuffle(std::begin(endpoints, std::end(endpoints)));
        target.their_endpoint = std::front(endpoints);
      }
    }
  }
  return targets;
}

group_change connection_manager::lost_network_connection(endpoint their_endpoint) {
  routing_table_.drop_node(their_endpoint);
  return group_changed();
}

group_change connection_manager::drop_node(NodeId their_id) {
  routing_table_.drop_node(their_id);
  return group_changed();
}


group_change connection_manager::add_node(node_info node_to_add, endpoint their_endpoint,
                                          rudp::NatType nat_type) {
  // do not try and add a non close node that is symmetric if we are also symmetric
  if (!(routing_table_.size() > group_size && nat_type == rudp::NatType::kSymmetric &&
        our_nat_type_ == rudp::NatType::kSymmetric &&
        routing_table_.target_nodes(node_to_add.id).size() > 1)) {
    // get an rudp connection if possible
    // FIXME - stub implementation
    node_to_add.their_endpoint = their_endpoint;
    routing_table_.add_node(node_to_add);
  }

  return group_changed();
}

group_change connection_manager::group_changed() {
  auto new_group(routing_table_.our_close_group());
  group_change changes;
  if (new_group == current_close_group_) {
    changes = {false, {new_group, current_close_group_}};
  } else {
    changes = {true, {new_group, current_close_group_}};
    current_close_group_ = new_group;
  }
  return changes;
}


}  // namespace routing

}  // namespace maidsafe
