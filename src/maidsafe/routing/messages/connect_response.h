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

class ConnectResponse {
 public:
  ConnectResponse() = default;
  ~ConnectResponse() = default;

  template <typename T, typename U, typename V, typename W, typename X>
  ConnectResponse(T&& requester_endpoints, U&& receiver_endpoints, V&& requester_id,
                  W&& receiver_id, X&& receiver_fob)
      : requester_endpoints_{std::forward<T>(requester_endpoints)},
        receiver_endpoints_{std::forward<U>(receiver_endpoints)},
        requester_id_{std::forward<V>(requester_id)},
        receiver_id_{std::forward<W>(receiver_id)},
        receiver_fob_{std::forward<X>(receiver_fob)} {}

  ConnectResponse(ConnectResponse&& other) MAIDSAFE_NOEXCEPT
      : requester_endpoints_(std::move(other.requester_endpoints_)),
        receiver_endpoints_(std::move(other.receiver_endpoints_)),
        requester_id_(std::move(other.requester_id_)),
        receiver_id_(std::move(other.receiver_id_)),
        receiver_fob_(std::move(other.receiver_fob_)) {}

  ConnectResponse& operator=(ConnectResponse&& other) MAIDSAFE_NOEXCEPT {
    requester_endpoints_ = std::move(other.requester_endpoints_);
    receiver_endpoints_ = std::move(other.receiver_endpoints_);
    requester_id_ = std::move(other.requester_id_);
    receiver_id_ = std::move(other.receiver_id_);
    receiver_fob_ = std::move(other.receiver_fob_);
    return *this;
  }

  ConnectResponse(const ConnectResponse&) = default;
  ConnectResponse& operator=(const ConnectResponse&) = default;

  void operator()() {}

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(requester_endpoints_, receiver_endpoints_, requester_id_, receiver_id_, receiver_fob_);
  }

  rudp::EndpointPair get_requester_endpoints() { return requester_endpoints_; }
  rudp::EndpointPair get_receiver_endpoints() { return receiver_endpoints_; }
  Address get_requester_id() { return requester_id_; }
  Address get_receiver_id() { return receiver_id_; }
  passport::PublicPmid get_receiver_fob() { return receiver_fob_; }

 private:
  rudp::EndpointPair requester_endpoints_;
  rudp::EndpointPair receiver_endpoints_;
  Address requester_id_;
  Address receiver_id_;
  passport::PublicPmid receiver_fob_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_CONNECT_RESPONSE_H_
