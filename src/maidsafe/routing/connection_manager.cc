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
#include "maidsafe/routing/async_exchange.h"

namespace maidsafe {

namespace routing {

using std::weak_ptr;
using std::make_shared;
using std::move;
using boost::none_t;
using boost::optional;

ConnectionManager::ConnectionManager(boost::asio::io_service& ios, PublicPmid our_fob)
    : io_service_(ios),
      our_fob_(std::move(our_fob)),
      our_id_(our_fob_.name()->string()),
      peers_(Comparison(our_id_)),
      current_close_group_(),
      destroy_indicator_(new boost::none_t()) {}

bool ConnectionManager::IsManaged(const Address& node_id) const {
  return peers_.find(node_id) != peers_.end();
  // return routing_table_.CheckNode(node_to_add);
}

std::set<Address, ConnectionManager::Comparison> ConnectionManager::GetTarget(
    const Address& target_node) const {
  // TODO(PeterJ): The previous code was quite more complicated, so recheck correctness of this one.
  return std::set<Address, Comparison>(Comparison(target_node));
  // for (const auto& peer : peers_) {
  //  result.insert(peer.first);
  //}
  // return result;
  // auto nodes(routing_table_.TargetNodes(target_node));
  ////nodes.erase(std::remove_if(std::begin(nodes), std::end(nodes),
  ////                           [](NodeInfo& node) { return !node.connected(); }),
  ////            std::end(nodes));
  // return nodes;
}

// boost::optional<CloseGroupDifference> ConnectionManager::LostNetworkConnection(
//    const Address& node) {
//  routing_table_.DropNode(node);
//  return GroupChanged();
//}

optional<CloseGroupDifference> ConnectionManager::DropNode(const Address& their_id) {
  // routing_table_.DropNode(their_id);
  peers_.erase(their_id);
  return GroupChanged();
}

// acceptor_(io_service_, crux::endpoint(boost::asio::ip::udp::v4(), 5483)),
void ConnectionManager::StartAccepting(unsigned short port) {
  auto acceptor_i = acceptors_.find(port);

  if (acceptor_i == acceptors_.end()) {
    crux::endpoint endpoint(boost::asio::ip::udp::v4(), port);
    auto acceptor = std::unique_ptr<crux::acceptor>(new crux::acceptor(io_service_, endpoint));
    auto pair = acceptors_.insert(std::make_pair(port, std::move(acceptor)));
    acceptor_i = pair.first;
  }

  auto socket = make_shared<crux::socket>(io_service_);

  auto& acceptor = acceptor_i->second;

  weak_ptr<none_t> destroy_guard = destroy_indicator_;

  acceptor->async_accept(*socket, [=](boost::system::error_code error) {
    if (!destroy_guard.lock())
      return;

    if (error) {
      if (error == boost::asio::error::operation_aborted) {
        return;
      }
    }

    StartAccepting(port);

    AsyncExchange(*socket, Serialise(our_fob_),
                  [=](boost::system::error_code error, SerialisedMessage data) {
      if (!destroy_guard.lock())
        return;

      if (error)
        return;

      PublicPmid their_public_pmid(Parse<PublicPmid>(std::move(data)));
      Address their_id(their_public_pmid.name()->string());
      InsertPeer(PeerNode(NodeInfo(std::move(their_id), std::move(their_public_pmid), true),
                          std::move(socket)));
    });
  });
}

void ConnectionManager::AddNode(optional<NodeInfo> assumed_node_info, EndpointPair eps) {
  static const crux::endpoint unspecified_ep(boost::asio::ip::udp::v4(), 0);

  // TODO(PeterJ): Try the internal endpoint as well
  auto endpoint = convert::ToBoost(eps.external);

  auto pair_i = being_connected_.find(endpoint);

  if (pair_i == being_connected_.end()) {
    bool inserted = false;
    auto socket = make_shared<crux::socket>(io_service_, unspecified_ep);
    std::tie(pair_i, inserted) = being_connected_.insert(std::make_pair(endpoint, socket));
  }

  auto socket = pair_i->second;
  weak_ptr<crux::socket> weak_socket = socket;

  socket->async_connect(convert::ToBoost(eps.external), [=](boost::system::error_code error) {
    auto socket = weak_socket.lock();

    if (!socket)
      return;

    if (error) {
      being_connected_.erase(endpoint);
      return;
    }

    AsyncExchange(*socket, Serialise(our_fob_),
                  [=](boost::system::error_code error, SerialisedMessage data) {
      auto socket = weak_socket.lock();

      if (!socket)
        return;

      being_connected_.erase(endpoint);

      if (error)
        return;

      PublicPmid their_public_pmid(Parse<PublicPmid>(std::move(data)));
      Address their_id(their_public_pmid.name()->string());
      NodeInfo their_node_info(std::move(their_id), std::move(their_public_pmid), true);

      if (assumed_node_info && *assumed_node_info != their_node_info)
        return;

      InsertPeer(PeerNode(std::move(their_node_info), std::move(socket)));
    });
  });
}

void ConnectionManager::InsertPeer(PeerNode&& node_arg) {
  const auto& id = node_arg.id();
  const auto pair = peers_.insert(std::make_pair(id, std::move(node_arg)));

  if (!pair.second /* = inserted */) {
    return;
  }

  auto& node = pair.first->second;

  StartReceiving(node);

  if (on_connection_added_) {
    on_connection_added_(node.id());
  }
}

void ConnectionManager::StartReceiving(PeerNode& node) {
  auto node_guard = node.DestroyGuard();

  node.Receive([=, &node](asio::error_code error, const SerialisedMessage& bytes) {
    if (!node_guard.lock())
      return;
    if (error)
      return;
    if (!on_receive_)
      return;
    // Complex handler invocation to be safe in cases where the
    // handler destroys this object or in case where the handler
    // invocation resets the handler to something else.
    auto h = move(on_receive_);
    h(node.id(), bytes);
    if (!node_guard.lock())
      return;
    if (!on_receive_) {
      on_receive_ = move(h);
    }
    StartReceiving(node);
  });
}

// bool ConnectionManager::CloseGroupMember(const Address& their_id) {
//  auto close_group(routing_table_.OurCloseGroup());
//  return std::any_of(std::begin(close_group), std::end(close_group),
//                     [&their_id](const NodeInfo& node) { return node.id == their_id; });
//}

optional<CloseGroupDifference> ConnectionManager::GroupChanged() {
  auto new_group(OurCloseGroup());
  std::vector<Address> new_group_ids;
  for (const auto& group_member_public_pmid : new_group)
    new_group_ids.push_back(Address(group_member_public_pmid.name()->string()));

  if (new_group_ids != current_close_group_) {
    auto changed = std::make_pair(new_group_ids, current_close_group_);
    current_close_group_ = new_group_ids;
    return changed;
  }

  return boost::none;
}

}  // namespace routing

}  // namespace maidsafe
