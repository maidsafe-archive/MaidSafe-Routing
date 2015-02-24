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

#include <functional>
#include <map>
#include <vector>

#include "asio/io_service.hpp"
#include "boost/optional.hpp"

#include "maidsafe/common/convert.h"
#include "maidsafe/common/asio_service.h"

#include "async_queue.h"

#include "maidsafe/crux/socket.hpp"
#include "maidsafe/crux/acceptor.hpp"

namespace maidsafe {

namespace routing {

class Connections {
 private:
  using Bytes = std::vector<unsigned char>;

 public:
  Connections(boost::asio::io_service&, const NodeId& our_node_id);

  Connections(const Connections&) = delete;
  Connections(Connections&&)      = delete;

  Connections& operator=(const Connections&) = delete;
  Connections& operator=(Connections&&)      = delete;

  ~Connections();

  template<class Handler /* void (error_code) */>
  void Send(const NodeId&, Handler);

  template<class Handler /* void (error_code, NodeId, const Bytes&) */>
  void Receive(Handler);

  template<class Handler /* void (error_code, NodeId) */>
  void Connect(asio::ip::udp::endpoint, Handler);

  template<class Handler /* void (endpoint, NodeId) */>
  void Accept(unsigned short port, const Handler);

  void Drop(const NodeId& their_id);

  void Shutdown();

  const NodeId& OurId() const { return our_id_; }

  std::size_t max_message_size() { return 1048576; }

  boost::asio::io_service& get_io_service();


 private:
  void StartReceiving(const NodeId&,
                      const crux::endpoint&,
                      const std::shared_ptr<crux::socket>&);

 private:
  boost::asio::io_service& service_;

  NodeId our_id_;

  std::function<void(NodeId, const Bytes&)> on_receive_;
  std::function<void(NodeId)>               on_drop_;

  std::map<unsigned short, std::shared_ptr<crux::acceptor>> acceptors_;
  std::map<crux::endpoint, std::shared_ptr<crux::socket>>   connections_;
  std::map<NodeId, crux::endpoint>                          id_to_endpoint_map_;

  async_queue<asio::error_code, NodeId, Bytes> receive_queue_;
};

inline Connections::Connections(boost::asio::io_service& ios, const NodeId&  our_node_id)
  : service_(ios),
    our_id_(our_node_id)
{
}

template <typename Handler /* void (error_code, NodeId, Bytes) */>
void Connections::Receive(Handler handler) {
  service_.post([=]() {
    receive_queue_.async_pop(std::move(handler));
  });
}

inline Connections::~Connections() {
  Shutdown();
}

template<class Handler /* void (error_code, NodeId) */>
void Connections::Connect(asio::ip::udp::endpoint endpoint, Handler handler) {
  service_.post([=]() {
    crux::endpoint unspecified_ep(boost::asio::ip::udp::v4(), 0);
    auto socket = std::make_shared<crux::socket>(service_, unspecified_ep);
    std::weak_ptr<crux::socket> weak_socket = socket;

    socket->async_connect(convert::ToBoost(endpoint), [=](boost::system::error_code error) {
      if (!weak_socket.lock()) {
        return handler(asio::error::operation_aborted, NodeId());
      }
      if (error) {
        return handler(convert::ToStd(error), NodeId());
      }
      connections_[socket->remote_endpoint()] = socket;
      NodeId his_id; // TODO: Get this id from him
      handler(convert::ToStd(error), his_id);
      });
  });
}

template<class Handler /* void (error_code, endpoint, NodeId) */>
void Connections::Accept(unsigned short port, const Handler on_accept) {
  service_.post([=]() {
    auto find_result = acceptors_.insert(std::make_pair(port, std::shared_ptr<crux::acceptor>()));

    auto& acceptor = find_result.first->second;

    if (!acceptor) {
      crux::endpoint endpoint(boost::asio::ip::udp::v4(), port);
      acceptor.reset(new crux::acceptor(service_, endpoint));
    }

    std::weak_ptr<crux::acceptor> weak_acceptor = acceptor;

    auto socket = std::make_shared<crux::socket>(service_);

    acceptor->async_accept(*socket, [=](boost::system::error_code error) {
      if (!weak_acceptor.lock()) {
        return on_accept(asio::error::operation_aborted,
                         asio::ip::udp::endpoint(),
                         NodeId());
      }

      if (error) {
        return on_accept(asio::error::operation_aborted,
                         asio::ip::udp::endpoint(),
                         NodeId());
      }

      acceptors_.erase(port);
      connections_[socket->remote_endpoint()] = socket;

      NodeId id; // TODO: Exchange this id with him.

      StartReceiving(id, socket->remote_endpoint(), socket);

      on_accept(asio::error_code(),
                convert::ToAsio(socket->remote_endpoint()),
                NodeId() /* TODO */);
    });
  });
}

inline void Connections::StartReceiving(const NodeId& id,
                                        const crux::endpoint& remote_endpoint,
                                        const std::shared_ptr<crux::socket>& socket) {
  std::weak_ptr<crux::socket> weak_socket = socket;

  // TODO(PeterJ): Buffer reuse
  auto buffer = std::make_shared<Bytes>(max_message_size());

  socket->async_receive(boost::asio::buffer(*buffer),
                        [=](boost::system::error_code error, size_t) {
    auto socket = weak_socket.lock();

    if (!socket) {
      return receive_queue_.push(asio::error::operation_aborted, id, std::move(*buffer));
    }

    if (error) {
      id_to_endpoint_map_.erase(id);
      connections_.erase(remote_endpoint);
    }

    receive_queue_.push(convert::ToStd(error), id, std::move(*buffer));

    if (error) return;

    StartReceiving(id, remote_endpoint, socket);
  });
}

inline boost::asio::io_service& Connections::get_io_service() {
  return service_;
}

inline void Connections::Shutdown() {
  service_.post([=]() {
      acceptors_.clear();
      connections_.clear();
      id_to_endpoint_map_.clear();
      });
}

}  // namespace routing

}  // namespace maidsafe

#endif // MAIDSAFE_ROUTING_CONNECTIOS_H_

