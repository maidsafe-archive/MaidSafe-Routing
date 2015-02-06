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

#ifndef MAIDSAFE_ROUTING_PEER_NODE_H_
#define MAIDSAFE_ROUTING_PEER_NODE_H_

#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/boost_asio_conversions.h"
#include "maidsafe/crux/socket.hpp"

namespace maidsafe {
namespace routing {

class PeerNode {
 public:
  using PublicPmid = passport::PublicPmid;

  PeerNode(NodeId id, std::shared_ptr<crux::socket> socket, PublicPmid dht_fob)
    : node_info_(id, std::move(dht_fob))
    , socket(socket)
  {}

  template<class Message, class Handler> void Send(Message msg, const Handler& handler) {
    auto msg_ptr = std::make_shared<Message>(std::move(msg));
    socket->async_send(boost::asio::buffer(*msg_ptr),
                       [msg_ptr, handler](boost::system::error_code error, size_t) {
                         handler(from_boost(error));
                       });
  }

  const NodeInfo& node_info() const { return node_info_; }
  bool connected() const { return connected_; }

 private:
  NodeInfo node_info_;
  std::shared_ptr<crux::socket> socket;
  //int32_t rank;
  bool connected_;
};

}  // namespace routing
}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_PEER_NODE_H_

