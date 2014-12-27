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

#ifndef MAIDSAFE_ROUTING_VAULT_NODE_H_
#define MAIDSAFE_ROUTING_VAULT_NODE_H_

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

class VaultNode : std::enable_shared_from_this<VaultNode> {
 public:  // key      value
  using Filter = LruCache<std::pair<SourceAddress, MessageId>, void>;

 public:
  class Listener {
   public:
    virtual ~Listener() {}
    virtual bool Post(const MessageHeader&, const SerialisedMessage&) { return false; }
    virtual boost::expected<SerialisedMessage, CommonErrors> Get(Identity) {
      return boost::make_unexpected(CommonErrors::no_such_element);
    }
    virtual bool Put(const MessageHeader&, const SerialisedMessage&) { return true; }
    virtual void CloseGroupDifference(CloseGroupDifference) {}
  };

  VaultNode(asio::io_service& io_service, boost::filesystem::path db_location,
            const passport::Pmid& pmid, std::shared_ptr<Listener> listen_ptr);
  VaultNode(const VaultNode&) = delete;
  VaultNode(VaultNode&&) = delete;
  VaultNode& operator=(const VaultNode&) = delete;
  VaultNode& operator=(VaultNode&&) = delete;
  ~VaultNode();

  // normal bootstrap mechanism
  template <typename CompletionToken>
  BootstrapReturn<CompletionToken> Bootstrap(CompletionToken token);
  // used where we wish to pass a specific node to bootstrap from
  template <typename CompletionToken>
  BootstrapReturn<CompletionToken> Bootstrap(Endpoint endpoint, CompletionToken token);

  template <typename CompletionToken>
  GetReturn<CompletionToken> Get(Address key, CompletionToken token);

  template <typename CompletionToken>
  PutReturn<CompletionToken> Put(Address key, SerialisedMessage message, CompletionToken token);

  template <typename CompletionToken>
  PostReturn<CompletionToken> Post(Address key, SerialisedMessage message, CompletionToken token);

  Address OurId() const { return our_id_; }

 private:
  std::shared_ptr<VaultNode> node_ptr_;
  class RudpListener : public rudp::ManagedConnections::Listener,
                       public std::enable_shared_from_this<RudpListener> {
   public:
    RudpListener(std::shared_ptr<VaultNode> node_ptr_) : node_ptr_(node_ptr_) {}
    virtual void MessageReceived(NodeId /*peer_id*/,
                                 rudp::ReceivedMessage /*message*/) override final;
    virtual void ConnectionLost(NodeId peer) override final;

   private:
    std::shared_ptr<VaultNode> node_ptr_;
  };

  class MessageHandlerListener : public MessageHandler::Listener,
                                 public std::enable_shared_from_this<MessageHandlerListener> {
   public:
    virtual void GetDataResponseReceived(SerialisedData /*data*/) override final {}
    virtual void PutDataResponseReceived(Address /*data_name*/,
                                         maidsafe_error /*result*/) override final {}
    virtual void PostReceived(Address /*data_name*/, SerialisedData /*data*/) override final {}
  };

  void OnMessageReceived(rudp::ReceivedMessage&& serialised_message, NodeId peer_id);
  void OnCloseGroupChanged(CloseGroupDifference close_group_difference);

  asio::io_service& io_service_;
  const Address our_id_;
  const asymm::Keys keys_;
  rudp::ManagedConnections rudp_;
  BootstrapHandler bootstrap_handler_;
  ConnectionManager connection_manager_;
  std::shared_ptr<RudpListener> rudp_listener_;
  std::shared_ptr<MessageHandlerListener> message_handler_listener_;
  std::weak_ptr<Listener> listener_ptr_;
  Listener listener_;
  MessageHandler message_handler_;
  Filter filter_;
};

template <typename CompletionToken>
BootstrapReturn<CompletionToken> VaultNode::Bootstrap(CompletionToken token) {
  auto handler(std::forward<decltype(token)>(token));
  auto result(handler);
  io_service_.post([=] {
    rudp_.Bootstrap(bootstrap_handler_.ReadBootstrapContacts(), rudp_listener_, our_id_, keys_,
                    handler);
  });
  return result.get();
}

template <typename CompletionToken>
BootstrapReturn<CompletionToken> VaultNode::Bootstrap(Endpoint local_endpoint,
                                                      CompletionToken token) {
  auto handler(std::forward<decltype(token)>(token));
  auto result(handler);
  io_service_.post([=] {
    rudp_.Bootstrap(bootstrap_handler_.ReadBootstrapContacts(), rudp_listener_, our_id_, keys_,
                    handler, local_endpoint);
  });
  return result.get();
}

template <typename CompletionToken>
GetReturn<CompletionToken> VaultNode::Get(Address key, CompletionToken token) {
  auto handler(std::forward<decltype(token)>(token));
  auto result(handler);
  io_service_.post([=] { DoGet(key, handler); });
  return result.get();
}

template <typename CompletionToken>
PutReturn<CompletionToken> VaultNode::Put(Address key, SerialisedMessage message,
                                          CompletionToken token) {
  auto handler(std::forward<decltype(token)>(token));
  auto result(handler);
  io_service_.post([=] { DoPut(key, message, handler); });
  return result.get();
}

template <typename CompletionToken>
PostReturn<CompletionToken> VaultNode::Post(Address key, SerialisedMessage message,
                                            CompletionToken token) {
  auto handler(std::forward<decltype(token)>(token));
  auto result(handler);
  io_service_.post([=] { DoPost(key, message, handler); });
  return result.get();
}

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_VAULT_NODE_H_
