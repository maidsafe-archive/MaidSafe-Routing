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

#ifndef MAIDSAFE_ROUTING_CONNECTIONS_H_
#define MAIDSAFE_ROUTING_CONNECTIONS_H_

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "asio/io_service.hpp"
#include "boost/optional.hpp"

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/convert.h"
#include "maidsafe/crux/acceptor.hpp"
#include "maidsafe/crux/socket.hpp"

#include "maidsafe/routing/async_queue.h"
#include "maidsafe/routing/async_exchange.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

class Connections {
 public:
  Connections(boost::asio::io_service&, const Address& our_node_id);

  Connections() = delete;
  Connections(const Connections&) = delete;
  Connections(Connections&&) = delete;

  Connections& operator=(const Connections&) = delete;
  Connections& operator=(Connections&&) = delete;

  ~Connections();

  template <class Handler /* void (error_code) */>
  void Send(const Address&, const SerialisedMessage&, Handler);

  template <class Handler /* void (error_code, Address, const SerialisedMessage&) */>
  void Receive(Handler);

  template <class Handler /* void (error_code, Address, Endpoint our_endpoint) */>
  void Connect(asio::ip::udp::endpoint, Handler);

  template <class Handler /* void (error_code, endpoint, Address, Endpoint our_endpoint) */>
  void Accept(unsigned short port, const Handler);

  void Drop(const Address& their_id);

  void Shutdown();

  const Address& OurId() const { return our_id_; }

  std::size_t max_message_size() const { return 1048576; }

  boost::asio::io_service& get_io_service();

 private:
  void StartReceiving(const Address&, const crux::endpoint&, const std::shared_ptr<crux::socket>&);

  boost::asio::io_service& service_;

  Address our_id_;

  std::function<void(Address, const SerialisedMessage&)> on_receive_;
  std::function<void(Address)> on_drop_;

  std::map<unsigned short, std::shared_ptr<crux::acceptor>> acceptors_;  // NOLINT
  std::map<crux::endpoint, std::shared_ptr<crux::socket>> connections_;
  std::map<Address, crux::endpoint> id_to_endpoint_map_;

  AsyncQueue<asio::error_code, Address, SerialisedMessage> receive_queue_;
};

inline Connections::Connections(boost::asio::io_service& ios, const Address& our_node_id)
    : service_(ios), our_id_(our_node_id) {}

template <class Handler /* void (error_code) */>
void Connections::Send(const Address& remote_id, const SerialisedMessage& bytes, Handler handler) {
  service_.post([=]() {
    auto remote_endpoint_i = id_to_endpoint_map_.find(remote_id);

    if (remote_endpoint_i == id_to_endpoint_map_.end()) {
      return handler(asio::error::bad_descriptor);
    }

    auto remote_endpoint = remote_endpoint_i->second;
    auto socket_i = connections_.find(remote_endpoint);
    assert(socket_i != connections_.end());

    auto& socket = socket_i->second;
    auto buffer = std::make_shared<SerialisedMessage>(std::move(bytes));

    std::weak_ptr<crux::socket> weak_socket = socket;

    socket->async_send(boost::asio::buffer(*buffer),
                       [=](boost::system::error_code error, std::size_t) {
                         static_cast<void>(buffer);

                         if (!weak_socket.lock()) {
                           return handler(asio::error::operation_aborted);
                         }
                         if (error) {
                           id_to_endpoint_map_.erase(remote_id);
                           connections_.erase(remote_endpoint);
                         }
                         handler(convert::ToStd(error));
                       });
  });
}

template <typename Handler /* void (error_code, Address, SerialisedMessage) */>
void Connections::Receive(Handler handler) {
  service_.post([=]() { receive_queue_.AsyncPop(std::move(handler)); });
}

inline Connections::~Connections() { Shutdown(); }

template <class Handler /* void (error_code, Address, asio::ip::udp::endpoint) */>
void Connections::Connect(asio::ip::udp::endpoint endpoint, Handler handler) {
  using asio_endpoint = asio::ip::udp::endpoint;

  service_.post([=]() {
    crux::endpoint unspecified_ep(boost::asio::ip::udp::v4(), 0);
    auto socket = std::make_shared<crux::socket>(service_, unspecified_ep);

    auto insert_result = connections_.insert(std::make_pair(convert::ToBoost(endpoint), socket));

    if (!insert_result.second) {
      return handler(asio::error::already_started, Address(), asio_endpoint());
    }

    std::weak_ptr<crux::socket> weak_socket = socket;

    socket->async_connect(convert::ToBoost(endpoint), [=](boost::system::error_code error) {
      auto socket = weak_socket.lock();

      if (!socket) {
        return handler(asio::error::operation_aborted, Address(), asio_endpoint());
      }
      if (error) {
        return handler(convert::ToStd(error), Address(), asio_endpoint());
      }

      auto remote_endpoint = socket->remote_endpoint();

      connections_[remote_endpoint] = socket;
      auto his_endpoint = convert::ToAsio(socket->remote_endpoint());

      AsyncExchange(*socket, Serialise(our_id_, his_endpoint),
                    [=](boost::system::error_code error, SerialisedMessage data) {
                      auto socket = weak_socket.lock();

                      if (!socket) {
                        return handler(asio::error::operation_aborted, Address(), asio_endpoint());
                      }

                      if (error) {
                        connections_.erase(remote_endpoint);
                        return handler(convert::ToStd(error), Address(), asio_endpoint());
                      }

                      InputVectorStream stream(data);
                      Address his_id;
                      asio::ip::udp::endpoint our_endpoint;
                      Parse(stream, his_id, our_endpoint);

                      id_to_endpoint_map_[his_id] = remote_endpoint;
                      StartReceiving(his_id, remote_endpoint, socket);

                      handler(convert::ToStd(error), his_id, our_endpoint);
                    });
    });
  });
}

template <class Handler /* void (error_code, endpoint, Address) */>
void Connections::Accept(unsigned short port, const Handler handler) {
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
        return handler(asio::error::operation_aborted, Endpoint(), Address(), Endpoint());
      }

      if (error) {
        return handler(asio::error::operation_aborted, Endpoint(), Address(), Endpoint());
      }

      acceptors_.erase(port);
      auto remote_endpoint = socket->remote_endpoint();
      connections_[remote_endpoint] = socket;
      auto his_endpoint = convert::ToAsio(socket->remote_endpoint());

      std::weak_ptr<crux::socket> weak_socket = socket;

      AsyncExchange(*socket, Serialise(our_id_, his_endpoint), [=](boost::system::error_code error,
                                                                   SerialisedMessage data) {
        auto socket = weak_socket.lock();

        if (!socket) {
          return handler(asio::error::operation_aborted, convert::ToAsio(remote_endpoint),
                         Address(), Endpoint());
        }

        if (error) {
          connections_.erase(remote_endpoint);
          return handler(convert::ToStd(error), convert::ToAsio(remote_endpoint), Address(),
                         Endpoint());
        }

        InputVectorStream stream(data);
        Address his_id;
        Endpoint our_endpoint;
        Parse(stream, his_id, our_endpoint);

        id_to_endpoint_map_[his_id] = remote_endpoint;
        StartReceiving(his_id, remote_endpoint, socket);

        handler(convert::ToStd(error), convert::ToAsio(remote_endpoint), his_id, our_endpoint);
      });
    });
  });
}

inline void Connections::StartReceiving(const Address& id, const crux::endpoint& remote_endpoint,
                                        const std::shared_ptr<crux::socket>& socket) {
  std::weak_ptr<crux::socket> weak_socket = socket;

  // TODO(PeterJ): Buffer reuse
  auto buffer = std::make_shared<SerialisedMessage>(max_message_size());

  socket->async_receive(
      boost::asio::buffer(*buffer), [=](boost::system::error_code error, size_t size) {
        auto socket = weak_socket.lock();

        if (!socket) {
          return receive_queue_.Push(asio::error::operation_aborted, id, std::move(*buffer));
        }

        if (error) {
          id_to_endpoint_map_.erase(id);
          connections_.erase(remote_endpoint);
        }

        buffer->resize(size);
        receive_queue_.Push(convert::ToStd(error), id, std::move(*buffer));

        if (error)
          return;

        StartReceiving(id, remote_endpoint, socket);
      });
}

inline boost::asio::io_service& Connections::get_io_service() { return service_; }

inline void Connections::Shutdown() {
  service_.post([=]() {
    acceptors_.clear();
    connections_.clear();
    id_to_endpoint_map_.clear();
  });
}

inline void Connections::Drop(const Address& their_id) {
  service_.post([=]() {
    // TODO: Migth it be that it is in connections_ but not in the id_to_endpoint_map_?
    // I.e. that above layers would wan't to remove by ID nodes which were not
    // yet connected?
    auto i = id_to_endpoint_map_.find(their_id);

    if (i == id_to_endpoint_map_.end()) {
      return;
    }

    connections_.erase(i->second);
  });
}

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_CONNECTIONS_H_
