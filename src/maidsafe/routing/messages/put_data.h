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

#ifndef MAIDSAFE_ROUTING_MESSAGES_PUT_DATA_H_
#define MAIDSAFE_ROUTING_MESSAGES_PUT_DATA_H_

#include <cstdint>
#include <vector>

#include "maidsafe/common/config.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/compile_time_mapper.h"

#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/messages/messages_fwd.h"

namespace maidsafe {

namespace routing {

struct PutData {
  PutData() = default;
  ~PutData() = default;

  template<typename T, typename U, typename V, typename W>
  PutData(T&& data_name_in, U&& signature_in, V&& data_in, W&& part_in)
      : data_name{std::forward<T>(data_name_in)},
        signature{std::forward<U>(signature_in)},
        data{std::forward<V>(data_in)},
        part{std::forward<W>(part_in)} {}

  PutData(PutData&& other) MAIDSAFE_NOEXCEPT : data_name(std::move(other.data_name)),
                                               signature(std::move(other.signature)),
                                               data(std::move(other.data)),
                                               part(std::move(other.part)) {}

  PutData(Address data_name_in, asymm::Signature signature_in,
          std::vector<byte> data_in, uint8_t part_in)
      : data_name(std::move(data_name_in)),
        signature(std::move(signature_in)),
        data(std::move(data_in)),
        part(std::move(part_in)) {}

  PutData& operator=(PutData&& other) MAIDSAFE_NOEXCEPT {
    data_name = std::move(other.data_name);
    signature = std::move(other.signature);
    data = std::move(other.data);
    part = std::move(other.part);
    return *this;
  }

  PutData(const PutData&) = delete;
  PutData& operator=(const PutData&) = delete;

  void operator()() {

  }

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(data_name, signature, data, part);
  }

  Address data_name;
  asymm::Signature signature;
  std::vector<byte> data;
  uint8_t part;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_PUT_DATA_H_
