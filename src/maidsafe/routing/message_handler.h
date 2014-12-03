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

#include "maidsafe/common/asio_service.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/messages.h"

namespace maidsafe {

namespace routing {

class ConnectionManager;

class MessageHandler {
 public:
  MessageHandler(AsioService& asio_service, rudp::ManagedConnections& managed_connections,
                 ConnectionManager& connection_manager);
  MessageHandler() = delete;
  ~MessageHandler() = default;
  MessageHandler(const MessageHandler&) = delete;
  MessageHandler(MessageHandler&&) = delete;
  MessageHandler& operator=(const MessageHandler&) = delete;
  MessageHandler& operator=(MessageHandler&&) = delete;
  void OnMessageReceived(rudp::ReceivedMessage&& serialised_message);

 private:
  void HandleMessage(const Ping& ping);
  void HandleMessage(const PingResponse& ping_response);
  void HandleMessage(const FindGroup& find_group);
  void HandleMessage(const FindGroupResponse& find_group_reponse);
  void HandleMessage(const Connect& connect);
  void HandleMessage(const ForwardConnect& forward_connect);
  void HandleMessage(const VaultMessage& vault_message);
  void HandleMessage(const CacheableGet& cacheable_get);
  void HandleMessage(const CacheableGetResponse& cacheable_get_response);

  AsioService& asio_service_;
  rudp::ManagedConnections& rudp_;
  ConnectionManager& connection_manager_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGE_HANDLER_H_
