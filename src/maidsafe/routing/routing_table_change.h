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

#ifndef MAIDSAFE_ROUTING_ROUTING_TABLE_CHANGE_H_
#define MAIDSAFE_ROUTING_ROUTING_TABLE_CHANGE_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "maidsafe/common/node_id.h"

#include "maidsafe/routing/types.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

struct RoutingTableChange {
  struct Remove {
    Remove() : node(), routing_only_removal(true) {}
    Remove(NodeInfo& node_in, bool routing_only_removal_in)
        : node(node_in), routing_only_removal(routing_only_removal_in) {}
    NodeInfo node;
    bool routing_only_removal;
  };
  RoutingTableChange()
      : added_node(), removed(), insertion(false), close_nodes_change(), health(0) {}
  RoutingTableChange(const NodeInfo& added_node_in, const Remove& removed_in, bool insertion_in,
                     std::shared_ptr<CloseNodesChange> close_nodes_change_in,
                     unsigned int health_in)
      : added_node(added_node_in),
        removed(removed_in),
        insertion(insertion_in),
        close_nodes_change(close_nodes_change_in),
        health(health_in) {}
  NodeInfo added_node;
  Remove removed;
  bool insertion;
  std::shared_ptr<CloseNodesChange> close_nodes_change;
  unsigned int health;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_TABLE_CHANGE_H_
