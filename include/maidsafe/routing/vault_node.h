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

#include <memory>

#include "asio/io_service.hpp"
#include "boost/filesystem/path.hpp"

#include "maidsafe/passport/types.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/types.h"
#include "maidsafe/routing/bootstrap_handler.h"

namespace maidsafe {

namespace routing {

class VaultNode : private rudp::ManagedConnections::Listener {
 public:
  class Listener {
   public:
    virtual ~Listener() {}
    virtual void MessageReceived(Address id, SerialisedMessage message) = 0;
  };

  VaultNode(asio::io_service& io_service, rudp::ManagedConnections& managed_connections,
            boost::filesystem::path db_location, const passport::Pmid& pmid);
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
  class RudpListener : private rudp::ManagedConnections::Listener {
   public:
    virtual void MessageReceived(NodeId /*peer_id*/,
                                 rudp::ReceivedMessage /*message*/) override final {}
    virtual void ConnectionLost(NodeId /*peer_*/) override final {}
  };

  asio::io_service& io_service_;
  rudp::ManagedConnections& rudp_;
  BootstrapHandler bootstrap_handler_;
  const Address our_id_;
  const asymm::Keys keys_;
  std::shared_ptr<RudpListener> rudp_listener_;
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
