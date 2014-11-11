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
#include "maidsafe/common/tagged_value.h"

namespace maidsafe {

enum class SerialisableTypeTag : unsigned char {
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

namespace routing {

static const size_t group_size = 32;
static const size_t quorum_size = 29;
static const size_t default_routing_table_size = 64;

using single_destination_id = TaggedValue<NodeId, struct SingleDestinationTag>;
using group_destination_id = TaggedValue<NodeId, struct GroupDestinationTag>;
using single_source_id = TaggedValue<NodeId, struct SingleSourceTag>;
using group_source_id = TaggedValue<NodeId, struct GroupSourceTag>;
using message_id = TaggedValue<uint32_t, struct MessageIdTag>;
using our_endpoint = TaggedValue<boost::asio::ip::udp::endpoint, struct OurEndpointTag>;
using their_endpoint = TaggedValue<boost::asio::ip::udp::endpoint, struct TheirEndpointTag>;
using byte = unsigned char;
using murmur_hash = uint32_t;
using checksums = std::array<murmur_hash, group_size - 1>;
using serialised_message = std::vector<unsigned char>;

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TYPES_H_
