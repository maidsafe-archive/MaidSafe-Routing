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

#ifndef MAIDSAFE_ROUTING_MESSAGES_CONNECT_H_
#define MAIDSAFE_ROUTING_MESSAGES_CONNECT_H_

#include "maidsafe/routing/types.h"
#include "maidsafe/passport/passport.h"


namespace maidsafe {

namespace routing {

class Connect {
 public:
  Connect() = default;
  ~Connect() = default;


  template <typename T, typename U, typename V, typename W>
  Connect(T&& requester_endpoints, U&& requester_id, V&& receiver_id, W&& requester_fob)
      : requester_endpoints{std::forward<T>(requester_endpoints)},
        requester_id{std::forward<U>(requester_id)},
        receiver_id{std::forward<V>(receiver_id)},
        requester_fob{std::forward<W>(requester_fob)} {}

  Connect(Connect&& other) MAIDSAFE_NOEXCEPT
      : requester_endpoints{std::move(other.requester_endpoints)},
        requester_id{std::move(other.requester_id)},
        receiver_id{std::move(other.receiver_id)} {}

  Connect& operator=(Connect&& other) MAIDSAFE_NOEXCEPT {
    requester_endpoints = std::move(other.requester_endpoints);
    requester_id = std::move(other.requester_id);
    receiver_id = std::move(other.receiver_id);
    return *this;
  }

  Connect(const Connect&) = default;
  Connect& operator=(const Connect&) = default;

  void operator()() {}

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(requester_endpoints, requester_id, receiver_id, requester_fob);
  }
  rudp::EndpointPair get_requester_endpoints() { return requester_endpoints; }
  Address get_requester_id() { return requester_id; }
  Address get_receiver_id() { return receiver_id; }
  passport::PublicPmid get_requester_fob() { return requester_fob; }

 private:
  rudp::EndpointPair requester_endpoints;
  Address requester_id;
  Address receiver_id;
  passport::PublicPmid requester_fob;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_CONNECT_H_
