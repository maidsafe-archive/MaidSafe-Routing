/*  Copyright 2015 MaidSafe.net limited

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

#include <memory>

#include "maidsafe/common/convert.h"
#include "maidsafe/crux/socket.hpp"
#include "maidsafe/passport/types.h"

#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

class PeerNode {
 public:
  PeerNode(const PeerNode&) = delete;
  PeerNode& operator=(const PeerNode&) = delete;

  PeerNode(PeerNode&& other)
      : node_info_(std::move(other.node_info_)),
        receive_buffer_(std::move(other.receive_buffer_)),
        socket_(std::move(other.socket_)),
        destroy_indicator_(std::move(other.destroy_indicator_)) {}

  PeerNode& operator=(PeerNode&& other) {
    node_info_ = std::move(other.node_info_);
    receive_buffer_ = std::move(other.receive_buffer_);
    socket_ = std::move(other.socket_);
    destroy_indicator_ = std::move(other.destroy_indicator_);
    return *this;
  }

  PeerNode(NodeInfo node_info, std::shared_ptr<crux::socket> socket)
      : node_info_(std::move(node_info)),
        receive_buffer_(std::make_shared<SerialisedMessage>(MaxMessageSize())),
        socket_(std::move(socket)),
        destroy_indicator_(new boost::none_t) {}

  template <typename Message, typename Handler>
  void Send(Message msg, const Handler& handler) {
    auto msg_ptr = std::make_shared<Message>(std::move(msg));
    auto guard = DestroyGuard();

    socket_->async_send(boost::asio::buffer(*msg_ptr),
                        [this, msg_ptr, handler, guard](boost::system::error_code error, size_t) {
      if (!guard.lock()) {
        // This object was destroyed.
        return handler(asio::error::operation_aborted);
      }

      if (error) {
        // TODO(team) - drop connection
        node_info_.connected = false;
      }

      handler(convert::ToStd(error));
    });
  }

  template <typename Handler>
  void Receive(const Handler& handler) {
    auto guard = DestroyGuard();

    // Make a shared copy to make sure the buffer is valid
    // even if this object is destroyed.
    auto buffer = receive_buffer_;

    assert(buffer);
    socket_->async_receive(boost::asio::buffer(*buffer),
                           [guard, buffer, handler](boost::system::error_code error, size_t) {
      if (!guard.lock()) {
        // This object was destroyed.
        return handler(asio::error::operation_aborted, *buffer);
      }

      if (error) {
        return handler(convert::ToStd(error), *buffer);
      }

      handler(convert::ToStd(error), *buffer);
    });
  }

  const Address& id() const { return node_info_.id; }
  const NodeInfo& node_info() const { return node_info_; }

  std::weak_ptr<boost::none_t> DestroyGuard() { return destroy_indicator_; }

  // TODO(Team): This should be in some global scope config file or something.
  static size_t MaxMessageSize() { return 1048576; }

 private:
  NodeInfo node_info_;
  std::shared_ptr<SerialisedMessage> receive_buffer_;
  std::shared_ptr<crux::socket> socket_;  // TODO(Team): ditch shared_ptr
  std::shared_ptr<boost::none_t> destroy_indicator_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_PEER_NODE_H_
