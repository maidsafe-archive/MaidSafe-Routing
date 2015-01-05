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

#ifndef MAIDSAFE_ROUTING_ROUTING_NODE_H_
#define MAIDSAFE_ROUTING_ROUTING_NODE_H_

#include <chrono>
#include <memory>
#include <utility>

#include "asio/io_service.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/expected/expected.hpp"

#include "maidsafe/common/types.h"
#include "maidsafe/common/containers/lru_cache.h"
#include "maidsafe/passport/types.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/types.h"

#include "maidsafe/routing/bootstrap_handler.h"
#include "maidsafe/routing/connection_manager.h"
#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

class RoutingNode : public std::enable_shared_from_this<RoutingNode>,
                    public rudp::ManagedConnections::Listener {
 public:  // key      value
  using Filter = LruCache<std::pair<SourceAddress, MessageId>, void>;

 public:
  class Listener {
   public:
    virtual ~Listener() {}
    // default no post allowed unless implemented in upper layers
    virtual bool Post(const MessageHeader&, const SerialisedMessage&) { return false; }
    template <typename DataName>
    virtual boost::expected<typename DataName::data_type, CommonErrors> Get(const DataName&) {
      return boost::make_unexpected(CommonErrors::no_such_element);
    }
    virtual boost::expected<typename DataName::data_type, CommonErrors> GetKey(const DataName& data) {
      if (cache_.Get(data))
        return data;
      else
      return boost::make_unexpected(CommonErrors::no_such_element);
    }
    // default no request allowed unless implemented in upper layers
    virtual boost::expected<SerialisedMessage, CommonErrors> Request(MessageHeader,
                                                                     SerialisedMessage) {
      return boost::make_unexpected(CommonErrors::no_such_element);
    }
    // default put is allowed unless prevented by upper layers
    template <typename Data>
    virtual bool Put(Data) { return true; }
    virtual void CloseGroupDifference(CloseGroupDifference) {}
  };

  RoutingNode(asio::io_service& io_service, boost::filesystem::path db_location,
            const passport::Pmid& pmid, std::shared_ptr<Listener> listen_ptr);
  RoutingNode(const RoutingNode&) = delete;
  RoutingNode(RoutingNode&&) = delete;
  RoutingNode& operator=(const RoutingNode&) = delete;
  RoutingNode& operator=(RoutingNode&&) = delete;
  ~RoutingNode();

  // normal bootstrap mechanism
  template <typename CompletionToken>
  BootstrapReturn<CompletionToken> Bootstrap(CompletionToken token);
  // used where we wish to pass a specific node to bootstrap from
  template <typename CompletionToken>
  BootstrapReturn<CompletionToken> Bootstrap(Endpoint endpoint, CompletionToken token);
  // will return with the data
  template <typename CompletionToken>
  GetReturn<CompletionToken> Get(DataKey data_key, CompletionToken token);
  // will return with allowed or not (error_code only)
  template <typename CompletionToken>
  PutReturn<CompletionToken> Put(Address key, SerialisedMessage message, CompletionToken token);
  // will return with allowed or not (error_code only)
  template <typename CompletionToken>
  PostReturn<CompletionToken> Post(Address key, SerialisedMessage message, CompletionToken token);
  // will return with response message
  template <typename CompletionToken>
  RequestReturn<CompletionToken> Request(Address key, SerialisedMessage message,
                                         CompletionToken token);

  Address OurId() const { return our_id_; }

 private:
  void GetDataResponseReceived(GetData get_data);
  void PutDataResponseReceived(PutData put_data);
  void ResponseReceived(Response response);

  virtual void MessageReceived(NodeId peer_id,
                               rudp::ReceivedMessage serialised_message) override final;
  virtual void ConnectionLost(NodeId peer) override final;
  void OnCloseGroupChanged(CloseGroupDifference close_group_difference);

  asio::io_service& io_service_;
  Address our_id_;
  asymm::Keys keys_;
  rudp::ManagedConnections rudp_;
  BootstrapHandler bootstrap_handler_;
  ConnectionManager connection_manager_;
  std::shared_ptr<Listener> listener_ptr_;
  MessageHandler message_handler_;
  Filter filter_;
  Accumulator<Identity, SerialisedMessage> accumulator_;
};

template <typename CompletionToken>
BootstrapReturn<CompletionToken> RoutingNode::Bootstrap(CompletionToken token) {
  auto handler(std::forward<decltype(token)>(token));
  auto result(handler);
  io_service_.post([=] {
    rudp_.Bootstrap(bootstrap_handler_.ReadBootstrapContacts(), shared_from_this(), our_id_, keys_,
                    handler);
  });
  return result.get();
}

template <typename CompletionToken>
BootstrapReturn<CompletionToken> RoutingNode::Bootstrap(Endpoint local_endpoint,
                                                      CompletionToken token) {
  auto handler(std::forward<decltype(token)>(token));
  auto result(handler);
  io_service_.post([=] {
    rudp_.Bootstrap(bootstrap_handler_.ReadBootstrapContacts(), shared_from_this(), our_id_, keys_,
                    handler, local_endpoint);
  });
  return result.get();
}

template <typename CompletionToken>
GetReturn<CompletionToken> RoutingNode::Get(DataKey data_key, CompletionToken token) {
  auto handler(std::forward<decltype(token)>(token));
  auto result(handler);
  io_service_.post([=] { DoGet(data_key, handler); });
  return result.get();
}

template <typename CompletionToken>
PutReturn<CompletionToken> RoutingNode::Put(Address key, SerialisedMessage message,
                                          CompletionToken token) {
  auto handler(std::forward<decltype(token)>(token));
  auto result(handler);
  io_service_.post([=] { DoPut(key, message, handler); });
  return result.get();
}

template <typename CompletionToken>
PostReturn<CompletionToken> RoutingNode::Post(Address key, SerialisedMessage message,
                                            CompletionToken token) {
  auto handler(std::forward<decltype(token)>(token));
  auto result(handler);
  io_service_.post([=] { DoPost(key, message, handler); });
  return result.get();
}

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_NODE_H_
