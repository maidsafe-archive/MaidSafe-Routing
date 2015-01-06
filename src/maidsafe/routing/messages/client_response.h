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

#ifndef MAIDSAFE_ROUTING_MESSAGES_FORWARD_RESPONSE_H_
#define MAIDSAFE_ROUTING_MESSAGES_FORWARD_RESPONSE_H_

#include <vector>

#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

struct ClientResponse {
  ClientResponse() = default;
  ~ClientResponse() = default;

  template<typename T, typename U, typename V, typename W>
  ClientResponse(T&& key_in, U&& data_in, V&& checksum_in, W&& requesters_public_key_in)
      : key{std::forward<T>(key_in)},
        data{std::forward<U>(data_in)},
        checksum{std::forward<V>(checksum_in)},
        requesters_public_key{std::forward<W>(requesters_public_key_in)} {}

  ClientResponse(ClientResponse&& other) MAIDSAFE_NOEXCEPT
      : key{std::move(other.key)},
        data{std::move(other.data)},
        checksum{std::move(other.checksum)},
        requesters_public_key{std::move(other.requesters_public_key)} {}

  ClientResponse& operator=(ClientResponse&& other) MAIDSAFE_NOEXCEPT {
    key = std::move(other.key);
    data = std::move(other.data);
    checksum = std::move(other.checksum);
    requesters_public_key = std::move(other.requesters_public_key);
    return *this;
  }

  ClientResponse(const ClientResponse&) = delete;
  ClientResponse& operator=(const ClientResponse&) = delete;

  void operator()() {

  }

  template<typename Archive>
  void serialize(Archive& archive) {
    archive(key, data, checksum, requesters_public_key);
  }

  Address key;
  SerialisedMessage data;
  std::vector<crypto::SHA1Hash> checksum;
  asymm::PublicKey requesters_public_key;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_FORWARD_RESPONSE_H_
