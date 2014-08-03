/*  Copyright 2012 MaidSafe.net limited

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

#include "maidsafe/routing/message_handler.h"

#include <vector>

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"

#include "maidsafe/routing/client_routing_table.h"
#include "maidsafe/routing/message.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/service.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {
template <>
void MessageHandler<ClientNode>::HandleMessage(protobuf::Message& message) {
  LOG(kVerbose) << "[" << connections_.kNodeId() << "]"
                << " MessageHandler<NodeType>::HandleMessage handle message with id: "
                << message.id();
  if (!ValidateMessage(message)) {
    LOG(kWarning) << "Validate message failed， id: " << message.id();
    BOOST_ASSERT_MSG((message.hops_to_live() > 0),
                     "Message has traversed maximum number of hops allowed");
    return;
  }

  // Decrement hops_to_live
  message.set_hops_to_live(message.hops_to_live() - 1);

  // If group message request to self id
  if (IsGroupMessageRequestToSelfId(message)) {
    LOG(kInfo) << "MessageHandler<NodeType>::HandleMessage " << message.id()
               << " HandleGroupMessageToSelfId";
    return HandleGroupMessageToSelfId(message);
  }
  LOG(kInfo) << "MessageHandler<NodeType>::HandleMessage " << message.id()
             << " HandleClientMessage";
  return HandleClientMessage(message);
}

template <>
void MessageHandler<ClientNode>::HandleGroupMessageAsClosestNode(protobuf::Message& /*message*/) {}


}  // namespace routing

}  // namespace maidsafe
