/*  Copyright 2013 MaidSafe.net limited

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

#ifndef MAIDSAFE_ROUTING_MESSAGE_H_
#define MAIDSAFE_ROUTING_MESSAGE_H_

#include <algorithm>
#include <string>
#include <utility>
#include "maidsafe/routing/relay.h"

#include "maidsafe/common/config.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/tagged_value.h"
#include "maidsafe/common/serialisation.h"

namespace maidsafe {


enum class TypeTag : unsigned char {
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

class Ping;
class PingResponse;
class Connect;
class ConnectResponse;
class FindNode;
class FindNodeResponse;
class FindGroup;
class FindGroupResponse;
class Connect;
class ConnectResponse;
class NodeMessage;
class NodeMessageResponse;

using MAP = GetMap<
    KVPair<TypeTag::kPing, Ping>, KVPair<TypeTag::kPingResponse, PingResponse>,
    KVPair<TypeTag::kConnect, Connect>, KVPair<TypeTag::kConnectResponse, ConnectResponse>,
    KVPair<TypeTag::kFindNode, FindNode>, KVPair<TypeTag::kFindNodeResponse, FindNodeResponse>,
    KVPair<TypeTag::kFindGroup, FindGroup>, KVPair<TypeTag::kFindGroupResponse, FindGroupResponse>,
    KVPair<TypeTag::kNodeMessage, NodeMessage>,
    KVPair<TypeTag::kNodeMessageResponse, NodeMessageResponse>>::MAP;

template <TypeTag Key>
using CustomType = typename Find<MAP, Key>::ResultCustomType;

template <TypeTag T>
CustomType<T> Parse(std::stringstream& ref_binary_stream) {
  CustomType<T> obj_deserialised;

  {
    cereal::BinaryInputArchive input_bin_archive{ref_binary_stream};
    input_bin_archive(obj_deserialised);
  }

  return obj_deserialised;
}



template <typename Sender, typename Receiver>
struct Message {
  Message() = default;
  Message(std::string contents_in, Sender sender_in, Receiver receiver_in,
          Cacheable cacheable_in = Cacheable::kNone)
      : contents(contents_in), sender(sender_in), receiver(receiver_in), cacheable(cacheable_in) {}
  Message(const Message& other) = default;
  Message(Message&& other) MAIDSAFE_NOEXCEPT : contents(std::move(other.contents)),
                                               sender(std::move(other.sender)),
                                               receiver(std::move(other.receiver)),
                                               cacheable(std::move(other.cacheable)) {}
  Message& operator=(const Message&) = default;

  bool operator==(const Message& other) const MAIDSAFE_NOEXCEPT {
    return std::tie(contents, sender, receiver, cacheable) ==
           std::tie(other.contents, other.sender, other.receiver, other.cacheable);
  }
  bool operator!=(const Message& other) const MAIDSAFE_NOEXCEPT { return !operator==(other); }

  bool operator<(const Message& other) const MAIDSAFE_NOEXCEPT {
    return std::tie(contents, sender, receiver, cacheable) <
           std::tie(other.contents, other.sender, other.receiver, other.cacheable);
  }

  // static_assert(is_regular<Message<Sender, Receiver>>::value, "Not a regular type");
  std::string contents;
  Sender sender;
  Receiver receiver;
  Cacheable cacheable;
};

using SingleToSingleMessage = Message<SingleSource, SingleId>;
using SingleToGroupMessage = Message<SingleSource, GroupId>;
using GroupToSingleMessage = Message<GroupSource, SingleId>;
using GroupToGroupMessage = Message<GroupSource, GroupId>;

using SingleToGroupRelayMessage = Message<SingleRelaySource, GroupId>;
using GroupToSingleRelayMessage = Message<GroupSource, SingleIdRelay>;


}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGE_H_
