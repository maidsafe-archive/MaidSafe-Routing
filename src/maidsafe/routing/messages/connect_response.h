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

#ifndef MAIDSAFE_ROUTING_MESSAGES_CONNECT_RESPONSE_H_
#define MAIDSAFE_ROUTING_MESSAGES_CONNECT_RESPONSE_H_

#include <vector>

#include "maidsafe/common/rsa.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

struct ConnectResponse {
  ConnectResponse() = default;
  ~ConnectResponse() = default;

  template<typename T, typename U, typename V, typename W, typename X>
  ConnectResponse(T&& requester_endpoints_in, U&& receiver_endpoints_in,
                  V&& requester_id_in, W&& receiver_id_in, X&& receiver_public_key_in)
      : requester_endpoints{std::forward<T>(requester_endpoints_in)},
        receiver_endpoints{std::forward<U>(receiver_endpoints_in)},
        requester_id{std::forward<V>(requester_id_in)},
        receiver_id{std::forward<W>(receiver_id_in)},
        receiver_public_key{std::forward<X>(receiver_public_key_in)} {}

  ConnectResponse(ConnectResponse&& other) MAIDSAFE_NOEXCEPT
      : requester_endpoints(std::move(other.requester_endpoints)),
        receiver_endpoints(std::move(other.receiver_endpoints)),
        requester_id(std::move(other.requester_id)),
        receiver_id(std::move(other.receiver_id)),
        receiver_public_key(std::move(other.receiver_public_key)) {}

  ConnectResponse& operator=(ConnectResponse&& other) MAIDSAFE_NOEXCEPT {
    requester_endpoints = std::move(other.requester_endpoints);
    receiver_endpoints = std::move(other.receiver_endpoints);
    requester_id = std::move(other.requester_id);
    receiver_id = std::move(other.receiver_id);
    receiver_public_key = std::move(other.receiver_public_key);
    return *this;
  }

  ConnectResponse(const ConnectResponse&) = delete;
  ConnectResponse& operator=(const ConnectResponse&) = delete;

  void operator()() {

  }

  template<typename Archive>
  void serialize(Archive& archive) {
    archive(requester_endpoints, receiver_endpoints, requester_id,
            receiver_id, receiver_public_key);
  }

  rudp::EndpointPair requester_endpoints;
  rudp::EndpointPair receiver_endpoints;
  Address requester_id;
  Address receiver_id;
  asymm::PublicKey receiver_public_key;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_CONNECT_RESPONSE_H_
