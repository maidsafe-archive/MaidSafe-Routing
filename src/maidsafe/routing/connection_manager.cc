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

#include <algorithm>
#include <mutex>
#include <utility>
#include <vector>

#include "asio/spawn.hpp"
#include "asio/use_future.hpp"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/contact.h"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/node_info.h"

namespace maidsafe {

namespace routing {

bool ConnectionManager::SuggestNodeToAdd(const Address& node_to_add) const {
  return routing_table_.CheckNode(node_to_add);
}

void ConnectionManager::LostNetworkConnection(const Address& node) {
  routing_table_.DropNode(node);
  GroupChanged();
}

void ConnectionManager::DropNode(const Address& their_id) {
  routing_table_.DropNode(their_id);
  GroupChanged();
}

void ConnectionManager::AddNode(NodeInfo node_to_add, rudp::EndpointPair their_endpoint_pair) {
  rudp::Contact rudp_contact(node_to_add.id, std::move(their_endpoint_pair),
                             node_to_add.public_key);
  asio::spawn(io_service_, [=](asio::yield_context yield) {
    asio::error_code error;
    rudp_.Add(std::move(rudp_contact), yield[error]);

    if (!error) {
      auto added = routing_table_.AddNode(node_to_add);
      if (!added.first) {
        rudp_.Remove(node_to_add.id, asio::use_future);  // become invalid for us
        GroupChanged();
      } else if (added.second) {
        rudp_.Remove(added.second->id, asio::use_future);  // a sacrificlal node was found
        GroupChanged();
      }
    }
  });
}

void ConnectionManager::GroupChanged() {
  auto new_nodeinfo_group(routing_table_.OurCloseGroup());
  std::vector<Address> new_group;
  for (const auto& nodes : new_nodeinfo_group)
    new_group.push_back(nodes.id);

  std::lock_guard<std::mutex> lock(mutex_);
  if (new_group != current_close_group_) {
    group_changed_functor_([new_group, this]() -> CloseGroupDifference {
      return std::make_pair(new_group, current_close_group_);
    }());
    current_close_group_ = new_group;
  }
}

}  // namespace routing

}  // namespace maidsafe
