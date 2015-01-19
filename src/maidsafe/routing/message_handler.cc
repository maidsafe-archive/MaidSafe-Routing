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

#include "maidsafe/routing/message_handler.h"

#include <utility>
#include <vector>

#include "asio/spawn.hpp"
#include "asio/use_future.hpp"
#include "boost/optional.hpp"
#include "maidsafe/common/crypto.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/log.h"
#include "maidsafe/common/containers/lru_cache.h"
#include "maidsafe/common/serialisation/binary_archive.h"

#include "maidsafe/routing/connection_manager.h"
#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/messages/messages.h"
#include "maidsafe/rudp/managed_connections.h"

namespace maidsafe {

namespace routing {

MessageHandler::MessageHandler(asio::io_service& io_service,
                               rudp::ManagedConnections& managed_connections,
                               ConnectionManager& connection_manager, asymm::Keys& keys)
    : io_service_(io_service),
      rudp_(managed_connections),
      connection_manager_(connection_manager),
      cache_(std::chrono::hours(1)),
      accumulator_(std::chrono::minutes(10), QuorumSize),
      keys_(keys) {}

// reply with details, require to check incoming ID (GetKey)
void MessageHandler::HandleMessage(Connect connect, MessageId message_id) {
  if (!connection_manager_.SuggestNodeToAdd(connect.requester_id))
    return;
  // TODO(dirvine) check public key (co-routine)  :05/01/2015
  // FIXME: the 'connect' local variable will go out of scope and the address
  // will point somewhere wierd (same with message_id).
  // FIXME: The body of the handler will happen inside another thread,
  // and since it (the handler) accesses 'this', it should be wrapped inside a strand.
  rudp_.GetAvailableEndpoints(
      connect.receiver_id,
      [this, &connect, &message_id](asio::error_code error, rudp::EndpointPair endpoint_pair) {
        if (error)
          return;
        auto targets(connection_manager_.GetTarget(connect.requester_id));
        ConnectResponse respond;
        respond.requester_id = connect.requester_id;
        respond.requester_endpoints = connect.requester_endpoints;
        respond.receiver_id = connect.receiver_id;
        assert(connect.receiver_id == connection_manager_.OurId());
        respond.receiver_endpoints = endpoint_pair;


        MessageHeader header(DestinationAddress(connect.requester_id),
                             SourceAddress(std::make_pair(NodeAddress(connection_manager_.OurId()),
                                                          boost::optional<GroupAddress>())),
                             message_id, asymm::Sign(Serialise(respond), keys_.private_key));
        // FIXME: What's this?
        auto dave = Serialise(header);
        // FIXME: Don't block inside handlers, it will block everything with it.
        rudp_.Send(connect.receiver_id,
                   Serialise(header, MessageToTag<ConnectResponse>::value(), respond),
                   asio::use_future).get();
      });
}

void MessageHandler::HandleMessage(ConnectResponse /* connect_response */) {}

void MessageHandler::HandleMessage(FindGroup /*find_group*/) {}

void MessageHandler::HandleMessage(FindGroupResponse /*find_group_reponse*/) {}

void MessageHandler::HandleMessage(GetData /*get_data*/) {}

void MessageHandler::HandleMessage(GetDataResponse /* get_data_response */) {}

void MessageHandler::HandleMessage(PutData /*put_data*/) {}

void MessageHandler::HandleMessage(PutDataResponse /*put_data_response*/) {}

void MessageHandler::HandleMessage(Post /*post*/) {}

SourceAddress MessageHandler::OurSourceAddress() const {
  return std::make_pair(NodeAddress(connection_manager_.OurId()), boost::optional<GroupAddress>());
}

}  // namespace routing

}  // namespace maidsafe
