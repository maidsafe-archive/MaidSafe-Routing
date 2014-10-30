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

#include "maidsafe/common/crypto.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/tagged_value.h"
#include "maidsafe/common/serialisation.h"

namespace maidsafe {

namespace routing {

enum class MessageType : unsigned char {
  kPing,
  kPingResponse,
  kConnect,
  kConnectResponse,
  kVaultMessage,
  kCacheableGet,
  kCacheableGetResponse
};

static const size_t kGroupSize = 32;
static const size_t kQuorumSize = 29;

using SingleDestinationId = TaggedValue<NodeId, struct SingleDestinationTag>;
using GroupDestinationId = TaggedValue<NodeId, struct GroupDestinationTag>;
using SingleSourceId = TaggedValue<NodeId, struct SingleSourceTag>;
using GroupSourceId = TaggedValue<NodeId, struct GroupSourceTag>;
using MessageId = TaggedValue<uint32_t, struct MessageIdTag>;
using OurEndpoint = TaggedValue<boost::asio::ip::udp::endpoint, struct OurEndpointTag>;
using TheirEndpoint = TaggedValue<boost::asio::ip::udp::endpoint, struct TheirEndpointTag>;
NodeId OurId(NodeId::IdType::kRandomId);
using SerialisedMessage = std::vector<unsigned char>;

// For use with small messages which aren't scatter/gathered
template <typename Destination, typename Source>
struct SmallHeader {
  SmallHeader() = delete;
  SmallHeader(SmallHeader const&) = delete;
  SmallHeader(SmallHeader&&) MAIDSAFE_NOEXCEPT = default;
  SmallHeader(Destination destination_in, Source source_in, MessageId message_id_in,
              crypto::Murmur checksum_in)
      : destination(std::move(destination_in)),
        source(std::move(source_in)),
        message_id(std::move(message_id_in)),
        checksum(std::move(checksum_in)) {}
  ~SmallHeader() = default;
  SmallHeader& operator=(SmallHeader const&) = delete;
  SmallHeader& operator=(SmallHeader&&) MAIDSAFE_NOEXCEPT = default;

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(destination, source, message_id, checksum);
  }

  Destination destination;
  Source source;
  MessageId message_id;
  crypto::Murmur checksum;
};

// For use with scatter/gather messages
template <typename Destination, typename Source>
struct Header {
  Header() = delete;
  Header(Header const&) = delete;
  Header(Header&&) MAIDSAFE_NOEXCEPT = default;
  Header(Destination destination_in, Source source_in, MessageId message_id_in,
         crypto::Murmur payload_checksum_in, crypto::Murmur other_checksum_in)
      : basic_info(std::move(destination_in), std::move(source_in), std::move(message_id_in),
                   std::move(payload_checksum_in)),
        other_checksum(std::move(other_checksum_in)) {}
  ~Header() = default;
  Header& operator=(Header const&) = delete;
  Header& operator=(Header&&) MAIDSAFE_NOEXCEPT = default;

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(basic_info, other_checksums);
  }

  SmallHeader<Destination, Source> basic_info;
  std::array<crypto::Murmur, kGroupSize - 1> other_checksums;
};



struct Ping {
  Ping(SingleDestinationId destination_address)
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
  PingResponse(Ping ping)
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
