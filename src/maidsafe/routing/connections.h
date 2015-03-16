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
   struct AcceptResult {
     Endpoint his_endpoint;
     Address  his_address;
     Endpoint our_endpoint; // As seen by the other end
   };

   struct ConnectResult {
     Address  his_address;
     Endpoint our_endpoint; // As seen by the other end
   };

   struct ReceiveResult {
     Address his_address;
     SerialisedMessage message;
   };

 public:
  Connections(const Address& our_node_id);
  Connections() = delete;
  Connections(const Connections&) = delete;
  Connections(Connections&&) = delete;

  Connections& operator=(const Connections&) = delete;
  Connections& operator=(Connections&&) = delete;

  ~Connections();

  template <class Token>
  AsyncResultReturn<Token> Send(const Address&, const SerialisedMessage&, Token&&);

  template <class Token>
  AsyncResultReturn<Token, ReceiveResult> Receive(Token&&);

  template <class Token>
  AsyncResultReturn<Token, ConnectResult> Connect(asio::ip::udp::endpoint, Token&&);

  // The secont argument is an ugly C-style return of the actual port that has been
  // chosen. TODO: Try to return it using a proper C++ way.
  template <class Token>
  AsyncResultReturn<Token, AcceptResult>
  Accept(unsigned short port, unsigned short* chosen_port, Token&&);

  void Drop(const Address& their_id);

  void Shutdown();

  const Address& OurId() const { return our_id_; }

  std::size_t max_message_size() const { return 1048576; }

  boost::asio::io_service& get_io_service();

  std::weak_ptr<boost::none_t> Guard() { return destroy_indicator_; }

  void Wait();

 private:
  void StartReceiving(const Address&, const crux::endpoint&, const std::shared_ptr<crux::socket>&);

  Address our_id_;

  std::function<void(Address, const SerialisedMessage&)> on_receive_;
  std::function<void(Address)> on_drop_;

  std::map<unsigned short, std::shared_ptr<crux::acceptor>> acceptors_;  // NOLINT
  std::map<crux::endpoint, std::shared_ptr<crux::socket>> connections_;
  std::map<Address, crux::endpoint> id_to_endpoint_map_;

  AsyncQueue<asio::error_code, ReceiveResult> receive_queue_;

  BoostAsioService runner_;

  std::shared_ptr<boost::none_t> destroy_indicator_;
};

inline Connections::Connections(const Address& our_node_id)
    : our_id_(our_node_id), runner_(1), destroy_indicator_(new boost::none_t) {}

template <class Token>
AsyncResultReturn<Token> Connections::Send(const Address& remote_id, const SerialisedMessage& bytes,
                                           Token&& token) {
  using Handler = AsyncResultHandler<Token>;
  Handler handler(std::forward<Token>(token));
  asio::async_result<Handler> result(handler);

  get_io_service().post([=]() mutable {
    auto remote_endpoint_i = id_to_endpoint_map_.find(remote_id);

    if (remote_endpoint_i == id_to_endpoint_map_.end()) {
      LOG(kWarning) << "bad_descriptor !! " <<  remote_id;
      return handler(asio::error::bad_descriptor);
    }

    auto remote_endpoint = remote_endpoint_i->second;
    auto socket_i = connections_.find(remote_endpoint);
    assert(socket_i != connections_.end());

    auto& socket = socket_i->second;
    auto buffer = std::make_shared<SerialisedMessage>(std::move(bytes));

    std::weak_ptr<crux::socket> weak_socket = socket;

    socket->async_send(boost::asio::buffer(*buffer),
                       [=](boost::system::error_code error, std::size_t) mutable {
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
  return result.get();
}

template <typename Token>
AsyncResultReturn<Token, Connections::ReceiveResult> Connections::Receive(Token&& token) {
  using Handler = AsyncResultHandler<Token, ReceiveResult>;
  Handler handler(std::forward<Token>(token));
  asio::async_result<Handler> result(handler);

  // TODO(PeterJ): For some reason I need to wrap the handler, otherwise I get crashes
  // in the future tests.
  auto handler2 = [=](asio::error_code error, ReceiveResult result) mutable {
    handler(error, std::move(result));
  };

  get_io_service().post([=]() mutable { receive_queue_.AsyncPop(handler2); });

  return result.get();
}

inline Connections::~Connections() {
  destroy_indicator_.reset();
  Shutdown();
  runner_.Stop();
}

template <class Token>
AsyncResultReturn<Token, Connections::ConnectResult> Connections::Connect(Endpoint endpoint,
                                                                          Token&& token) {

  using Handler = AsyncResultHandler<Token, ConnectResult>;
  Handler handler(std::forward<Token>(token));
  asio::async_result<Handler> result(handler);

  get_io_service().post([=]() mutable {
    crux::endpoint unspecified_ep(boost::asio::ip::udp::v4(), 0);
    auto socket = std::make_shared<crux::socket>(get_io_service(), unspecified_ep);

    auto insert_result = connections_.insert(std::make_pair(convert::ToBoost(endpoint), socket));

    if (!insert_result.second) {
      return handler(asio::error::already_started, ConnectResult());
    }

    std::weak_ptr<crux::socket> weak_socket = socket;

    socket->async_connect(convert::ToBoost(endpoint), [=](boost::system::error_code error) mutable {
      auto socket = weak_socket.lock();

      if (!socket) {
        return handler(asio::error::operation_aborted, ConnectResult());
      }
      if (error) {
        return handler(convert::ToStd(error), ConnectResult());
      }

      auto remote_endpoint = socket->remote_endpoint();

      connections_[remote_endpoint] = socket;
      auto his_endpoint = convert::ToAsio(socket->remote_endpoint());

      AsyncExchange(*socket, Serialise(our_id_, his_endpoint),
                    [=](boost::system::error_code error, SerialisedMessage data) mutable {
                      auto socket = weak_socket.lock();

                      if (!socket) {
                        return handler(asio::error::operation_aborted, ConnectResult());
                      }

                      if (error) {
                        connections_.erase(remote_endpoint);
                        return handler(convert::ToStd(error), ConnectResult());
                      }

                      InputVectorStream stream(data);
                      Address his_id;
                      asio::ip::udp::endpoint our_endpoint;
                      Parse(stream, his_id, our_endpoint);
                      id_to_endpoint_map_[his_id] = remote_endpoint;
                      StartReceiving(his_id, remote_endpoint, socket);

                      handler(convert::ToStd(error), ConnectResult{his_id, our_endpoint});
                    });
    });
  });

  return result.get();
}

template <class Token>
AsyncResultReturn<Token, Connections::AcceptResult>
Connections::Accept(unsigned short port, unsigned short* chosen_port, Token&& token) {

  using Handler = AsyncResultHandler<Token, AcceptResult>;
  Handler handler(std::forward<Token>(token));
  asio::async_result<Handler> result(handler);


  auto loopback = [](unsigned short port) {
    return crux::endpoint(boost::asio::ip::udp::v4(), port);
  };

  // TODO(PeterJ):Make sure this operation is thread safe in crux.
  std::shared_ptr<crux::acceptor> acceptor;

  try {
    acceptor = std::make_shared<crux::acceptor>(get_io_service(), loopback(port));
  }
  catch(...) {
    acceptor = std::make_shared<crux::acceptor>(get_io_service(), loopback(0));
  }

  if (chosen_port) {
    *chosen_port = acceptor->local_endpoint().port();
  }

  get_io_service().post([=]() mutable {
    auto find_result = acceptors_.insert(std::make_pair(port, acceptor));

    if (!find_result.second /* inserted? */) {
      return handler(asio::error::already_started, Connections::AcceptResult());

    }

    std::weak_ptr<crux::acceptor> weak_acceptor = acceptor;

    auto socket = std::make_shared<crux::socket>(get_io_service());

    acceptor->async_accept(*socket, [=](boost::system::error_code error) mutable {
      if (!weak_acceptor.lock()) {
        return handler(asio::error::operation_aborted, AcceptResult());
      }

      if (error) {
        return handler(asio::error::operation_aborted, AcceptResult());
      }

      acceptors_.erase(port);
      auto remote_endpoint = socket->remote_endpoint();
      connections_[remote_endpoint] = socket;
      auto his_endpoint = convert::ToAsio(socket->remote_endpoint());

      std::weak_ptr<crux::socket> weak_socket = socket;

      AsyncExchange(*socket, Serialise(our_id_, his_endpoint), [=](boost::system::error_code error,
                                                                   SerialisedMessage data) mutable {
        auto socket = weak_socket.lock();

        if (!socket) {
          return handler(asio::error::operation_aborted,
                         AcceptResult{convert::ToAsio(remote_endpoint), Address(), Endpoint()});
       }

        if (error) {
          connections_.erase(remote_endpoint);
          return handler(convert::ToStd(error),
                         AcceptResult{convert::ToAsio(remote_endpoint), Address(), Endpoint()});
        }

        InputVectorStream stream(data);
        Address his_id;
        Endpoint our_endpoint;
        Parse(stream, his_id, our_endpoint);

        id_to_endpoint_map_[his_id] = remote_endpoint;
        StartReceiving(his_id, remote_endpoint, socket);

        handler(convert::ToStd(error),
                AcceptResult{convert::ToAsio(remote_endpoint), his_id, our_endpoint});
      });
    });
  });

  return result.get();
}

inline void Connections::StartReceiving(const Address& id, const crux::endpoint& remote_endpoint,
                                        const std::shared_ptr<crux::socket>& socket) {
  std::weak_ptr<crux::socket> weak_socket = socket;

  // TODO(PeterJ): Buffer reuse
  auto buffer = std::make_shared<SerialisedMessage>(max_message_size());

  socket->async_receive(
      boost::asio::buffer(*buffer), [=](boost::system::error_code error, size_t size) mutable {
        auto socket = weak_socket.lock();

        if (!socket) {
          return receive_queue_.Push(asio::error::operation_aborted,
                                     ReceiveResult{id, std::move(*buffer)});
        }

        if (error) {
          id_to_endpoint_map_.erase(id);
          connections_.erase(remote_endpoint);
        }

        buffer->resize(size);
        receive_queue_.Push(convert::ToStd(error), ReceiveResult{id, std::move(*buffer)});

        if (error)
          return;

        StartReceiving(id, remote_endpoint, socket);
      });
}

inline boost::asio::io_service& Connections::get_io_service() { return runner_.service(); }

inline void Connections::Shutdown() {
  get_io_service().post([=]() {
    acceptors_.clear();
    connections_.clear();
    id_to_endpoint_map_.clear();
  });
}

inline void Connections::Wait() {
  runner_.Stop();
}

inline void Connections::Drop(const Address& their_id) {
  get_io_service().post([=]() {
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
