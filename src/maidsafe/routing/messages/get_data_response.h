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

#ifndef MAIDSAFE_ROUTING_MESSAGES_GET_DATA_RESPONSE_H_
#define MAIDSAFE_ROUTING_MESSAGES_GET_DATA_RESPONSE_H_

#include "maidsafe/common/config.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/compile_time_mapper.h"

#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/messages/get_data.h"
#include "maidsafe/routing/messages/messages_fwd.h"

namespace maidsafe {

namespace routing {

struct GetDataResponse {
  GetDataResponse() = default;
  ~GetDataResponse() = default;

//  GetDataResponse(Address data_name_in, SerialisedData data_in)
//      : key{std::move(data_name_in)},
//        data{std::move(data_in)} {}

  // The one above will have either double move or 1 copy 1 move or double copy (if a parameter
  // does not have a move ctor) depending on invocation site.
  // The one below will always have single move or single copy depending on invocation site.
  // Also if the type of the member var is changed we will have to revisit the one above, while
  // there will be no change in the signature of the one below.

  template<typename T, typename U>
  GetDataResponse(T&& key_in, U&& data_in)
      : key{std::forward<T>(key_in)},
        data{std::forward<U>(data_in)} {}

  GetDataResponse(GetDataResponse&& other) MAIDSAFE_NOEXCEPT
      : key{std::move(other.key)},
        data{std::move(other.data)} {}

  GetDataResponse& operator=(GetDataResponse&& other) MAIDSAFE_NOEXCEPT {
    key = std::move(other.key);
    data = std::move(other.data);
    return *this;
  }

  GetDataResponse(const GetDataResponse&) = delete;
  GetDataResponse& operator=(const GetDataResponse&) = delete;

  void operator()() {

  }

  template<typename Archive>
  void serialize(Archive& archive) {
    archive(key, data);
  }

  Address key;
  SerialisedData data;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_GET_DATA_RESPONSE_H_
