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

#ifndef MAIDSAFE_ROUTING_MESSAGE_HEADER_H_
#define MAIDSAFE_ROUTING_MESSAGE_HEADER_H_

#include <cstdint>
#include <tuple>

#include "maidsafe/common/config.h"
#include "maidsafe/common/crypto.h"

#include "maidsafe/routing/types.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

class MessageHeader {
 public:
  MessageHeader() = default;
  ~MessageHeader() = default;

  template <typename T, typename U>
  MessageHeader(T&& destination, U&& source, MessageId message_id, asymm::Signature&& signature)
      : destination{std::forward<T>(destination)},
        source{std::forward<U>(source)},
        message_id(message_id),
        signature{std::forward<asymm::Signature>(signature)} {}

  template <typename T, typename U>
  MessageHeader(T&& destination, U&& source, MessageId message_id, Checksum&& checksum)
      : destination{std::forward<T>(destination)},
        source{std::forward<U>(source)},
        message_id(message_id),
        checksum{std::forward<Checksum>(checksum)} {}

  template <typename T, typename U>
  MessageHeader(T&& destination, U&& source, MessageId message_id)
      : destination{std::forward<T>(destination)},
        source{std::forward<U>(source)},
        message_id{message_id} {}

  MessageHeader(MessageHeader&& other) MAIDSAFE_NOEXCEPT
      : destination(std::move(other.destination)),
        source(std::move(other.source)),
        message_id(std::move(other.message_id)),
        checksum(std::move(other.checksum)),
        signature(std::move(other.signature)) {}

  MessageHeader& operator=(MessageHeader&& other) MAIDSAFE_NOEXCEPT {
    destination = std::move(other.destination);
    source = std::move(other.source);
    message_id = std::move(other.message_id);
    checksum = std::move(other.checksum);
    signature = std::move(other.signature);
    return *this;
  }

  MessageHeader(const MessageHeader&) = delete;
  MessageHeader& operator=(const MessageHeader&) = delete;

  // regular
  bool operator==(const MessageHeader& other) const {
    return std::tie(message_id, destination, source) ==
           std::tie(other.message_id, other.destination, other.source);
  }

  bool operator!=(const MessageHeader& other) const { return !operator==(other); }

  // fully ordered
  bool operator<(const MessageHeader& other) const {
    return std::tie(message_id, destination, source) <
           std::tie(other.message_id, other.destination, other.source);
  }

  bool operator>(const MessageHeader& other) const {
    return !(operator<(other) || operator==(other));
  }

  bool operator<=(const MessageHeader& other) const { return !operator>(other); }
  bool operator>=(const MessageHeader& other) const { return !operator<(other); }

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(destination.data, source.data, message_id, checksum, signature);
  }
  DestinationAddress GetDestination() { return destination; }
  SourceAddress GetSource() { return source; }
  uint32_t GetMessageId() { return message_id; }
  boost::optional<Checksum> GetChecksum() { return checksum; }
  boost::optional<asymm::Signature> GetSignature() { return signature; }

 private:
  DestinationAddress destination;
  SourceAddress source;
  uint32_t message_id;
  boost::optional<crypto::SHA1Hash> checksum;
  boost::optional<asymm::Signature> signature;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGE_HEADER_H_
