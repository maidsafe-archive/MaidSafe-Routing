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

#ifndef MAIDSAFE_ROUTING_TYPES_H_
#define MAIDSAFE_ROUTING_TYPES_H_

#include <array>
#include <cstdint>
#include <vector>

#include "boost/asio/ip/udp.hpp"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/tagged_value.h"
#include "maidsafe/common/serialisation.h"

#include "maidsafe/routing/utils.h"

namespace maidsafe {

enum class SerialisableTypeTag : unsigned char {
  kPing,
  kPingResponse,
  kFindGroup,
  kFindGroupResponse,
  kConnect,
  kConnectResponse,
  kNodeMessage,
  kCacheableGet,
  kCacheableGetResponse
};


namespace routing {

static const size_t kGroupSize = 32;
static const size_t kQuorumSize = 29;
static const size_t kRoutingTableSize = 64;
extern const NodeId OurId;  // FIXME - remove this
using SingleDestinationId = TaggedValue<NodeId, struct SingleDestinationTag>;
using GroupDestinationId = TaggedValue<NodeId, struct GroupDestinationTag>;
using SingleSourceId = TaggedValue<NodeId, struct SingleSourceTag>;
using GroupSourceId = TaggedValue<NodeId, struct GroupSourceTag>;
using MessageId = TaggedValue<uint32_t, struct MessageIdTag>;
using OurEndpoint = TaggedValue<boost::asio::ip::udp::endpoint, struct OurEndpointTag>;
using TheirEndpoint = TaggedValue<boost::asio::ip::udp::endpoint, struct TheirEndpointTag>;
using byte = unsigned char;
using CheckSums = std::array<Murmur, kGroupSize - 1>;
using SerialisedMessage = std::vector<unsigned char>;


// For use with small messages which aren't scatter/gathered
template <typename Destination, typename Source>
struct SmallMessage {
  SmallMessage() = delete;
  SmallMessage(const SmallMessage&) = delete;
  SmallMessage(SmallMessage&&) MAIDSAFE_NOEXCEPT = default;
  SmallMessage(Destination destination_in, Source source_in, MessageId message_id_in,
               Murmur checksum_in)
      : destination(std::move(destination_in)),
        source(std::move(source_in)),
        message_id(std::move(message_id_in)),
        checksum(std::move(checksum_in)) {}
  ~SmallMessage() = default;
  SmallMessage& operator=(SmallMessage const&) = delete;
  SmallMessage& operator=(SmallMessage&&) MAIDSAFE_NOEXCEPT = default;

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(destination, source, message_id, checksum);
  }

  Destination destination;
  Source source;
  MessageId message_id;
  Murmur checksum;
};

// For use with scatter/gather messages
template <typename Destination, typename Source>
struct Message {
  Message() = delete;
  Message(const Message&) = delete;
  Message(Message&&) MAIDSAFE_NOEXCEPT = default;
  Message(Destination destination_in, Source source_in, MessageId message_id_in,
          Murmur payload_checksum_in, CheckSums other_checksum_in)
      : basic_info(std::move(destination_in), std::move(source_in), std::move(message_id_in),
                   std::move(payload_checksum_in)),
        other_checksums(std::move(other_checksum_in)) {}
  ~Message() = default;
  Message& operator=(Message const&) = delete;
  Message& operator=(Message&&) MAIDSAFE_NOEXCEPT = default;

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(basic_info, other_checksums);
  }

  SmallMessage<Destination, Source> basic_info;
  CheckSums other_checksums;
};



struct Ping {
  explicit Ping(SingleDestinationId destination_address)
      : source_address(OurId),
        destination_address(destination_address),
        message_id(RandomUint32()) {}
  template <typename Archive>
  void serialize(Archive& archive) {
    archive(destination_address, message_id, source_address);
  }

  SingleSourceId source_address;
  SingleDestinationId destination_address;
  MessageId message_id;
};

struct PingResponse {
  explicit PingResponse(Ping ping)
      : source_address(OurId),
        destinaton_address(SingleDestinationId(ping.source_address)),
        message_id(ping.message_id) {}
  template <typename Archive>
  void serialize(Archive& archive) {
    archive(destinaton_address, message_id, source_address);
  }

  SingleSourceId source_address;
  SingleDestinationId destinaton_address;
  MessageId message_id;
};

struct Connect {
  Connect(SingleDestinationId destinaton_address, OurEndpoint our_endpoint)
      : our_endpoint(our_endpoint), our_id(OurId), their_id(destinaton_address) {}

  OurEndpoint our_endpoint;
  SingleSourceId our_id;
  SingleDestinationId their_id;
};

struct ConnectResponse {
  ConnectResponse(Connect connect, OurEndpoint our_endpoint)
      : our_endpoint(our_endpoint),
        their_endpoint(TheirEndpoint(connect.our_endpoint)),
        our_id(SingleSourceId(connect.their_id)),
        their_id(connect.our_id) {}

  OurEndpoint our_endpoint;
  TheirEndpoint their_endpoint;
  SingleSourceId our_id;
  SingleDestinationId their_id;
};

struct FindGroup {};

struct FindGroupResponse {};

struct NodeMessage;

struct CacheableGet;
struct CacheableGetResponse;


using Map =
    GetMap<Serialisable<SerialisableTypeTag::kPing, Ping>,
           Serialisable<SerialisableTypeTag::kPingResponse, PingResponse>,
           Serialisable<SerialisableTypeTag::kConnect, Connect>,
           Serialisable<SerialisableTypeTag::kConnectResponse, ConnectResponse>,
           Serialisable<SerialisableTypeTag::kFindGroup, FindGroup>,
           Serialisable<SerialisableTypeTag::kFindGroupResponse, FindGroupResponse>,
           Serialisable<SerialisableTypeTag::kNodeMessage, NodeMessage>,
           Serialisable<SerialisableTypeTag::kCacheableGet, CacheableGet>,
           Serialisable<SerialisableTypeTag::kCacheableGetResponse, CacheableGetResponse>>::Map;

template <SerialisableTypeTag Tag>
using CustomType = typename Find<Map, Tag>::ResultCustomType;

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TYPES_H_
