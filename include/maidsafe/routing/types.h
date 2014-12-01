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

namespace routing {

static const size_t GroupSize = 32;
static const size_t QuorumSize = 29;
static const size_t DefaultRoutingTableSize = 64;

using DestinationId = TaggedValue<NodeId, struct DestinationTag>;
using SourceId = TaggedValue<NodeId, struct SourceTag>;
using MessageId = TaggedValue<uint32_t, struct MessageIdTag>;
using Endpoint = boost::asio::ip::udp::endpoint;
using Connection = boost::asio::ip::udp::endpoint;
using byte = unsigned char;
using MurmurHash = uint32_t;
using Checksums = std::array<murmur_hash, group_size - 1>;
using SerialisedMessage = std::vector<unsigned char>;
using CloseGroupDifference = std::pair<std::vector<NodeId>, std::vector<NodeId>>;

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TYPES_H_
