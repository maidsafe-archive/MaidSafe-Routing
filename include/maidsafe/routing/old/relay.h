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

#ifndef MAIDSAFE_ROUTING_RELAY_H_
#define MAIDSAFE_ROUTING_RELAY_H_

#include <algorithm>
#include <string>
#include <utility>

#include "maidsafe/routing/group_source.h"

#include "maidsafe/common/config.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/tagged_value.h"

namespace maidsafe {

namespace routing {



template <typename T>
struct Relay {
  Relay() = default;
  Relay(T node_id_in, NodeId connection_id_in, T relay_node_in)
      : node_id(node_id_in), connection_id(connection_id_in), relay_node(relay_node_in) {}
  Relay(const Relay& other) = default;
  Relay(Relay&& other) MAIDSAFE_NOEXCEPT : node_id(std::move(other.node_id)),
                                           connection_id(std::move(other.connection_id)),

                                           Relay& operator=(const Relay&) = default;

  bool operator==(const Relay<T>& other) const MAIDSAFE_NOEXCEPT {
    return std::tie(node_id, connection_id, relay_node) ==
           std::tie(other.node_id, other.connection_id, other.relay_node);
  }
  // static_assert(is_regular<Relay<T>>::value, "Not a regular type");
  T node_id;             // original source/receiver
  NodeId connection_id;  //  source/receiver's connection id
  T relay_node;          // node relaying messages to/fro on behalf of original sender/receiver
};


using SingleRelaySource = Relay<SingleSource>;
using SingleIdRelay = Relay<SingleId>;

// ==================== Implementation =============================================================

namespace detail {
inline SingleIdRelay GetRelayIdToReply(const SingleRelaySource& single_relay_src) {
  return SingleIdRelay(SingleId(NodeId(single_relay_src.node_id->string())),
                       single_relay_src.connection_id,
                       SingleId(NodeId(single_relay_src.relay_node->string())));
}
}  // namespace detail

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_RELAY_H_
