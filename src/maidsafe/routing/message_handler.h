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

#ifndef MAIDSAFE_ROUTING_MESSAGE_HANDLER_H_
#define MAIDSAFE_ROUTING_MESSAGE_HANDLER_H_

#include <chrono>
#include <memory>
#include <utility>

#include "asio/io_service.hpp"

#include "maidsafe/common/types.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/containers/lru_cache.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/types.h"
#include "maidsafe/routing/accumulator.h"
#include "maidsafe/routing/messages/messages_fwd.h"
#include "maidsafe/routing/message_header.h"

namespace maidsafe {

namespace routing {

class ConnectionManager;

class MessageHandler {
 public:
  MessageHandler(asio::io_service& io_service, rudp::ManagedConnections& managed_connections,
                 ConnectionManager& connection_manager, asymm::Keys& keys);
  MessageHandler() = delete;
  ~MessageHandler() = default;
  MessageHandler(const MessageHandler&) = delete;
  MessageHandler(MessageHandler&&) = delete;
  MessageHandler& operator=(const MessageHandler&) = delete;
  MessageHandler& operator=(MessageHandler&&) = delete;

  // Send ourAddress, targetAddress and endpointpair to our close group
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
  void HandleMessage(Post post);

 private:
  SourceAddress OurSourceAddress() const;

  asio::io_service& io_service_;
  rudp::ManagedConnections& rudp_;
  ConnectionManager& connection_manager_;
  LruCache<Identity, SerialisedMessage> cache_;
  Accumulator<Identity, SerialisedMessage> accumulator_;
  asymm::Keys keys_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGE_HANDLER_H_
