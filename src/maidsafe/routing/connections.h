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

#include "maidsafe/crux/socket.hpp"
#include "maidsafe/crux/acceptor.hpp"

namespace maidsafe {

namespace routing {

class Connections {
 private:
  using Bytes = std::vector<unsigned char>;

 public:
  template<class ReceiveHandler /* void (NodeId, Bytes) */,
           class DropHandler    /* void (NodeId) */>
  Connections(const NodeId& our_node_id, ReceiveHandler, DropHandler);

  Connections(const Connections&) = delete;
  Connections(Connections&&)      = delete;
  Connections& operator=(const Connections&) = delete;
  Connections& operator=(Connections&&)      = delete;
  ~Connections();

  template<class Handler /* void (error_code, NodeId) */>
  void Connect(asio::ip::udp::endpoint, Handler);

  void Drop(const NodeId& their_id);

  template<class Handler /* void (endpoint, NodeId) */>
  void Accept(unsigned short port, const Handler);

  const NodeId& OurId() const { return our_id_; }

  std::size_t max_message_size() { return 1048576; }

 private:
  void StartReceiving(const NodeId&,
                      const std::shared_ptr<Bytes>&,
                      const std::shared_ptr<crux::socket>&);

  boost::asio::io_service& get_io_service();

 private:
  BoostAsioService service_;

  NodeId    our_id_;

  std::function<void(NodeId, const Bytes&)> on_receive_;
  std::function<void(NodeId)>               on_drop_;

  std::map<unsigned short, std::shared_ptr<crux::acceptor>> acceptors_;
  std::map<crux::endpoint, std::shared_ptr<crux::socket>>   connections_;
  std::map<NodeId, crux::endpoint>                          id_to_endpoint_map_;

  std::shared_ptr<boost::none_t> destroy_indicator_;
};

template<class ReceiveHandler /* void (NodeId, Bytes) */,
         class DropHandler    /* void (NodeId) */>
Connections::Connections(const NodeId&  our_node_id,
                         ReceiveHandler on_receive,
                         DropHandler    on_drop)
  : service_(1),
    our_id_(our_node_id),
    on_receive_(std::move(on_receive)),
    on_drop_(std::move(on_drop))
{
}

inline boost::asio::io_service& Connections::get_io_service() {
  return service_.service();
}

inline Connections::~Connections() {
  get_io_service().post([=]() {
      // TODO(PeterJ): Uncommenting this causes crash in the TwoConnections test
      //acceptors_.clear();
      //connections_.clear();
      //id_to_endpoint_map_.clear();
      });

  service_.Stop();
}

template<class Handler /* void (error_code, NodeId) */>
void Connections::Connect(asio::ip::udp::endpoint endpoint, Handler handler) {
  get_io_service().post([=]() {
    crux::endpoint unspecified_ep(boost::asio::ip::udp::v4(), 0);
    auto socket = std::make_shared<crux::socket>(get_io_service(), unspecified_ep);
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
  get_io_service().post([=]() {
    auto find_result = acceptors_.insert(std::make_pair(port, std::shared_ptr<crux::acceptor>()));

    auto& acceptor = find_result.first->second;

    if (!acceptor) {
      crux::endpoint endpoint(boost::asio::ip::udp::v4(), port);
      acceptor.reset(new crux::acceptor(get_io_service(), endpoint));
    }

    std::weak_ptr<crux::acceptor> weak_acceptor = acceptor;

    auto socket = std::make_shared<crux::socket>(get_io_service());

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

      StartReceiving(id, std::make_shared<Bytes>(max_message_size()), socket);

      on_accept(asio::error_code(),
                convert::ToAsio(socket->remote_endpoint()),
                NodeId() /* TODO */);
    });
  });
}

inline void Connections::StartReceiving(const NodeId& id,
                                        const std::shared_ptr<Bytes>& buffer,
                                        const std::shared_ptr<crux::socket>& socket) {
  std::weak_ptr<crux::socket> weak_socket = socket;

  socket->async_receive(boost::asio::buffer(*buffer),
                        [=](boost::system::error_code error, size_t) {
    if (!weak_socket.lock()) {
      // This object has been destroyed.
      return;
    }

    if (error) {
      return StartReceiving(id, buffer, socket);
    }

    auto on_receive = std::move(on_receive_);

    on_receive(id, *buffer);

    if (!weak_socket.lock()) {
      return;
    }

    on_receive_ = std::move(on_receive);

    StartReceiving(id, buffer, socket);
  });
}

}  // namespace routing

}  // namespace maidsafe

#endif // MAIDSAFE_ROUTING_CONNECTIOS_H_

