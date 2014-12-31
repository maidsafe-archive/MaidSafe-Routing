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

#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

struct ConnectResponse {
  ConnectResponse() = default;

  ConnectResponse(const ConnectResponse&) = delete;

  ConnectResponse(ConnectResponse&& other) MAIDSAFE_NOEXCEPT
      : requester_endpoints(std::move(other.requester_endpoints)),
        receiver_endpoints(std::move(other.receiver_endpoints)),
        requester_id(std::move(other.requester_id)),
        receiver_id(std::move(other.receiver_id)) {}

  ~ConnectResponse() = default;

  ConnectResponse& operator=(const ConnectResponse&) = delete;

  ConnectResponse& operator=(ConnectResponse&& other) MAIDSAFE_NOEXCEPT {
    requester_endpoints = std::move(other.requester_endpoints);
    receiver_endpoints = std::move(other.receiver_endpoints);
    requester_id = std::move(other.requester_id);
    receiver_id = std::move(other.receiver_id);
    return *this;
  }

  void operator()() {

  }

  template<typename Archive>
  void serialize(Archive& archive) {
    archive(requester_endpoints, receiver_endpoints, requester_id, receiver_id);
  }

  rudp::EndpointPair requester_endpoints;
  rudp::EndpointPair receiver_endpoints;
  Address requester_id;
  Address receiver_id;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_CONNECT_RESPONSE_H_
