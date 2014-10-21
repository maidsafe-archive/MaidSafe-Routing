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

#include "maidsafe/common/serialisation.h"

#ifndef MAIDSAFE_ROUTING_TYPES_H_
#define MAIDSAFE_ROUTING_TYPES_H_

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

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TYPES_H_

