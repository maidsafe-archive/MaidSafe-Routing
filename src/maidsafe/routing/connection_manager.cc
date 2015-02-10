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

#include "boost/asio/spawn.hpp"
#include "asio/use_future.hpp"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/peer_node.h"
#include "maidsafe/routing/boost_asio_conversions.h"

namespace maidsafe {

namespace routing {

bool ConnectionManager::IsManaged(const Address& node_id) const {
  return peers_.find(node_id) != peers_.end();
  //return routing_table_.CheckNode(node_to_add);
}

std::set<Address, ConnectionManager::Comparison> ConnectionManager::GetTarget(const Address& target_node) const {
  // TODO(PeterJ): The previous code was quite more complicated, so recheck correctness of this one.
  auto result = std::set<Address, Comparison>(Comparison(target_node));
  for (const auto& peer : peers_) {
    result.insert(peer.first);
  }
  return result;
  //auto nodes(routing_table_.TargetNodes(target_node));
  ////nodes.erase(std::remove_if(std::begin(nodes), std::end(nodes),
  ////                           [](NodeInfo& node) { return !node.connected(); }),
  ////            std::end(nodes));
  //return nodes;
}

//boost::optional<CloseGroupDifference> ConnectionManager::LostNetworkConnection(
//    const Address& node) {
//  routing_table_.DropNode(node);
//  return GroupChanged();
//}

boost::optional<CloseGroupDifference> ConnectionManager::DropNode(const Address& their_id) {
  //routing_table_.DropNode(their_id);
  peers_.erase(their_id);
  return GroupChanged();
}

void ConnectionManager::StartAccepting() {
  auto socket = std::make_shared<crux::socket>(io_service_);
  acceptor_.async_accept(*socket, [this, socket](boost::system::error_code error) {
      if (error) {
        if (error == boost::asio::error::operation_aborted) {
          return;
        }
        return StartAccepting();
      }
      // TODO: Exchange node info and add the socket to the peers_ map.
      StartAccepting();
      });
}

boost::optional<CloseGroupDifference> ConnectionManager::AddNode(NodeInfo node_info, EndpointPair eps) {
  boost::asio::spawn(io_service_, [=](boost::asio::yield_context yield) {
    boost::system::error_code error;
    static const crux::endpoint unspecified_ep(boost::asio::ip::udp::v4(), 0);
    auto socket = std::make_shared<crux::socket>(io_service_, unspecified_ep);

    // TODO(PeterJ): Try both endpoints, choose the first one that connects.
    socket->async_connect(to_boost(eps.external), yield[error]);

    if (!error) {
      peers_.insert(std::make_pair(node_info.id, PeerNode(node_info, socket)));
    }
  });
  // FIXME: The above stuff happens inside io_service, the GroupChanged() function
  // always returns 'no change' in here. The result should be returned
  // in form of an argument to callback.
  return GroupChanged();
}

//bool ConnectionManager::CloseGroupMember(const Address& their_id) {
//  auto close_group(routing_table_.OurCloseGroup());
//  return std::any_of(std::begin(close_group), std::end(close_group),
//                     [&their_id](const NodeInfo& node) { return node.id == their_id; });
//}

boost::optional<CloseGroupDifference> ConnectionManager::GroupChanged() {
  auto new_nodeinfo_group(OurCloseGroup());
  std::vector<Address> new_group;
  for (const auto& nodes : new_nodeinfo_group)
    new_group.push_back(nodes.id);

  if (new_group != current_close_group_) {
    auto changed = std::make_pair(new_group, current_close_group_);
    current_close_group_ = new_group;
    return changed;
  }
  return boost::none;
}

}  // namespace routing

}  // namespace maidsafe
