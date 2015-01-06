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

struct MessageHeader {
  MessageHeader() = default;
  ~MessageHeader() = default;

  template <typename T, typename U, typename V, typename W, typename X>
  MessageHeader(T&& destination_in, U&& source_in, V&& checksums_in, W&& message_id_in,
                X&& signature_in)
      : destination{std::forward<T>(destination_in)},
        source{std::forward<U>(source_in)},
        checksums{std::forward<V>(checksums_in)},
        message_id{std::forward<W>(message_id_in)},
        signature{std::forward<X>(signature_in)} {}

  template <typename T, typename U, typename V, typename W, typename X>
  MessageHeader(T&& destination_in, U&& source_in, V&& checksums_in, X&& message_id_in)
      : destination{std::forward<T>(destination_in)},
        source{std::forward<U>(source_in)},
        checksums{std::forward<V>(checksums_in)},
        message_id{std::forward<W>(message_id_in)},
        signature{} {}

  MessageHeader(MessageHeader&& other) MAIDSAFE_NOEXCEPT
      : destination(std::move(other.destination)),
        source(std::move(other.source)),
        checksums(std::move(other.checksums)),
        message_id(std::move(other.message_id)),
        signature(std::move(other.signature)) {}

  MessageHeader& operator=(MessageHeader&& other) MAIDSAFE_NOEXCEPT {
    destination = std::move(other.destination);
    source = std::move(other.source);
    checksums = std::move(other.checksums);
    message_id = std::move(other.message_id);
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
    archive(destination.data, source.data, checksums, message_id, signature);
  }

  DestinationAddress destination;
  SourceAddress source;
  std::vector<crypto::SHA1Hash> checksums;
  uint32_t message_id;
  boost::optional<asymm::Signature> signature;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGE_HEADER_H_
