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

#ifndef MAIDSAFE_ROUTING_MESSAGES_H_
#define MAIDSAFE_ROUTING_MESSAGES_H_

#include <vector>

#include "maidsafe/common/config.h"
#include "maidsafe/common/serialisation.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/header.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

struct Ping {
  using Header = SmallHeader<SingleDestinationId, SingleSourceId>;

  Ping() = default;
  Ping(const Ping&) = delete;
  Ping(Ping&&) MAIDSAFE_NOEXCEPT = default;
  Ping(SingleDestinationId destination_in, SingleSourceId source_in)
      : header(std::move(destination_in), std::move(source_in), MessageId(RandomUint32()),
               MurmurHash2(std::vector<byte>{})) {}
  explicit Ping(Header header_in) : header(std::move(header_in)) {}
  ~Ping() = default;
  Ping& operator=(const Ping&) = delete;
  Ping& operator=(Ping&&) MAIDSAFE_NOEXCEPT = default;

  template <typename Archive>
  void save(Archive& archive) const {
    archive(header);
  }

  Header header;
};

struct PingResponse {
  using Header = SmallHeader<SingleDestinationId, SingleSourceId>;

  PingResponse() = default;
  PingResponse(const PingResponse&) = delete;
  PingResponse(PingResponse&&) MAIDSAFE_NOEXCEPT = default;
  explicit PingResponse(Ping ping)
      : header(SingleDestinationId(std::move(ping.header.source.data)),
               SingleSourceId(std::move(ping.header.destination.data)), ping.header.message_id,
               ping.header.checksum)) {}
  explicit PingResponse(Header header_in) : header(std::move(header_in)) {}
  ~PingResponse() = default;
  PingResponse& operator=(const PingResponse&) = delete;
  PingResponse& operator=(PingResponse&&) MAIDSAFE_NOEXCEPT = default;

  template <typename Archive>
  void save(Archive& archive) const {
    archive(header);
  }

  Header header;
};

struct Connect {
  using Header = SmallHeader<SingleDestinationId, SingleSourceId>;

  Connect() = default;
  Connect(const Connect&) = delete;
  Connect(Connect&&) MAIDSAFE_NOEXCEPT = default;
  Connect(SingleDestinationId destination_in, SingleSourceId source_in, OurEndpoint our_endpoint_in)
      : our_endpoint(std::move(our_endpoint_in)),
        header(std::move(destination_in), std::move(source_in), MessageId(RandomUint32()),
               MurmurHash2(std::vector<byte>{})) {}
  explicit Connect(Header header_in) : header(std::move(header_in)) {}
  ~Connect() = default;
  Connect& operator=(const Connect&) = delete;
  Connect& operator=(Connect&&) MAIDSAFE_NOEXCEPT = default;


  Connect(SingleDestinationId destinaton_address)
      : our_endpoint(our_endpoint), our_id(OurId), their_id(destinaton_address) {}

  OurEndpoint our_endpoint;
  Header header;
};

struct ConnectResponse {
  using Header = SmallHeader<SingleDestinationId, SingleSourceId>;

  ConnectResponse() = default;
  ConnectResponse(const ConnectResponse&) = delete;
  ConnectResponse(ConnectResponse&&) MAIDSAFE_NOEXCEPT = default;
  ConnectResponse(SingleDestinationId destination_in, SingleSourceId source_in)
      : header(std::move(destination_in), std::move(source_in), MessageId(RandomUint32()),
               MurmurHash2(std::vector<byte>{})) {}
  explicit ConnectResponse(Header header_in) : header(std::move(header_in)) {}
  ~ConnectResponse() = default;
  ConnectResponse& operator=(const ConnectResponse&) = delete;
  ConnectResponse& operator=(ConnectResponse&&) MAIDSAFE_NOEXCEPT = default;


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

struct VaultMessage;

struct CacheableGet;
struct CacheableGetResponse;

using MessageMap =
    GetMap<Serialisable<SerialisableTypeTag::kPing, Ping>,
           Serialisable<SerialisableTypeTag::kPingResponse, PingResponse>,
           Serialisable<SerialisableTypeTag::kConnect, Connect>,
           Serialisable<SerialisableTypeTag::kConnectResponse, ConnectResponse>,
           Serialisable<SerialisableTypeTag::kFindGroup, FindGroup>,
           Serialisable<SerialisableTypeTag::kFindGroupResponse, FindGroupResponse>,
           Serialisable<SerialisableTypeTag::kVaultMessage, VaultMessage>,
           Serialisable<SerialisableTypeTag::kCacheableGet, CacheableGet>,
           Serialisable<SerialisableTypeTag::kCacheableGetResponse, CacheableGetResponse>>::Map;

template <SerialisableTypeTag Tag>
using CustomType = typename Find<MessageMap, Tag>::ResultCustomType;

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TYPES_H_
