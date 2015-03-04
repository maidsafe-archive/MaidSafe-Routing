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
#include <string>
#include <utility>
#include <vector>

#include "asio/use_future.hpp"
#include "boost/asio/spawn.hpp"

#include "maidsafe/common/convert.h"

#include "maidsafe/routing/peer_node.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

using std::weak_ptr;
using std::make_shared;
using std::move;
using boost::none_t;
using boost::optional;

ConnectionManager::ConnectionManager(Address our_id, OnReceive on_receive)
    : mutex_(),
      boost_io_service_(1),
      routing_table_(our_id),
      on_receive_(std::move(on_receive)),
      current_close_group_(),
      connections_(new Connections(boost_io_service_.service(), our_id)) {
  StartReceiving();
}

bool ConnectionManager::SuggestNodeToAdd(const Address& node_to_add) const {
  return routing_table_.CheckNode(node_to_add);
}

std::vector<NodeInfo> ConnectionManager::GetTarget(const Address& target_node) const {
  auto nodes(routing_table_.TargetNodes(target_node));
  nodes.erase(std::remove_if(std::begin(nodes), std::end(nodes),
                             [](NodeInfo& node) { return !node.connected; }),
              std::end(nodes));
  return nodes;
}

boost::optional<CloseGroupDifference> ConnectionManager::LostNetworkConnection(
    const Address& node) {
  routing_table_.DropNode(node);
  return GroupChanged();
}

optional<CloseGroupDifference> ConnectionManager::DropNode(const Address& their_id) {
  routing_table_.DropNode(their_id);
  // FIXME(Prakash) remove connection ?
  return GroupChanged();
}

void ConnectionManager::AddNode(
    NodeInfo node_to_add, EndpointPair their_endpoint_pair, OnAddNode on_node_added) {

  std::weak_ptr<Connections> weak_connections = connections_;

  // TODO(PeterJ): Use local endpoint as well
  connections_->Connect(their_endpoint_pair.external,
                        [=](asio::error_code error, Address addr, Endpoint) {
    if (!weak_connections.lock()) {
      return;
    }

    if (error || (addr != node_to_add.id)) {
      return;
    }

    on_node_added(AddToRoutingTable(node_to_add));
  });
}

template<class Handler> void StartAccepting(std::weak_ptr<Connections> weak_connections,
                                            NodeInfo node_to_add, EndpointPair node_eps,
                                            Handler handler) {
  auto connections = weak_connections.lock();

  if (!connections) {
    return;
  }

  connections->Accept(6378,
    [=](asio::error_code error, asio::ip::udp::endpoint, Address addr, Endpoint /*our_endpoint*/) {
    if (error) {
      return;
    }

    if (node_to_add.id != addr) {
      // Restart
      StartAccepting(weak_connections, std::move(node_to_add), std::move(node_eps),
                     std::move(handler));
      return;
    }

    handler();
  });
}

void ConnectionManager::AddNodeAccept(NodeInfo node_to_add, EndpointPair their_endpoint_pair,
                                      OnAddNode on_node_added) {

  StartAccepting(connections_, node_to_add, their_endpoint_pair, [=]() {
    on_node_added(AddToRoutingTable(node_to_add));
  });
}

boost::optional<CloseGroupDifference> ConnectionManager::AddToRoutingTable(NodeInfo node_to_add) {
  auto added = routing_table_.AddNode(node_to_add);

  if (!added.first) {
    connections_->Drop(node_to_add.id);
  } else if (added.second) {
    connections_->Drop(node_to_add.id);
  }

  // FIXME: It is incorrect to assume the GroupChanged will reflect changes made
  // by the previous Drop command because that command will execute its business
  // in a separate thread (Same in the AddNodeAccept function and others).
  return GroupChanged();
}

bool ConnectionManager::CloseGroupMember(const Address& their_id) {
  auto close_group(routing_table_.OurCloseGroup());
  return std::any_of(std::begin(close_group), std::end(close_group),
                     [&their_id](const NodeInfo& node) { return node.id == their_id; });
}

boost::optional<CloseGroupDifference> ConnectionManager::GroupChanged() {
  auto new_nodeinfo_group(routing_table_.OurCloseGroup());
  std::vector<Address> new_group;
  for (const auto& nodes : new_nodeinfo_group)
    new_group.push_back(nodes.id);
  std::lock_guard<std::mutex> lock(mutex_);
  if (new_group != current_close_group_) {
    auto changed = std::make_pair(new_group, current_close_group_);
    current_close_group_ = new_group;
    return changed;
  }
  return boost::none;
}

void ConnectionManager::StartReceiving() {
  std::weak_ptr<Connections> weak_connections = connections_;

  connections_->Receive([=](asio::error_code error, Address address,
                            const SerialisedMessage& message) {
    if (!weak_connections.lock()) return;
    auto h = std::move(on_receive_);
    h(error, std::move(address), std::move(message));
    if (!weak_connections.lock()) return;
    on_receive_ = std::move(h);
    StartReceiving();
  });
}

}  // namespace routing

}  // namespace maidsafe
