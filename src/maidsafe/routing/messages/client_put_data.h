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

#ifndef MAIDSAFE_ROUTING_MESSAGES_FORWARD_PUT_DATA_RESPONSE_H_
#define MAIDSAFE_ROUTING_MESSAGES_FORWARD_PUT_DATA_RESPONSE_H_

#include <vector>

#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

struct ClientPutData {
  ClientPutData() = default;
  ~ClientPutData() = default;

//  ClientPutData(SerialisedMessage new_data,
//                 std::vector<crypto::SHA1Hash> new_part,
//                 asymm::PublicKey new_requester_public_key)
//    : data{std::move(new_data)},
//      part{std::move(new_part)},
//      requester_public_key{std::move(new_requester_public_key)} {}

  // The one above will have either double move or 1 copy 1 move or double copy (if a parameter
  // does not have a move ctor) depending on invocation site.
  // The one below will always have single move or single copy depending on invocation site.
  // Also if the type of the member var is changed we will have to revisit the one above, while
  // there will be no change in the signature of the one below.

  template<typename T, typename U, typename V>
  ClientPutData(T&& data_in, U&& part_in, V&& requester_public_key_in)
      : data{std::forward<T>(data_in)},
        part{std::forward<U>(part_in)},
        requester_public_key{std::forward<V>(requester_public_key_in)} {}

  ClientPutData(ClientPutData&& other) MAIDSAFE_NOEXCEPT
      : data{std::move(other.data)},
        part{std::move(other.part)},
        requester_public_key{std::move(other.requester_public_key)} {}

  ClientPutData& operator=(ClientPutData&& other) MAIDSAFE_NOEXCEPT {
    data = std::move(other.data);
    part = std::move(other.part);
    requester_public_key = std::move(other.requester_public_key);
    return *this;
  }

  ClientPutData(const ClientPutData&) = delete;
  ClientPutData& operator=(const ClientPutData&) = delete;

  void operator()() {

  }

  template<typename Archive>
  void serialize(Archive& archive) {
    archive(data, part, requester_public_key);
  }

  SerialisedMessage data;
  std::vector<crypto::SHA1Hash> part;
  asymm::PublicKey requester_public_key;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_FORWARD_PUT_DATA_RESPONSE_H_
