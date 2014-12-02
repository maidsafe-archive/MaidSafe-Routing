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

#include "maidsafe/common/utils.h"
#include "maidsafe/common/config.h"
#include "maidsafe/common/serialisation.h"

#include "maidsafe/routing/header.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

enum class message_type_tag : uint16_t {
  ping,
  ping_response,
  find_group,
  find_group_response,
  connect,
  forward_connect,
  vault_message,
  cacheable_get,
  cacheable_get_response
};

struct ping {
  static const message_type_tag message_type = message_type_tag::ping;

  ping() = default;
  ping(const ping&) = delete;
  ping(ping&& other) MAIDSAFE_NOEXCEPT : header(std::move(other.header)) {}
  ping(DestinationAddress destination_in, SourceAddress source_in)
      : header(std::move(destination_in), std::move(source_in), message_id(RandomUint32())) {}
  explicit ping(header header_in) : header(std::move(header_in)) {}
  ~ping() = default;
  ping& operator=(const ping&) = delete;
  ping& operator=(ping&& other) MAIDSAFE_NOEXCEPT {
    header = std::move(other.header);
    return *this;
  };

  template <typename Archive>
  void Serialise(Archive& archive) const {
    archive(header);
  }

  header header;
};

struct ping_response {
  static const message_type_tag message_type = message_type_tag::ping_response;

  ping_response() = default;
  ping_response(const ping_response&) = delete;
  ping_response(ping_response&& other) MAIDSAFE_NOEXCEPT : header(std::move(other.header)) {}
  explicit ping_response(ping ping)
      : header(DestinationAddress(std::move(ping.header.source.data)),
               SourceAddress(std::move(ping.header.destination.data)),
               message_id(std::move(ping.header.message_id))) {}
  ~ping_response() = default;
  ping_response& operator=(const ping_response&) = delete;
  ping_response& operator=(ping_response&& other) MAIDSAFE_NOEXCEPT {
    header = std::move(other.header);
    return *this;
  };

  template <typename Archive>
  void Serialise(Archive& archive) const {
    archive(header);
  }

  header header;
};

struct connect {
  static const message_type_tag message_type = message_type_tag::connect;

  connect() = default;
  connect(const connect&) = delete;
  connect(connect&& other) MAIDSAFE_NOEXCEPT : our_endpoints(std::move(other.our_endpoints)),
                                               header(std::move(other.header)) {}
  connect(DestinationAddress destination_in, SourceAddress source_in, rudp::endpoint_pair our_endpoint_in)
      : our_endpoints(std::move(our_endpoint_in)),
        header(std::move(destination_in), std::move(source_in), message_id(RandomUint32())) {}
  ~connect() = default;
  connect& operator=(const connect&) = delete;
  connect& operator=(connect&& other) MAIDSAFE_NOEXCEPT {
    our_endpoints = std::move(other.our_endpoints);
    header = std::move(other.header);
    return *this;
  };

  template <typename Archive>
  void Serialise(Archive& archive) const {
    archive(header);
  }

  rudp::endpoint_pair our_endpoints;
  header header;
};

struct forward_connect {
  static const message_type_tag message_type = message_type_tag::forward_connect;

  forward_connect() = default;
  forward_connect(const forward_connect&) = delete;
  forward_connect(forward_connect&& other) MAIDSAFE_NOEXCEPT
      : requesters_endpoint(std::move(other.requesters_endpoint)),
        requesters_nat_type(std::move(other.requesters_nat_type)),
        requesters_public_key(std::move(other.requesters_public_key)),
        header(std::move(other.header)) {}
  explicit forward_connect(header header_in)
      : header(std::move(header_in)),
        requesters_endpoint(),
        requesters_nat_type(rudp::NatType::kUnknown),
        requesters_public_key() {}
  ~forward_connect() = default;
  forward_connect& operator=(const forward_connect&) = delete;
  forward_connect& operator=(forward_connect&& other) MAIDSAFE_NOEXCEPT {
    requesters_endpoint = std::move(other.requesters_endpoint);
    requesters_nat_type = std::move(other.requesters_nat_type);
    requesters_public_key = std::move(other.requesters_public_key);
    header = std::move(other.header);
    return *this;
  };

  forward_connect(SourceAddress source_in, connect connect, asymm::PublicKey requesters_public_key_in)
      : requesters_endpoint(std::move(connect.our_endpoint)),
        requesters_nat_type(std::move(connect.our_nat_type)),
        requesters_public_key(std::move(requesters_public_key_in)),
        header(single_DestinationAddress(connect.their_id), std::move(source_in),    // FIXME calculate
               connect.header.message_id, murmur_hash2(std::vector<byte>{})) {}  // hash proerly

  rudp::endpoint_pair requesters_endpoint;
  rudp::NatType requesters_nat_type;
  asymm::PublicKey requesters_public_key;
  header header;
};
// messages from a client are forwarded as is as well as being passed up. Upper layers
// can act on this part as a whole chunk/message by multiplying size by group_size. If message from
// a group (vault) then they are
// synchronised (accumulated / re-constituted) and passed up.
struct vault_message {
  vault_message() = default;
  vault_message(vault_message const&) = default;
  vault_message(vault_message&&) = default MAIDSAFE_NOEXCEPT : header(std::move(rhs.header)),
  checksums(std::move(rhs.checksums)), close_group(std::move(rhs.close_group)) {}
  ~vault_message() = default;
  vault_message& operator=(vault_message const&) = default;
  vault_message& operator=(vault_message&& rhs) MAIDSAFE_NOEXCEPT {
    header = std::move(rhs.header), checksums = std::move(rhs.checksums),
    close_group = std::move(rhs.close_group)
  }

  bool operator==(const vault_message& other) const {
    return std::tie(header, checksums, close_group) ==
           std::tie(other.header, other.checksums, other.close_group);
  }
  bool operator!=(const vault_message& other) const { return !operator==(*this, other); }
  bool operator<(const vault_message& other) const {
    return std::tie(header, checksums, close_group) <
           std::tie(other.header, other.checksums, other.close_group);
  }
  bool operator>(const vault_message& other) { return operator<(other, *this); }
  bool operator<=(const vault_message& other) { return !operator>(*this, other); }
  bool operator>=(const vault_message& other) { return !operator<(*this, other); }

  header header{};
  std::vector<checksum> checksums{};
  std::vector<node_info> close_group{};
};

struct cacheable_get {
  static const message_type_tag message_type = message_type_tag::cacheable_get;
};

struct cacheable_get_response {
  static const message_type_tag message_type = message_type_tag::cacheable_get_response;
};

using message_map =
    GetMap<Serialisable<message_type_tag::ping, ping>,
           Serialisable<message_type_tag::ping_response, ping_response>,
           Serialisable<message_type_tag::connect, connect>,
           Serialisable<message_type_tag::forward_connect, forward_connect>,
           Serialisable<message_type_tag::find_group, find_group>,
           Serialisable<message_type_tag::find_group_response, find_group_response>,
           Serialisable<message_type_tag::vault_message, vault_message>,
           Serialisable<message_type_tag::cacheable_get, cacheable_get>,
           Serialisable<message_type_tag::cacheable_get_response, cacheable_get_response>>::Map;

template <message_type_tag Tag>
using custom_type = typename Find<message_map, Tag>::ResultCustomType;

template <message_type_tag eTypeTag>
custom_type<eTypeTag> Parse(std::stringstream& ref_binary_stream) {
  custom_type<eTypeTag> obj_deserialised;

  {
    cereal::BinaryInputArchive input_bin_archive{ref_binary_stream};
    input_bin_archive(obj_deserialized);
  }

  return obj_deserialized;
}
}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TYPES_H_
