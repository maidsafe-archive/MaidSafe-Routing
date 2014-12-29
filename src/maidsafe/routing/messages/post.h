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

#ifndef MAIDSAFE_ROUTING_MESSAGES_POST_H_
#define MAIDSAFE_ROUTING_MESSAGES_POST_H_

#include <cstdint>
#include <vector>

#include "maidsafe/common/config.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/types.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/serialisation/compile_time_mapper.h"
#include "maidsafe/common/serialisation/serialisation.h"

#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/messages/messages_fwd.h"

namespace maidsafe {

namespace routing {

struct Post {
  static const SerialisableTypeTag kSerialisableTypeTag =
      static_cast<SerialisableTypeTag>(MessageTypeTag::Post);

  Post() = default;

  Post(const Post&) = delete;

  Post(Post&& other) MAIDSAFE_NOEXCEPT : header(std::move(other.header)),
                                         data_name(std::move(other.data_name)),
                                         signature(std::move(other.signature)),
                                         data(std::move(other.data)),
                                         part(std::move(other.part)) {}

  Post(DestinationAddress destination, SourceAddress source, Address data_name_in,
       asymm::Signature signature_in, std::vector<byte> data_in, uint8_t part_in)
      : header(std::move(destination), std::move(source), MessageId(RandomUint32())),
        data_name(std::move(data_name_in)),
        signature(std::move(signature_in)),
        data(std::move(data_in)),
        part(std::move(part_in)) {}

  explicit Post(MessageHeader header_in)
      : header(std::move(header_in)), data_name(), signature(), data(), part(0) {}

  ~Post() = default;

  Post& operator=(const Post&) = delete;

  Post& operator=(Post&& other) MAIDSAFE_NOEXCEPT {
    header = std::move(other.header);
    data_name = std::move(other.data_name);
    signature = std::move(other.signature);
    data = std::move(other.data);
    part = std::move(other.part);
    return *this;
  };

  template <typename Archive>
  void save(Archive& archive) const {
    archive(header, kSerialisableTypeTag, data_name, signature, data, part);
  }

  template <typename Archive>
  void load(Archive& archive) {
    if (!header.source->IsValid()) {
      LOG(kError) << "Invalid header.";
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
    }
    archive(data_name, signature, data, part);
  }

  MessageHeader header;
  Address data_name;
  asymm::Signature signature;
  SerialisedData data;
  uint8_t part;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_POST_H_
