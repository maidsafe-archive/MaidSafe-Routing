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
#include "maidsafe/routing/timer.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

using std::weak_ptr;
using std::make_shared;
using std::move;
using boost::none_t;
using boost::optional;

ConnectionManager::ConnectionManager(asio::io_service& ios, Address our_id, OnReceive on_receive,
                                     OnConnectionLost on_connection_lost)
    : io_service_(ios),
      mutex_(),
      our_accept_port_(5483),
      routing_table_(our_id),
      connected_non_routing_nodes_(),
      on_receive_(std::move(on_receive)),
      on_connection_lost_(std::move(on_connection_lost)),
      current_close_group_(),
      connections_(new Connections(io_service_, our_id)) {
  StartReceiving();
  StartAccepting();
}

bool ConnectionManager::SuggestNodeToAdd(const Address& node_to_add) const {
  return routing_table_.CheckNode(node_to_add);
}

std::vector<NodeInfo> ConnectionManager::GetTarget(const Address& target_node) const {
  auto nodes(routing_table_.TargetNodes(target_node));
  nodes.erase(std::remove_if(std::begin(nodes), std::end(nodes), [](NodeInfo& node) {
    return !node.connected;
  }), std::end(nodes));
  return nodes;
}

std::set<Address> ConnectionManager::GetNonRoutingNodes() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return connected_non_routing_nodes_;
}

// boost::optional<CloseGroupDifference> ConnectionManager::LostNetworkConnection(
//    const Address& node) {
//  routing_table_.DropNode(node);
//  return GroupChanged();
// }

// optional<CloseGroupDifference> ConnectionManager::DropNode(const Address& their_id) {
//  routing_table_.DropNode(their_id);
//  // FIXME(Prakash) remove connection ?
//  return GroupChanged();
//}
void ConnectionManager::DropNode(const Address& their_id) { connections_->Drop(their_id); }

void ConnectionManager::StartAccepting() {
  std::weak_ptr<Connections> weak_connections = connections_;

  connections_->Accept(our_accept_port_, &our_accept_port_, [=](asio::error_code error,
                                                                Connections::AcceptResult result) {
    if (!weak_connections.lock()) {
      return;
    }

    auto expected_i = expected_accepts_.find(result.his_address);

    if (expected_i != expected_accepts_.end()) {
      auto expected = std::move(expected_i->second);
      expected_accepts_.erase(expected_i);

      HandleAddNode(error, std::move(expected.node_info), std::move(expected.handler));

      // The handler may have destroyed 'this'.
      if (!weak_connections.lock()) {
        return;
      }
    } else {
      if (!error) {
        connected_non_routing_nodes_.insert(result.his_address);
      }
    }

    if (error != asio::error::operation_aborted) {
      StartAccepting();
    }
  });

  LOG(kInfo) << "StartAccepting() port " << our_accept_port_;
}

void ConnectionManager::HandleAddNode(asio::error_code error, NodeInfo node_info,
                                      OnAddNode user_handler) {
  if (error && error != asio::error::timed_out) {
    return;
  }

  node_info.connected = error != asio::error::timed_out;

  user_handler(error, AddToRoutingTable(std::move(node_info)));
}

void ConnectionManager::AddNodeAccept(NodeInfo node_info, EndpointPair, OnAddNode on_node_added) {
  auto id = node_info.id;

  auto timer = std::make_shared<Timer>(io_service_);

  auto canceling_handler =
      [on_node_added, timer](asio::error_code error, boost::optional<CloseGroupDifference> diff) {
        timer->cancel();
        on_node_added(error, std::move(diff));
      };

  auto insert_result =
      expected_accepts_.insert(std::make_pair(id, ExpectedAccept{node_info, canceling_handler}));

  if (!insert_result.second) {
    return io_service_.post(
        [on_node_added]() { on_node_added(asio::error::already_started, boost::none); });
  }

  // TODO(Team): Is the timeout value correct? Should it be in defined
  // somewhere else?
  timer->async_wait(std::chrono::seconds(10), [=]() {
    expected_accepts_.erase(id);
    HandleAddNode(asio::error::timed_out, node_info, on_node_added);
  });
}

void ConnectionManager::AddNode(NodeInfo node_to_add, EndpointPair their_endpoint_pair,
                                OnAddNode on_node_added) {
  std::weak_ptr<Connections> weak_connections = connections_;

  // TODO(PeterJ): Use local endpoint as well
  connections_->Connect(their_endpoint_pair.external,
                        [=](asio::error_code error, Connections::ConnectResult result) {
                          if (!weak_connections.lock()) {
                            return on_node_added(asio::error::operation_aborted, boost::none);
                          }

                          if (!error && (result.his_address != node_to_add.id)) {
                            error = asio::error::fault;
                          }

                          HandleAddNode(error, node_to_add, on_node_added);
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

  connections_->Receive([=](asio::error_code error, Connections::ReceiveResult result) {
    if (!weak_connections.lock())
      return;
    if (error) {
      return HandleConnectionLost(result.his_address);
    }
    auto h = std::move(on_receive_);
    if (h) {
      h(std::move(result.his_address), std::move(result.message));
      if (!weak_connections.lock())
        return;
      on_receive_ = std::move(h);
    }
    StartReceiving();
  });
}

void ConnectionManager::SendToNonRoutingNode(const Address& /*addr*/,
                                             const SerialisedMessage& /*message*/) {
  // connections_->Send(addr, message, std::move(handler));  // FIXME(Prakash)
  // remove connection if failed
}


void ConnectionManager::HandleConnectionLost(Address lost_connection) {
  routing_table_.DropNode(lost_connection);
  connected_non_routing_nodes_.erase(lost_connection);
  auto h = std::move(on_connection_lost_);
  if (h) {
    h(GroupChanged(), lost_connection);
  }
}

}  // namespace routing

}  // namespace maidsafe
