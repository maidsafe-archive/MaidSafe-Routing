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

#ifndef MAIDSAFE_ROUTING_PING_RESPONSE_H_
#define MAIDSAFE_ROUTING_PING_RESPONSE_H_

#include "maidsafe/common/config.h"
#include "maidsafe/common/serialisation/compile_time_mapper.h"

#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/messages.h"
#include "maidsafe/routing/ping.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

struct PingResponse {
  static const SerialisableTypeTag kSerialisableTypeTag =
      static_cast<SerialisableTypeTag>(MessageTypeTag::kPingResponse);

  PingResponse() = default;

  PingResponse(const PingResponse&) = delete;

  PingResponse(PingResponse&& other) MAIDSAFE_NOEXCEPT : header(std::move(other.header)),
                                                         ping_value(std::move(other.ping_value)) {}

  explicit PingResponse(Ping ping)
      : header(DestinationAddress(std::move(ping.header.source.data)),
               SourceAddress(std::move(ping.header.destination.data)),
               MessageId(std::move(ping.header.message_id))),
        ping_value(std::move(ping.random_value)) {}

  explicit PingResponse(MessageHeader header_in) : header(std::move(header_in)), ping_value(0) {}

  ~PingResponse() = default;

  PingResponse& operator=(const PingResponse&) = delete;

  PingResponse& operator=(PingResponse&& other) MAIDSAFE_NOEXCEPT {
    header = std::move(other.header);
    ping_value = std::move(other.ping_value);
    return *this;
  };

  template <typename Archive>
  void save(Archive& archive) const {
    archive(header, kSerialisableTypeTag, ping_value);
  }

  template <typename Archive>
  void load(Archive& archive) {
    if (!header.source->IsValid()) {
      LOG(kError) << "Invalid header.";
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
    }
    archive(ping_value);
  }

  MessageHeader header;
  int ping_value;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_PING_RESPONSE_H_
