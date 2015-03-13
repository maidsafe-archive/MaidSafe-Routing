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

#include "boost/optional/optional.hpp"

#include "maidsafe/common/config.h"
#include "maidsafe/common/crypto.h"
#include "maidsafe/common/error.h"
#include "maidsafe/common/rsa.h"

#include "maidsafe/routing/source_address.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

class MessageHeader {
 public:
  MessageHeader() = default;
  ~MessageHeader() = default;

  MessageHeader(DestinationAddress destination, SourceAddress source, MessageId message_id,
                Authority our_authority, asymm::Signature signature)
      : destination_(std::move(destination)),
        source_(std::move(source)),
        message_id_(message_id),
        authority_(our_authority),
        signature_(std::move(signature)) {
    Validate();
  }

  MessageHeader(DestinationAddress destination, SourceAddress source, MessageId message_id,
                Authority our_authority)
      : destination_(std::move(destination)),
        source_(std::move(source)),
        message_id_(message_id),
        authority_(our_authority),
        signature_() {
    Validate();
  }

  MessageHeader(MessageHeader&& other) MAIDSAFE_NOEXCEPT
      : destination_(std::move(other.destination_)),
        source_(std::move(other.source_)),
        message_id_(std::move(other.message_id_)),
        authority_(std::move(other.authority_)),
        signature_(std::move(other.signature_)) {}

  MessageHeader& operator=(MessageHeader&& other) MAIDSAFE_NOEXCEPT {
    destination_ = std::move(other.destination_);
    source_ = std::move(other.source_);
    message_id_ = std::move(other.message_id_);
    authority_ = std::move(other.authority_);
    signature_ = std::move(other.signature_);
    return *this;
  }

  MessageHeader(const MessageHeader&) = delete;
  MessageHeader& operator=(const MessageHeader&) = delete;

  bool operator==(const MessageHeader& other) const {
    return std::tie(message_id_, destination_, source_, authority_, signature_) ==
           std::tie(other.message_id_, other.destination_, other.source_, other.authority_,
                    other.signature_);
  }

  bool operator!=(const MessageHeader& other) const { return !operator==(other); }

  bool operator<(const MessageHeader& other) const {
    return std::tie(message_id_, destination_, source_, authority_, signature_) <
           std::tie(other.message_id_, other.destination_, other.source_, other.authority_,
                    other.signature_);
  }

  bool operator>(const MessageHeader& other) const { return other.operator<(*this); }
  bool operator<=(const MessageHeader& other) const { return !operator>(other); }
  bool operator>=(const MessageHeader& other) const { return !operator<(other); }

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(destination_, source_, message_id_, authority_, signature_);
  }

  // pair - Destination and reply to address (reply_to means this is a node not in routing tables)
  DestinationAddress Destination() const { return destination_; }
  // pair or pairs (messy, for on the wire efficiency)
  // Actual source, plus an optional pair that may contain a group address (claim to be from a
  // group)
  // OR a reply_to address that will get copied to the Destingation address reply to above (to allow
  // relaying of messages)
  SourceAddress Source() const { return source_; }
  uint32_t MessageId() const { return message_id_; }
  boost::optional<asymm::Signature> Signature() const { return signature_; }
  NodeAddress FromNode() const { return source_.node_address; }
  boost::optional<GroupAddress> FromGroup() const { return source_.group_address; }
  Authority FromAuthority() { return authority_; }
  bool RelayedMessage() const { return static_cast<bool>(source_.reply_to_address); }
  boost::optional<routing::ReplyToAddress> ReplyToAddress() const {
    return source_.reply_to_address;
  }

  Address FromAddress() const {
    if (FromGroup())
      return FromGroup()->data;
    else
      return FromNode().data;
  }

  DestinationAddress ReturnDestinationAddress() const {
    if (RelayedMessage())
      return DestinationAddress(
          std::make_pair(routing::Destination(source_.node_address), *ReplyToAddress()));
    else
      return DestinationAddress(
          std::make_pair(routing::Destination(source_.node_address), boost::none));
  }

  FilterType FilterValue() const { return std::make_pair(source_.node_address, message_id_); }

 private:
  void Validate() const {
    if (source_.node_address->IsInitialised() ||
        ((FromGroup() && FromGroup()->data.IsInitialised()) ||
         (RelayedMessage() && ReplyToAddress()->data.IsInitialised())) ||
        (FromGroup() && RelayedMessage())) {
      return;
    } else {
      LOG(kWarning) << std::boolalpha
                    << "Header is invalid:\n\tsource_.node_address->IsInitialised(): "
                    << source_.node_address->IsInitialised()
                    << "\n\tFromGroup(): " << static_cast<bool>(FromGroup())
                    << "\n\tFromGroup()->data.IsInitialised(): "
                    << (FromGroup() ? (FromGroup()->data.IsInitialised() ? "true" : "false") :
                                      "N/A")
                    << "\n\tRelayedMessage(): " << static_cast<bool>(RelayedMessage())
                    << "\n\tReplyToAddress()->data.IsInitialised(): "
                    << (RelayedMessage() ?
                            (ReplyToAddress()->data.IsInitialised() ? "true" : "false") :
                            "N/A");
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
    }
  }

  DestinationAddress destination_;
  SourceAddress source_;
  routing::MessageId message_id_;
  Authority authority_;
  boost::optional<asymm::Signature> signature_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGE_HEADER_H_
