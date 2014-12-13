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

#ifndef MAIDSAFE_ROUTING_PING_H_
#define MAIDSAFE_ROUTING_PING_H_

#include "maidsafe/common/config.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/serialisation/compile_time_mapper.h"

#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/messages.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

struct Ping {
  static const SerialisableTypeTag kSerialisableTypeTag =
      static_cast<SerialisableTypeTag>(MessageTypeTag::kPing);

  Ping() = default;

  Ping(const Ping&) = delete;

  Ping(Ping&& other) MAIDSAFE_NOEXCEPT : header(std::move(other.header)),
                                         random_value(std::move(other.random_value)) {}

  Ping(DestinationAddress destination, SourceAddress source)
      : header(std::move(destination), std::move(source), MessageId(RandomUint32())),
        random_value(RandomInt32()) {}

  explicit Ping(MessageHeader header_in) : header(std::move(header_in)), random_value(0) {}

  ~Ping() = default;

  Ping& operator=(const Ping&) = delete;

  Ping& operator=(Ping&& other) MAIDSAFE_NOEXCEPT {
    header = std::move(other.header);
    random_value = std::move(other.random_value);
    return *this;
  };

  template <typename Archive>
  void save(Archive& archive) const {
    archive(header, kSerialisableTypeTag, random_value);
  }

  template <typename Archive>
  void load(Archive& archive) {
    if (!header.source->IsValid()) {
      LOG(kError) << "Invalid header.";
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
    }
    archive(random_value);
  }

  MessageHeader header;
  int random_value;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_PING_H_
