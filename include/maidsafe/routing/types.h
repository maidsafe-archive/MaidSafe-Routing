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
#include <cstdint>

#include "boost/asio/ip/address.hpp"
#include "boost/asio/ip/udp.hpp"
#include "cereal/archives/binary.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/tagged_value.h"
#include "maidsafe/common/serialisation.h"
#ifndef MAIDSAFE_ROUTING_TYPES_H_
#define MAIDSAFE_ROUTING_TYPES_H_

namespace maidsafe {
enum class SerialisableTypeTag : unsigned char {
  kPing,
  kPingResponse,
  kConnect,
  kConnectResponse,
  kFindNode,
  kFindNodeResponse,
  kFindGroup,
  kFindGroupResponse,
  kAcknowledgement,
  kNodeMessage,
  kNodeMessageResponse
};

namespace routing {

using SingleDestinationId = TaggedValue<NodeId, struct singledestination>;
using GroupDestinationId = TaggedValue<NodeId, struct groupdestination>;
using SingleSourceId = TaggedValue<NodeId, struct singlesource>;
using GroupSourceId = TaggedValue<NodeId, struct groupsource>;
using OurEndPoint = TaggedValue<boost::asio::ip::udp::endpoint, struct ourendpoint>;
using TheirEndPoint = TaggedValue<boost::asio::ip::udp::endpoint, struct theirendpoint>;
NodeId OurId(NodeId::IdType::kRandomId);
using maidsafe_serialised = std::string;

struct Header {
  Header() = default;
  Header(Header const&) = default;
  Header(Header&&) = default MAIDSAFE_NOEXCEPT;
  ~Header() = default;
  Header& operator=(Header const&) = default;
  Header& operator=(Header&&) = default MAIDSAFE_NOEXCEPT;

  bool operator==(const Header& other) const MAIDSAFE_NOEXCEPT {
    return std::tie(destination, message_id, part_number) ==
           std::tie(other.destination, other.message_id, other.part_number);
  }
  bool operator!=(const Header& other) const MAIDSAFE_NOEXCEPT { return !operator==(*this, other); }
  bool operator<(const Header& other) const MAIDSAFE_NOEXCEPT {
    return std::tie(destination, message_id, part_number) <
           std::tie(other.destination, other.message_id, other.part_number);
  }
  bool operator>(const Header& other) { return operator<(other, *this); }
  bool operator<=(const Header& other) { return !operator>(*this, other); }
  bool operator>=(const Header& other) { return !operator<(*this, other); }

  DestinationType destination;
  uint32_t message_id;
  uint32_t part_number;
};

struct Ping {
  Ping(SingleDestinationId destination_address)
      : source_address(OurId),
        destination_address(destination_address),
        message_id(RandomUint32()) {}
  template <typename Archive>
  void serialise(Archive& archive) {
    archive(destination_address, message_id, source_address);
  }

  SingleSourceId source_address;
  SingleDestinationId destination_address;
  uint32_t message_id;
};

struct PingResponse {
  PingResponse(Ping ping)
      : source_address(OurId),
        destinaton_address(SingleDestinationId(ping.source_address)),
        message_id(ping.message_id) {}
  template <typename Archive>
  void serialised_response(Archive& archive) {
    archive(destinaton_address, message_id, source_address);
  }

  SingleSourceId source_address;
  SingleDestinationId destinaton_address;
  uint32_t message_id;
};

struct Connect {
  Connect(SingleDestinationId destinaton_address, OurEndPoint our_endpoint)
      : our_endpoint(our_endpoint), our_id(OurId), their_id(destinaton_address) {}

  OurEndPoint our_endpoint;
  SingleSourceId our_id;
  SingleDestinationId their_id;
};

struct ConnectResponse {
  ConnectResponse(Connect connect, OurEndPoint our_endpoint)
      : our_endpoint(our_endpoint),
        their_endpoint(TheirEndPoint(connect.our_endpoint)),
        our_id(SingleSourceId(connect.their_id)),
        their_id(connect.our_id) {}

  OurEndPoint our_endpoint;
  TheirEndPoint their_endpoint;
  SingleSourceId our_id;
  SingleDestinationId their_id;
};

struct FindGroup {};

struct FindGroupResponse {};

struct NodeMessage;
struct NodeMessageResponse;

using Map =
    GetMap<Serialisable<SerialisableTypeTag::kPing, Ping>,
           Serialisable<SerialisableTypeTag::kPingResponse, PingResponse>,
           Serialisable<SerialisableTypeTag::kConnect, Connect>,
           Serialisable<SerialisableTypeTag::kConnectResponse, ConnectResponse>,
           Serialisable<SerialisableTypeTag::kFindGroup, FindGroup>,
           Serialisable<SerialisableTypeTag::kFindGroupResponse, FindGroupResponse>,
           Serialisable<SerialisableTypeTag::kNodeMessage, NodeMessage>,
           Serialisable<SerialisableTypeTag::kNodeMessageResponse, NodeMessageResponse>>::Map;

template <SerialisableTypeTag Tag>
using CustomType = typename Find<Map, Tag>::ResultCustomType;

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TYPES_H_

