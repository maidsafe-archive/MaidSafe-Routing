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

#ifndef MAIDSAFE_ROUTING_MESSAGES_PUT_DATA_RESPONSE_H_
#define MAIDSAFE_ROUTING_MESSAGES_PUT_DATA_RESPONSE_H_

#include <cstdint>
#include "boost/optional/optional.hpp"
#include "maidsafe/common/config.h"
#include "maidsafe/common/error.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/messages/messages_fwd.h"
#include "maidsafe/routing/messages/put_data.h"

namespace maidsafe {

namespace routing {

struct PutDataResponse {
  PutDataResponse() = default;
  ~PutDataResponse() = default;

  template <typename T, typename U>
  PutDataResponse(T&& key_in, U&& result_in)
      : key{std::forward<T>(key_in)}, result{std::forward<U>(result_in)} {}

  PutDataResponse(PutDataResponse&& other) MAIDSAFE_NOEXCEPT : key{std::move(other.key)},
                                                               result{std::move(other.result)} {}

  PutDataResponse& operator=(PutDataResponse&& other) MAIDSAFE_NOEXCEPT {
    key = std::move(other.key);
    result = std::move(other.result);
    return *this;
  }

  PutDataResponse(const PutDataResponse&) = delete;
  PutDataResponse& operator=(const PutDataResponse&) = delete;

  void operator()() {}

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(key, result);
  }

  Address key;
  maidsafe_error result;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_PUT_DATA_RESPONSE_H_
