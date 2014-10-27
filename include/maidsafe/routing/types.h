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
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/tagged_value.h"
#include "maidsafe/common/serialisation.h"

#ifndef MAIDSAFE_ROUTING_TYPES_H_
#define MAIDSAFE_ROUTING_TYPES_H_

namespace maidsafe {
using SingleDestination = TaggedValue<NodeId, struct singledestination>;
using GroupDestination = TaggedValue<NodeId, struct groupdestination>;
using SingleSource = TaggedValue<NodeId, struct singlesource>;
using GroupSource = TaggedValue<NodeId, struct groupsource>;
using OurEndPoint = TaggedValue<boost::asio::ip::udp::endpoint, struct ourendpoint>;
using TheirEndPoint = TaggedValue<boost::asio::ip::udp::endpoint, struct theirendpoint>;

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


struct Ping {
  Ping(SingleDestination destination_address)
      : source_address(kNodeID),
        destinaton_address(destinaton_address),
        message_id(RandomUint32()) {}
  Ping(SerialisedPing);  // actually ping type parsed but means serialising again ??
  void operator() {
    if (destination_address == kNodeId) {
      // rudp.send(DestinationEndpoint, PingResponse(*this).serialise());
    } else {
      // routing_table.closest_node(destinaton_address)
      // check for a closer node than us or drop/return error
      // rudp.send(????)
      // rt copy with manipulation free functions
    }
  }

  SingleSource source_address;
  SingleDestination destinaton_address;
  uint32_t message_id;
};

struct PingResponse {
  PingResponse(Ping ping)
      : source_address(kNodeID),
        destinaton_address(SingleDestination(ping.source_address)),
        message_id(ping.message_id) {}

  SingleSource source_address;
  SingleDestination destinaton_address;
  uint32_t message_id;
};

struct Connect {
  Connect(SingleDestination destinaton_address, OurEndPoint our_endpoint)
      : our_endpoint(ourendpoint), our_id(kNodeId), their_id(destinaton_address) {}

  OurEndPoint our_endpoint;
  SingleSource our_id;
  SingleDestination their_id;
}

struct ConnectResponse {
  ConnectResponse(Connect connect, OurEndPoint our_endpoint)
      : our_endpoint(our_endpoint),
        their_endpoint(TheirEndPoint(connect.our_endpoint)),
        our_id(SingleSource(connect.their_id)),
        their_id(connect.our_id) {}

  OurEndPoint our_endpoint;
  TheirEndPoint their_endpoint;
  SingleSource our_id;
  SingleDestination their_id;
};

struct FindGroup {
}

struct FindGroupResponse {
}

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
