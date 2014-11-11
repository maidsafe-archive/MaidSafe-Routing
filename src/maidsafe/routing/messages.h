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

#include <cstdint>
#include <vector>

#include "maidsafe/common/config.h"
#include "maidsafe/common/serialisation.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/header.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

enum class message_type_tag : uint16_t {
  kPing,
  kPingResponse,
  kFindGroup,
  kFindGroupResponse,
  kConnect,
  kConnectResponse,
  kVaultMessage,
  kCacheableGet,
  kCacheableGetResponse
};

struct ping {
  using header = small_header<single_destination_id, single_source_id>;

  ping() = default;
  ping(const ping&) = delete;
  ping(ping&&) MAIDSAFE_NOEXCEPT = default;
  ping(single_destination_id destination_in, single_source_id source_in)
      : header(std::move(destination_in), std::move(source_in), message_id(RandomUint32()),
               murmur_hash2(std::vector<byte>{})) {}
  explicit ping(header header_in) : header(std::move(header_in)) {}
  ~ping() = default;
  ping& operator=(const ping&) = delete;
  ping& operator=(ping&&) MAIDSAFE_NOEXCEPT = default;

  template <typename Archive>
  void save(Archive& archive) const {
    archive(header);
  }

  header header;
};

struct ping_response {
  using header = small_header<single_destination_id, single_source_id>;

  ping_response() = default;
  ping_response(const ping_response&) = delete;
  ping_response(ping_response&&) MAIDSAFE_NOEXCEPT = default;
  explicit ping_response(ping ping)
      : header(single_destination_id(std::move(ping.header.source.data)),
               single_source_id(std::move(ping.header.destination.data)), ping.header.message_id,
               ping.header.checksum)) {}
  explicit ping_response(header header_in) : header(std::move(header_in)) {}
  ~ping_response() = default;
  ping_response& operator=(const ping_response&) = delete;
  ping_response& operator=(ping_response&&) MAIDSAFE_NOEXCEPT = default;

  template <typename Archive>
  void save(Archive& archive) const {
    archive(header);
  }

  header header;
};

struct connect {
  using header = small_header<single_destination_id, single_source_id>;

  connect() = default;
  connect(const connect&) = delete;
  connect(connect&&) MAIDSAFE_NOEXCEPT = default;
  connect(single_destination_id destination_in, single_source_id source_in, our_endpoint our_endpoint_in)
      : our_endpoint(std::move(our_endpoint_in)),
        header(std::move(destination_in), std::move(source_in), message_id(RandomUint32()),
               murmur_hash2(std::vector<byte>{})) {}
  explicit connect(header header_in) : header(std::move(header_in)) {}
  ~connect() = default;
  connect& operator=(const connect&) = delete;
  connect& operator=(connect&&) MAIDSAFE_NOEXCEPT = default;


  connect(single_destination_id destinaton_address)
      : our_endpoint(our_endpoint), our_id(OurId), their_id(destinaton_address) {}

  our_endpoint our_endpoint;
  header header;
};

struct connect_response {
  using header = small_header<single_destination_id, single_source_id>;

  connect_response() = default;
  connect_response(const connect_response&) = delete;
  connect_response(connect_response&&) MAIDSAFE_NOEXCEPT = default;
  connect_response(single_destination_id destination_in, single_source_id source_in)
      : header(std::move(destination_in), std::move(source_in), message_id(RandomUint32()),
               murmur_hash2(std::vector<byte>{})) {}
  explicit connect_response(header header_in) : header(std::move(header_in)) {}
  ~connect_response() = default;
  connect_response& operator=(const connect_response&) = delete;
  connect_response& operator=(connect_response&&) MAIDSAFE_NOEXCEPT = default;


  connect_response(connect connect, our_endpoint our_endpoint)
      : our_endpoint(our_endpoint),
        their_endpoint(their_endpoint(connect.our_endpoint)),
        our_id(single_source_id(connect.their_id)),
        their_id(connect.our_id) {}

  our_endpoint our_endpoint;
  their_endpoint their_endpoint;
  single_source_id our_id;
  single_destination_id their_id;
};

struct find_group {};

struct find_group_response {};

struct vault_message {};

struct cacheable_get {};
struct cacheable_get_response {};

using message_map =
    GetMap<Serialisable<SerialisableTypeTag::kPing, ping>,
           Serialisable<SerialisableTypeTag::kPingResponse, ping_response>,
           Serialisable<SerialisableTypeTag::kConnect, connect>,
           Serialisable<SerialisableTypeTag::kConnectResponse, connect_response>,
           Serialisable<SerialisableTypeTag::kFindGroup, find_group>,
           Serialisable<SerialisableTypeTag::kFindGroupResponse, find_group_response>,
           Serialisable<SerialisableTypeTag::kVaultMessage, vault_message>,
           Serialisable<SerialisableTypeTag::kCacheableGet, cacheable_get>,
           Serialisable<SerialisableTypeTag::kCacheableGetResponse, cacheable_get_response>>::Map;

template <SerialisableTypeTag Tag>
using custom_type = typename Find<message_map, Tag>::ResultCustomType;

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TYPES_H_
