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
#include <vector>

#include "maidsafe/common/config.h"
#include "maidsafe/common/error.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/serialisation/compile_time_mapper.h"

#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/messages/messages_fwd.h"
#include "maidsafe/routing/messages/put_data.h"

namespace maidsafe {

namespace routing {

struct PutDataResponse {
  static const SerialisableTypeTag kSerialisableTypeTag =
      static_cast<SerialisableTypeTag>(MessageTypeTag::PutDataResponse);

  PutDataResponse() = default;

  PutDataResponse(const PutDataResponse&) = delete;

  PutDataResponse(PutDataResponse&& other) MAIDSAFE_NOEXCEPT
      : header(std::move(other.header)),
        data_name(std::move(other.data_name)),
        part(std::move(other.part)),
        result(std::move(other.result)) {}

  PutDataResponse(PutData request, maidsafe_error result_in)
      : header(DestinationAddress(std::move(request.header.source.data)),
               SourceAddress(std::move(request.header.destination.data)),
               request.header.message_id),
        data_name(std::move(request.data_name)),
        part(std::move(request.part)),
        result(std::move(result_in)) {}

  explicit PutDataResponse(MessageHeader header_in)
      : header(std::move(header_in)), data_name(), part(0), result() {}

  ~PutDataResponse() = default;

  PutDataResponse& operator=(const PutDataResponse&) = delete;

  PutDataResponse& operator=(PutDataResponse&& other) MAIDSAFE_NOEXCEPT {
    header = std::move(other.header);
    data_name = std::move(other.data_name);
    part = std::move(other.part);
    result = std::move(other.result);
    return *this;
  };

  template <typename Archive>
  void save(Archive& archive) const {
    archive(header, kSerialisableTypeTag, data_name, result);
  }

  template <typename Archive>
  void load(Archive& archive) {
    if (!header.source->IsValid()) {
      LOG(kError) << "Invalid header.";
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
    }
    archive(data_name, result);
  }

  MessageHeader header;
  Address data_name;
  uint8_t part;
  maidsafe_error result;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_PUT_DATA_RESPONSE_H_
