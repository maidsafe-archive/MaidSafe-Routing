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

#ifndef MAIDSAFE_ROUTING_MESSAGES_FIND_GROUP_H_
#define MAIDSAFE_ROUTING_MESSAGES_FIND_GROUP_H_

#include "maidsafe/common/config.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/serialisation/compile_time_mapper.h"
#include "maidsafe/rudp/contact.h"

#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/messages/messages_fwd.h"

namespace maidsafe {

namespace routing {

struct FindGroup {
  static const SerialisableTypeTag kSerialisableTypeTag =
      static_cast<SerialisableTypeTag>(MessageTypeTag::kFindGroup);

  FindGroup() = default;

  FindGroup(const FindGroup&) = delete;

  FindGroup(FindGroup&& other) MAIDSAFE_NOEXCEPT : header(std::move(other.header)) /*,
        requester_endpoints(std::move(other.requester_endpoints)),
        requester_id(std::move(other.requester_id)),
        receiver_id(std::move(other.receiver_id))*/ {}

  FindGroup(DestinationAddress destination, SourceAddress source /*,
          rudp::EndpointPair requester_endpoints, Address requester_id_in, Address receiver_id_in*/)
      : header(std::move(destination), std::move(source), MessageId(RandomUint32())) /*,
        requester_endpoints(std::move(requester_endpoints)),
        requester_id(std::move(requester_id_in)),
        receiver_id(std::move(receiver_id_in))*/ {}

  explicit FindGroup(MessageHeader header_in)
      : header(std::move(header_in)) /*, requester_endpoints(), requester_id(), receiver_id()*/ {}

  ~FindGroup() = default;

  FindGroup& operator=(const FindGroup&) = delete;

  FindGroup& operator=(FindGroup&& other) MAIDSAFE_NOEXCEPT {
    header = std::move(other.header);
    // requester_endpoints = std::move(other.requester_endpoints);
    // requester_id = std::move(other.requester_id);
    // receiver_id = std::move(other.receiver_id);
    return *this;
  };

  template <typename Archive>
  void save(Archive& archive) const {
    archive(header, kSerialisableTypeTag /*, requester_endpoints, requester_id, receiver_id*/);
  }

  template <typename Archive>
  void load(Archive& /*archive*/) {
    if (!header.source->IsValid()) {
      LOG(kError) << "Invalid header.";
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
    }
    // archive(/*requester_endpoints, requester_id, receiver_id*/);
  }

  MessageHeader header;
  // rudp::EndpointPair requester_endpoints;
  // Address requester_id, receiver_id;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_FIND_GROUP_H_
