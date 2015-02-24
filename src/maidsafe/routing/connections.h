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

#ifndef MAIDSAFE_ROUTING_CONNECTIOS_H_
#define MAIDSAFE_ROUTING_CONNECTIOS_H_

namespace maidsafe {

namespace routing {

#include <functional>
#include <map>
#include <vector>

#include "asio/io_service.hpp"
#include "boost/optional.hpp"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/crux/socket.hpp"
#include "maidsafe/crux/acceptor.hpp"

namespace maidsafe {

namespace routing {

class Connections {
 private:
  using Bytes = std::vector<unsigned char>;

  struct AcceptOp {
    crux::acceptor              acceptor;
    std::function<void(NodeId)> on_accept;
  };

 public:
  template<class ReceiveHandler /* void (NodeId, Bytes) */,
           class DropHandler    /* void (NodeId) */>
  Connections(const NodeId& our_node_id, ReceiveHandler, DropHandler);

  Connections(const Connections&) = delete;
  Connections(Connections&&)      = delete;
  Connections& operator=(const Connections&) = delete;
  Connections& operator=(Connections&&)      = delete;
  ~Connections() = default;

  template<class Handler /* void (error_code, NodeId) */>
  void Connect(asio::ip::udp::endpoint, Handler);

  void Drop(const NodeId& their_id);

  template<class Handler /* void (endpoint, NodeId) */>
  void Accept(unsigned short port, const Handler);

  const Address& OurId() const { return our_id_; }

 private:
  void StartReceiving(std::shared_ptr<crux::socket>);

  boost::asio::io_service& get_io_service();

 private:
  BoostAsioService service_;

  NodeId    our_id_;

  std::function<void(NodeId, const Bytes&)> on_receive_;
  std::function<void(NodeId)>               on_drop_;

  std::map<unsigned short, std::unique_ptr<AcceptOp>>     acceptors_;
  std::map<crux::endpoint, std::shared_ptr<crux::socket>> connections_;
  std::map<NodeId, crux::endpoint>                        id_to_endpoint_map_;

  std::shared_ptr<boost::none_t> destroy_indicator_;
};

Connections::Connections(const NodeId&  our_node_id,
                         ReceiveHandler on_receive,
                         DropHandler    on_drop);
  : service_(1),
    our_id_(our_node_id),
    on_receive_(std::move(on_receive)),
    on_drop_(std::move(on_drop))
{
}

inline Connections::boost::asio::io_service& get_io_service() {
  return service_.service();
}

inline void Connections::~Connections() {
  get_io_service().post([=]() {
      acceptors_.clear();
      being_connected_.clear();
      connections_.clear();
      });

  service_.Stop();
}

template<class Handler /* void (endpoint, NodeId) */>
void Accept(unsigned short port, const Handler on_accept) {
  get_io_service().post([=]() {
    acceptors_.find(port);
  });
}

inline void StartReceiving(NodeId id,
                           std::shared_ptr<crux::socket> socket) {
  std::weak_ptr<crux::socket> weak_socket = socket;

  socket_->async_receive(boost::asio::buffer(*buffer),
                         [=](boost::system::error_code error, size_t) {
    if (!weak_socket.lock()) {
      // This object has been destroyed.
      return;
    }

    if (error) {
      return StartReceiving();
    }

    auto on_receive = std::move(on_receive_);

    on_receive(id, *buffer);

    if (!weak_socket.lock()) {
      return;
    }

    on_receive_ = std::move(on_receive);

    StartReceiving();
  });
}

}  // namespace routing

}  // namespace maidsafe

#endif // MAIDSAFE_ROUTING_CONNECTIOS_H_

