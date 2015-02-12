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

  ConnectResponse(EndpointPair requester_endpoints, EndpointPair receiver_endpoints,
                  Address requester_id, Address receiver_id, passport::PublicPmid receiver_fob)
      : requester_endpoints_(std::move(requester_endpoints)),
        receiver_endpoints_(std::move(receiver_endpoints)),
        requester_id_(std::move(requester_id)),
        receiver_id_(std::move(receiver_id)),
        receiver_fob_(std::move(receiver_fob)) {}

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

  ConnectResponse(const ConnectResponse&) = delete;
  ConnectResponse& operator=(const ConnectResponse&) = delete;

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(requester_endpoints_, receiver_endpoints_, requester_id_, receiver_id_, receiver_fob_);
  }

  EndpointPair requester_endpoints() const { return requester_endpoints_; }
  EndpointPair receiver_endpoints() const { return receiver_endpoints_; }
  Address requester_id() const { return requester_id_; }
  Address receiver_id() const { return receiver_id_; }
  passport::PublicPmid receiver_fob() const { return receiver_fob_; }

 private:
  EndpointPair requester_endpoints_;
  EndpointPair receiver_endpoints_;
  Address requester_id_;
  Address receiver_id_;
  passport::PublicPmid receiver_fob_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_CONNECT_RESPONSE_H_
