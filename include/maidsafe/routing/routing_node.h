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
#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/sentinel.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

class RoutingNode : public std::enable_shared_from_this<RoutingNode>,
                    public rudp::ManagedConnections::Listener {
 public:
  using PutToCache = bool;
  // The purpose of this object is to allow this API to allow upper layers to override default
  // behavour of the calls to a routing node where applicable.
  class Listener {
   public:
    explicit Listener(LruCache<Identity, SerialisedMessage>& cache) : cache_(cache) {}
    virtual ~Listener() {}
    // default no post allowed unless implemented in upper layers
    virtual bool HandlePost(const SerialisedMessage&) { return false; }
    // not in local cache do upper layers have it (called when we are in target group)
    virtual boost::expected<DataValue, maidsafe_error> HandleGet(DataKey) {
      return boost::make_unexpected(MakeError(CommonErrors::no_such_element));
    }
    virtual boost::expected<std::vector<byte>, maidsafe_error> HandleGetKey(DataKey data) {
      auto cache_data = cache_.Get(data);
      if (cache_data)
        return cache_data;
      else  // actually in this case get the close group keys and send back
        return boost::make_unexpected(MakeError(CommonErrors::no_such_element));
    }
    virtual boost::expected<std::vector<byte>, maidsafe_error> HandleGetGroupKey(DataKey data) {
      auto cache_data = cache_.Get(data);
      if (cache_data)
        return cache_data;
      else  // actually in this case get the close group keys and send back
        return boost::make_unexpected(MakeError(CommonErrors::no_such_element));
    }
    // default put is allowed unless prevented by upper layers
    virtual PutToCache HandlePut(DataKey, DataValue) { return true; }
    // if the implementation allows any put of data in unauthenticated mode
    virtual bool HandleUnauthenticatedPut(DataKey, DataValue) { return true; }
    virtual void HandleCloseGroupDifference(CloseGroupDifference) {}

   private:
    LruCache<Identity, SerialisedMessage>& cache_;
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

  Address OurId() const { return our_id_; }

 private:
  std::vector<MessageHeader> CreateHeaders(Address target, asymm::Signature signature,
                                           MessageId message_id);
  std::vector<MessageHeader> CreateHeaders(Address target, Checksum checksum, MessageId message_id);
  std::vector<MessageHeader> CreateHeaders(Address target, MessageId message_id);
  void HandleMessage(Connect connect, MessageId message_id);
  // recieve connect from node and add nodes publicKey clients request to targetAddress
  void HandleMessage(ClientConnect client_connect, const MessageHeader& header);
  // like connect but add targets endpoint
  void HandleMessage(ConnectResponse connect_response);
  // like clientconnect adding targets public key (recieved by targets close group) (recieveing
  // needs a Quorum)

  //  void HandleMessage(ClientConnectResponse client_connect_response);
  // sent by routing nodes to a network Address
  void HandleMessage(FindGroup find_group);
  // each member of the group close to network Address fills in their node_info and replies
  void HandleMessage(FindGroupResponse find_group_reponse);
  // may be directly sent to a network Address
  void HandleMessage(GetData get_data);
  // Each node wiht the data sends it back to the originator
  void HandleMessage(GetDataResponse get_data_response);
  // sent by a client to store data, client does information dispersal and sends a part to each of
  // its close group
  void HandleMessage(PutData put_data);
  void HandleMessage(PutDataResponse put_data);
  // each member of a group needs to send this to the network address (recieveing needs a Quorum)
  // filling in public key again.
  // each member of a group needs to send this to the network Address (recieveing needs a Quorum)
  // filling in public key again.
  // void HandleMessage(Post post);
  bool TryCache(MessageTypeTag tag, MessageHeader header, Address data_key);
  virtual void MessageReceived(NodeId peer_id,
                               rudp::ReceivedMessage serialised_message) override final;
  virtual void ConnectionLost(NodeId peer) override final;
  void OnCloseGroupChanged(CloseGroupDifference close_group_difference);
  SourceAddress OurSourceAddress() const;
  using unique_identifier = std::pair<Address, uint32_t>;
  asio::io_service& io_service_;
  Address our_id_;
  std::atomic<unsigned long> message_id_{RandomUint32()};
  asymm::Keys keys_;
  rudp::ManagedConnections rudp_;
  BootstrapHandler bootstrap_handler_;
  ConnectionManager connection_manager_;
  std::shared_ptr<Listener> listener_ptr_;
  LruCache<unique_identifier, void> filter_;
  Sentinel sentinel_;
  LruCache<Address, SerialisedMessage> cache_;
  std::map<MessageId, std::function<void(SerialisedMessage)>> responder_;
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
  io_service_.post([=] {
    for (const auto& header : CreateHeaders(Address(data_key->string()), ++message_id_)) {
      rudp_.Send(Address(data_key), Serialise(header, MessageToTag<GetData>::value(), data_key),
                 handler);
    }
  });
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
