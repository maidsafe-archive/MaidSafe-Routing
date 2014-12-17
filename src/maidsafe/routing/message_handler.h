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

#include <utility>
#include <chrono>
#include "asio/io_service.hpp"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/messages.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/accumulator.h"
#include "maidsafe/common/types.h"
#include "maidsafe/common/containers/lru_cache.h"


namespace maidsafe {

namespace routing {

class ConnectionManager;
struct Ping;
struct PingResponse;
struct FindGroup;
struct FindGroupResponse;
struct Connect;
struct ConnectResponse;
struct GetData;
struct PutData;
struct Post;

class MessageHandler {

 public:
  MessageHandler(asio::io_service& io_service, rudp::ManagedConnections& managed_connections,
                 ConnectionManager& connection_manager);
  MessageHandler() = delete;
  ~MessageHandler() = default;
  MessageHandler(const MessageHandler&) = delete;
  MessageHandler(MessageHandler&&) = delete;
  MessageHandler& operator=(const MessageHandler&) = delete;
  MessageHandler& operator=(MessageHandler&&) = delete;
  void OnMessageReceived(rudp::ReceivedMessage&& serialised_message);

 private:
  void HandleMessage(Ping&& ping);
  void HandleMessage(PingResponse&& ping_response);
  void HandleMessage(FindGroup&& find_group);
  void HandleMessage(FindGroupResponse&& find_group_reponse);
  void HandleMessage(Connect&& connect);
  void HandleMessage(ConnectResponse&& connect_response);
  void HandleMessage(GetData&& get_data);
  void HandleMessage(PutData&& put_data);
  void HandleMessage(Post&& post);

  asio::io_service& io_service_;
  rudp::ManagedConnections& rudp_;
  ConnectionManager& connection_manager_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGE_HANDLER_H_
