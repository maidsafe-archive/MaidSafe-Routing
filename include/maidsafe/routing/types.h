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
#include <utility>
#include <vector>

#include "boost/optional/optional.hpp"
#include "asio/async_result.hpp"
#include "asio/handler_type.hpp"
#include "asio/ip/udp.hpp"

#include "maidsafe/passport/types.h"
#include "maidsafe/passport/passport.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/tagged_value.h"
#include "maidsafe/rudp/contact.h"
#include "maidsafe/rudp/types.h"

namespace maidsafe {

namespace routing {

static const size_t GroupSize = 16;
#ifdef ZERO_STATE_NODE
static const size_t QuorumSize = 1;
#else
static const size_t QuorumSize = 12;
#endif
using Address = NodeId;
using DataKey = TaggedValue<Identity, struct DataKeyTag>;
using DataValue = TaggedValue<std::vector<byte>, struct DataValueTag>;
using MessageId = uint32_t;
using DestinationAddress = TaggedValue<Address, struct DestinationTag>;
using NodeAddress = TaggedValue<Address, struct NodeTag>;
using GroupAddress = TaggedValue<Address, struct GroupTag>;
using SourceAddress = std::pair<NodeAddress, boost::optional<GroupAddress>>;

template <class Archive>
void serialize(Archive& archive, SourceAddress& source_address) {
  archive(source_address.first, source_address.second);
}

using Endpoint = rudp::Endpoint;
using Port = uint16_t;
using SerialisedMessage = std::vector<byte>;
using Checksum = crypto::SHA1Hash;
using CloseGroupDifference = std::pair<std::vector<Address>, std::vector<Address>>;

template <typename CompletionToken>
using BootstrapHandlerHandler =
    typename asio::handler_type<CompletionToken, void(asio::error_code, rudp::Contact)>::type;

template <typename CompletionToken>
using BootstrapReturn = typename asio::async_result<BootstrapHandlerHandler<CompletionToken>>::type;

template <typename CompletionToken>
using PostHandler = typename asio::handler_type<CompletionToken, void(asio::error_code)>::type;

template <typename CompletionToken>
using PostReturn = typename asio::async_result<PostHandler<CompletionToken>>::type;

template <typename CompletionToken>
using PutHandler = typename asio::handler_type<CompletionToken, void(asio::error_code)>::type;

template <typename CompletionToken>
using PutReturn = typename asio::async_result<PutHandler<CompletionToken>>::type;

template <typename CompletionToken>
using GetHandler =
    typename asio::handler_type<CompletionToken, void(asio::error_code, SerialisedMessage)>::type;

template <typename CompletionToken>
using GetReturn = typename asio::async_result<BootstrapHandlerHandler<CompletionToken>>::type;

template <typename CompletionToken>
using RequestHandler =
    typename asio::handler_type<CompletionToken, void(asio::error_code, SerialisedMessage)>::type;

template <typename CompletionToken>
using RequestReturn = typename asio::async_result<RequestHandler<CompletionToken>>::type;


}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TYPES_H_
