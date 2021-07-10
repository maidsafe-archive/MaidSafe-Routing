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
#include <queue>
#include <vector>

#include "asio/io_service.hpp"
#include "boost/optional.hpp"

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/convert.h"
#include "maidsafe/crux/acceptor.hpp"
#include "maidsafe/crux/socket.hpp"

#include "maidsafe/routing/apply_tuple.h"
#include "maidsafe/routing/async_exchange.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

class Connections {
 public:
  struct AcceptResult {
    Endpoint his_endpoint;
    Address his_address;
    Endpoint our_endpoint;  // As seen by the other end
  };

  struct ConnectResult {
    Address his_address;
    Endpoint our_endpoint;  // As seen by the other end
  };

  struct ReceiveResult {
    Address his_address;
    SerialisedMessage message;
  };

 public:
  Connections(asio::io_service&, const Address& our_node_id);
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
  AsyncResultReturn<Token, AcceptResult> Accept(Port port, Port* chosen_port, Token&&);

  void Drop(const Address& their_id);

  void Shutdown();

  const Address& OurId() const { return our_id_; }

  std::size_t max_message_size() const { return 1048576; }

  boost::asio::io_service& get_io_service();

  void Wait();

 private:
  void StartReceiving(const Address&, const crux::endpoint&, const std::shared_ptr<crux::socket>&);
  void OnReceive(asio::error_code, ReceiveResult);

  template <class Handler, class... Args>
  void post(Handler&& handler, Args&&... args);

 private:
  asio::io_service& service;
  std::unique_ptr<asio::io_service::work> work_;

  Address our_id_;

  std::function<void(Address, const SerialisedMessage&)> on_receive_;
  std::function<void(Address)> on_drop_;

  std::map<Port, std::shared_ptr<crux::acceptor>> acceptors_;  // NOLINT
  std::map<Address, std::shared_ptr<crux::socket>> connections_;
  std::map<crux::endpoint, std::shared_ptr<crux::socket>> connecting_sockets_;

  struct ReceiveInput {
    asio::error_code error;
    ReceiveResult result;
  };

  using ReceiveOutput = std::function<void(asio::error_code, ReceiveResult)>;

  std::queue<ReceiveInput> receive_input_;
  std::queue<ReceiveOutput> receive_output_;

  BoostAsioService runner_;
};

inline Connections::Connections(asio::io_service& ios, const Address& our_node_id)
    : service(ios), work_(new asio::io_service::work(ios)), our_id_(our_node_id), runner_(1) {}

template <class Token>
AsyncResultReturn<Token> Connections::Send(const Address& remote_id, const SerialisedMessage& bytes,
                                           Token&& token) {
  using Handler = AsyncResultHandler<Token>;
  Handler handler(std::forward<Token>(token));
  asio::async_result<Handler> result(handler);

  get_io_service().post([=]() mutable {
    auto socket_i = connections_.find(remote_id);

    if (socket_i == connections_.end()) {
      LOG(kWarning) << "bad_descriptor !! " << remote_id;
      return post(handler, asio::error::bad_descriptor);
    }

    auto& socket = socket_i->second;
    auto buffer = std::make_shared<SerialisedMessage>(std::move(bytes));

    std::weak_ptr<crux::socket> weak_socket = socket;

    socket->async_send(boost::asio::buffer(*buffer),
                       [=](boost::system::error_code error, std::size_t) mutable {
                         static_cast<void>(buffer);
                         if (!weak_socket.lock()) {
                           return post(handler, asio::error::operation_aborted);
                         }
                         if (error) {
                           connections_.erase(remote_id);
                         }
                         post(handler, convert::ToStd(error));
                       });
  });
  return result.get();
}

template <typename Token>
AsyncResultReturn<Token, Connections::ReceiveResult> Connections::Receive(Token&& token) {
  using Handler = AsyncResultHandler<Token, ReceiveResult>;
  Handler handler(std::forward<Token>(token));
  asio::async_result<Handler> result(handler);

  get_io_service().post([=]() mutable {
    if (!receive_input_.empty()) {
      auto input = std::move(receive_input_.front());
      receive_input_.pop();
      post(handler, input.error, std::move(input.result));
    } else {
      receive_output_.push(std::move(handler));
    }
  });

  return result.get();
}

inline void Connections::OnReceive(asio::error_code error, ReceiveResult result) {
  if (!receive_output_.empty()) {
    auto handler = std::move(receive_output_.front());
    receive_output_.pop();
    post(handler, error, std::move(result));
  } else {
    receive_input_.push(ReceiveInput{error, std::move(result)});
  }
}

inline Connections::~Connections() {
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
    crux::endpoint unspecified_ep;

    LOG(kInfo) << OurId() << " Connections::Connect(" << endpoint << ")\n";
    if (endpoint.address().is_v4())
      unspecified_ep = crux::endpoint(boost::asio::ip::udp::v4(), 0);
    else
      unspecified_ep = crux::endpoint(boost::asio::ip::udp::v6(), 0);

    auto socket = std::make_shared<crux::socket>(get_io_service(), unspecified_ep);
    auto local_endpoint = socket->local_endpoint();

    auto insert_result = connecting_sockets_.insert(std::make_pair(local_endpoint, socket));

    assert(insert_result.second);

    std::weak_ptr<crux::socket> weak_socket = socket;

    socket->async_connect(convert::ToBoost(endpoint), [=](boost::system::error_code error) mutable {
      auto socket = weak_socket.lock();

      if (!socket) {
        return post(handler, asio::error::operation_aborted, ConnectResult());
      }

      if (error) {
        connecting_sockets_.erase(local_endpoint);
        return post(handler, convert::ToStd(error), ConnectResult());
      }

      auto remote_endpoint = socket->remote_endpoint();

      AsyncExchange(*socket, Serialise(our_id_, convert::ToAsio(remote_endpoint)),
                    [=](boost::system::error_code error, SerialisedMessage data) mutable {
                      auto socket = weak_socket.lock();

                      if (!socket) {
                        return post(handler, asio::error::operation_aborted, ConnectResult());
                      }

                      connecting_sockets_.erase(local_endpoint);

                      if (error) {
                        return post(handler, convert::ToStd(error), ConnectResult());
                      }

                      InputVectorStream stream(data);
                      Address his_id;
                      asio::ip::udp::endpoint our_endpoint;
                      Parse(stream, his_id, our_endpoint);

                      auto result = connections_.insert(std::make_pair(his_id, socket));

                      if (!result.second) {
                        return post(handler, asio::error::already_connected, ConnectResult{his_id, our_endpoint});
                      }

                      StartReceiving(his_id, remote_endpoint, socket);

                      post(handler, convert::ToStd(error), ConnectResult{his_id, our_endpoint});
                    });
    });
  });

  return result.get();
}

template <class Token>
AsyncResultReturn<Token, Connections::AcceptResult> Connections::Accept(Port port,
                                                                        Port* chosen_port,
                                                                        Token&& token) {
  using Handler = AsyncResultHandler<Token, AcceptResult>;
  Handler handler(std::forward<Token>(token));
  asio::async_result<Handler> result(handler);

  auto loopback = [](Port port) { return crux::endpoint(boost::asio::ip::udp::v4(), port); };

  std::shared_ptr<crux::acceptor> acceptor;

  try {
    acceptor = std::make_shared<crux::acceptor>(get_io_service(), loopback(port));
  } catch (...) {
    acceptor = std::make_shared<crux::acceptor>(get_io_service(), loopback(0));
  }

  if (chosen_port) {
    *chosen_port = acceptor->local_endpoint().port();
  }

  get_io_service().post([=]() mutable {
    auto find_result = acceptors_.insert(std::make_pair(port, acceptor));

    if (!find_result.second /* inserted? */) {
      return post(handler, asio::error::already_started, Connections::AcceptResult());
    }

    std::weak_ptr<crux::acceptor> weak_acceptor = acceptor;

    auto socket = std::make_shared<crux::socket>(get_io_service());

    acceptor->async_accept(*socket, [=](boost::system::error_code error) mutable {
      if (!weak_acceptor.lock()) {
        return post(handler, asio::error::operation_aborted, AcceptResult());
      }

      if (error) {
        return post(handler, asio::error::operation_aborted, AcceptResult());
      }

      acceptors_.erase(port);

      auto remote_endpoint = socket->remote_endpoint();
      auto local_endpoint  = socket->local_endpoint();

      connecting_sockets_[local_endpoint] = socket;

      std::weak_ptr<crux::socket> weak_socket = socket;

      AsyncExchange(*socket, Serialise(our_id_, convert::ToAsio(remote_endpoint)),
                    [=](boost::system::error_code error, SerialisedMessage data) mutable {
        auto socket = weak_socket.lock();

        if (!socket) {
          return post(handler, asio::error::operation_aborted,
                      AcceptResult{convert::ToAsio(remote_endpoint), Address(), Endpoint()});
        }

        if (error) {
          connecting_sockets_.erase(local_endpoint);
          return post(handler, convert::ToStd(error),
                      AcceptResult{convert::ToAsio(remote_endpoint), Address(), Endpoint()});
        }

        InputVectorStream stream(data);
        Address his_id;
        Endpoint our_endpoint;
        Parse(stream, his_id, our_endpoint);

        auto result = connections_.insert(std::make_pair(his_id, socket));

        if (result.second) {
          // Inserted
          StartReceiving(his_id, remote_endpoint, socket);
          post(handler, convert::ToStd(error),
               AcceptResult{convert::ToAsio(remote_endpoint), his_id, our_endpoint});
        }
        else {
          remote_endpoint = result.first->second->remote_endpoint();
          post(handler, asio::error::already_connected,
               AcceptResult{convert::ToAsio(remote_endpoint), his_id, our_endpoint});
        }
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
          return OnReceive(asio::error::operation_aborted, ReceiveResult{id, std::move(*buffer)});
        }

        if (error) {
          connections_.erase(id);
          size = 0;
        }

        buffer->resize(size);
        OnReceive(convert::ToStd(error), ReceiveResult{id, std::move(*buffer)});

        if (error)
          return;

        StartReceiving(id, remote_endpoint, socket);
      });
}

inline boost::asio::io_service& Connections::get_io_service() { return runner_.service(); }

inline void Connections::Shutdown() {
  get_io_service().post([=]() {
    work_.reset();
    acceptors_.clear();
    connections_.clear();
    connecting_sockets_.clear();
  });
}

inline void Connections::Wait() { runner_.Stop(); }

template <class Handler, class... Args>
void Connections::post(Handler&& handler, Args&&... args) {
  std::tuple<typename std::decay<Args>::type...> tuple(std::forward<Args>(args)...);
  service.post([handler, tuple]() mutable { ApplyTuple(handler, tuple); });
}

inline void Connections::Drop(const Address& their_id) {
  get_io_service().post([=]() {
    connections_.erase(their_id);
  });
}

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_CONNECTIONS_H_
