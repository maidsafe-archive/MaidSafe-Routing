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
#include "maidsafe/common/error.h"

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
      : destination_{std::forward<T>(destination)},
        source_{std::forward<U>(source)},
        message_id_(message_id),
        signature_{std::forward<asymm::Signature>(signature)} {
    if (!ValidateHeader())
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
  }

  template <typename T, typename U>
  MessageHeader(T&& destination, U&& source, MessageId message_id)
      : destination_{std::forward<T>(destination)},
        source_{std::forward<U>(source)},
        message_id_{message_id} {
    if (!ValidateHeader())
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
  }

  MessageHeader(MessageHeader&& other) MAIDSAFE_NOEXCEPT
      : destination_(std::move(other.destination_)),
        source_(std::move(other.source_)),
        message_id_(std::move(other.message_id_)),
        signature_(std::move(other.signature_)) {}

  MessageHeader& operator=(MessageHeader&& other) MAIDSAFE_NOEXCEPT {
    destination_ = std::move(other.destination_);
    source_ = std::move(other.source_);
    message_id_ = std::move(other.message_id_);
    signature_ = std::move(other.signature_);
    return *this;
  }

  MessageHeader(const MessageHeader&) = delete;
  MessageHeader& operator=(const MessageHeader&) = delete;

  // regular
  bool operator==(const MessageHeader& other) const {
    return std::tie(message_id_, destination_, source_) ==
           std::tie(other.message_id_, other.destination_, other.source_);
  }

  bool operator!=(const MessageHeader& other) const { return !operator==(other); }

  // fully ordered
  bool operator<(const MessageHeader& other) const {
    return std::tie(message_id_, destination_, source_) <
           std::tie(other.message_id_, other.destination_, other.source_);
  }

  bool operator>(const MessageHeader& other) const {
    return !(operator<(other) || operator==(other));
  }

  bool operator<=(const MessageHeader& other) const { return !operator>(other); }
  bool operator>=(const MessageHeader& other) const { return !operator<(other); }

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(destination_, source_, message_id_, signature_);
  }
  // pair - Destination and reply to address (reply_to means this is a node not in routing tables)
  DestinationAddress GetDestination() const { return destination_; }
  // pair or pairs (messy, for on the wire efficiency)
  // Actual source, plus an optional pair that may contain a group address (claim to be from a
  // group)
  // OR a reply_to address that will get copied to the Destingation address reply to above (to allow
  // relaying of messages)
  SourceAddress GetSource() const { return source_; }
  uint32_t GetMessageId() const { return message_id_; }
  boost::optional<asymm::Signature> GetSignature() const { return signature_; }
  NodeAddress FromNode() const { return std::get<0>(source_); }
  boost::optional<GroupAddress> FromGroup() const { return std::get<1>(source_); }

  boost::optional<ReplyToAddress> RelayedMessage() const { return std::get<2>(source_); }

  Address FromAddress() const {
    if (FromGroup())
      return FromGroup()->data;
    else
      return FromNode().data;
  }

  DestinationAddress ReturnDestinationAddress() const {
    if (RelayedMessage())
      return DestinationAddress(
          std::make_pair(Destination(std::get<0>(source_)), *RelayedMessage()));
    else
      return DestinationAddress(std::make_pair(Destination(std::get<0>(source_)), boost::none));
  }
  bool ValidateHeader() const {
    return (std::get<0>(source_)->IsValid() ||
            ((FromGroup() && FromGroup()->data.IsValid()) ||
             (RelayedMessage() && RelayedMessage()->data.IsValid())) ||
            (FromGroup() && RelayedMessage()));
  }
  FilterType FilterValue() { return std::make_pair(std::get<0>(source_), message_id_); }

 private:
  DestinationAddress destination_;
  SourceAddress source_;
  MessageId message_id_;
  boost::optional<asymm::Signature> signature_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGE_HEADER_H_
