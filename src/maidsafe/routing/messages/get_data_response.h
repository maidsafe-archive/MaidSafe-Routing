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

  GetDataResponse(const GetDataResponse&) = delete;

  GetDataResponse(GetDataResponse&& other) MAIDSAFE_NOEXCEPT
      : header(std::move(other.header)),
        data_name(std::move(other.data_name)),
        data(std::move(other.data)) {}

  GetDataResponse(GetData request, SerialisedData data_in)
      : header(DestinationAddress(std::move(request.header.source.data)),
               SourceAddress(std::move(request.header.destination.data)),
               request.header.message_id),
        data_name(std::move(request.data_name)),
        data(std::move(data_in)) {}

  explicit GetDataResponse(MessageHeader header_in)
      : header(std::move(header_in)), data_name(), data() {}

  ~GetDataResponse() = default;

  GetDataResponse& operator=(const GetDataResponse&) = delete;

  GetDataResponse& operator=(GetDataResponse&& other) MAIDSAFE_NOEXCEPT {
    header = std::move(other.header);
    data_name = std::move(other.data_name);
    data = std::move(other.data);
    return *this;
  }

  template <typename Archive>
  void save(Archive& archive) const {
    archive(header, GivenTypeFindTag_v<GetDataResponse>::value, data_name, data);
  }

  template <typename Archive>
  void load(Archive& archive) {
    if (!header.source->IsValid()) {
      LOG(kError) << "Invalid header.";
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
    }
    archive(data_name, data);
  }

  MessageHeader header;
  Address data_name;
  SerialisedData data;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_GET_DATA_RESPONSE_H_
