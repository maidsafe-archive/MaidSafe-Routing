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

#include <vector>

#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/header.h"
#include "maidsafe/routing/messages.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

class message_handler {
 public:
  message_handler(AsioService& asio_service, rudp::ManagedConnections& managed_connections);
  message_handler() = delete;
  message_handler(const message_handler&) = delete;
  message_handler(const message_handler&&) = delete;
  message_handler& operator=(const message_handler&) = delete;
  message_handler& operator=(message_handler&&) = delete;

  void OnMessageReceived(const serialised_message& serialised_message);

 private:
  template <typename T>
  void HandleMessage(const serialised_message& serialised_payload) {

  }

  AsioService& asio_service_;
  rudp::ManagedConnections& managed_connections_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGE_HANDLER_H_
