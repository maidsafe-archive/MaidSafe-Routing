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

#ifndef MAIDSAFE_ROUTING_MESSAGES_FORWARD_CONNECT_H_
#define MAIDSAFE_ROUTING_MESSAGES_FORWARD_CONNECT_H_

#include "maidsafe/common/config.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/compile_time_mapper.h"
#include "maidsafe/rudp/contact.h"

#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/utils.h"
#include "maidsafe/routing/messages/connect.h"
#include "maidsafe/routing/messages/messages_fwd.h"

namespace maidsafe {

namespace routing {

struct ClientConnect {
  ClientConnect() = default;
  ~ClientConnect() = default;

  template<typename T, typename U, typename V, typename W>
  ClientConnect(T&& requester_endpoints_in, U&& requester_id_in,
                 V&& receiver_id_in, W&& receiver_public_key_in)
      : requester_endpoints{std::forward<T>(requester_endpoints_in)},
        requester_id{std::forward<U>(requester_id_in)},
        receiver_id{std::forward<V>(receiver_id_in)},
        receiver_public_key{std::forward<W>(receiver_public_key_in)} {}

  ClientConnect(ClientConnect&& other) MAIDSAFE_NOEXCEPT
      : requester_endpoints(std::move(other.requester_endpoints)),
        requester_id(std::move(other.requester_id)),
        receiver_id(std::move(other.receiver_id)),
        receiver_public_key(std::move(other.receiver_public_key)) {}

  ClientConnect& operator=(ClientConnect&& other) MAIDSAFE_NOEXCEPT {
    requester_endpoints = std::move(other.requester_endpoints);
    requester_id = std::move(other.requester_id);
    receiver_id = std::move(other.receiver_id);
    receiver_public_key = std::move(other.receiver_public_key);
    return *this;
  }

  ClientConnect(const ClientConnect&) = delete;
  ClientConnect& operator=(const ClientConnect&) = delete;

  void operator()() {

  }

  template<typename Archive>
  void serialize(Archive& archive) {
    archive(requester_endpoints, requester_id, receiver_id, receiver_public_key);
  }

  rudp::EndpointPair requester_endpoints;
  Address requester_id;
  Address receiver_id;
  asymm::PublicKey receiver_public_key;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_FORWARD_CONNECT_H_
