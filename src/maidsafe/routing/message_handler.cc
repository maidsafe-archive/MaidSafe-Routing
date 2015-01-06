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
#include "maidsafe/routing/compile_time_mapper.h"

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
      accumulator_(std::chrono::minutes(10)),
      keys_(keys) {}
// reply with details, require to check incoming ID (GetKey)
void MessageHandler::HandleMessage(Connect connect) {
  if (!connection_manager_.SuggestNodeToAdd(connect.requester_id))
    return;
  // TODO(dirvine) check public key (co-routine)  :05/01/2015
  rudp_.GetAvailableEndpoints(
      connect.receiver_id,
      [this, &connect](asio::error_code error, rudp::EndpointPair endpoint_pair) {
        if (error)
          return;
        auto targets(connection_manager_.GetTarget(connect.requester_id));
        ConnectResponse respond;
        respond.requester_id = connect.requester_id;
        respond.requester_endpoints = connect.requester_endpoints;
        respond.receiver_id = connect.receiver_id;
        assert(connect.receiver_id == connection_manager_.OurId());
        respond.receiver_endpoints = endpoint_pair;
        respond.receiver_public_key = keys_.public_key;
        auto data(Serialise(respond));
        auto data_signature(asymm::Sign(data, keys_.private_key));
        MessageHeader header;
        header.message_id = RandomUint32();
        header.signature = boost::optional<rsa::Signature>(data_signature);
        header.source = SourceAddress(connection_manager_.OurId());
        header.destination = DestinationAddress(connect.requester_id);
        header.checksums.push_back(crypto::Hash<crypto::SHA1>(data));
        auto tag = GivenTypeFindTag_v<ConnectResponse>::value;
        rudp_.Send(connect.receiver_id, Serialise(header, tag, respond), asio::use_future).get();
      });
}

void MessageHandler::HandleMessage(ClientConnect client_connect) {
  // if (auto requester_public_key = connection_manager_.GetPublicKey(connect.requestor_id)) {
  //   ClientConnect client_connect{std::move(connect), OurSourceAddress(),
  //   *requester_public_key};
  //   // rudp_.
  // }
  auto& source_id = client_connect.requester_id;
  if (source_id == client_connect.requester_id) {
    LOG(kWarning) << "A peer can't send his own ClientConnect - potential attack attempt.";
    return;
  }

  if (!connection_manager_.SuggestNodeToAdd(source_id))
    return;

  asio::spawn(io_service_, [&](asio::yield_context yield) {
    std::error_code error;
    auto endpoints = rudp_.GetAvailableEndpoints(source_id, yield[error]);
    if (error) {
      LOG(kError) << "Failed to get available endpoints from RUDP: " << error.message();
      return;
    }
    (void)endpoints;
    // if this is a response to our own connect request, we just need to add them to rudp_
    // otherwise we send our own connect request to them and then add them to rudp_.
  });
}

void MessageHandler::HandleMessage(ConnectResponse /* connect_response */) {}

// void MessageHandler::HandleMessage(ClientConnectResponse /* client_connect_response */) {}

void MessageHandler::HandleMessage(FindGroup /*find_group*/) {}

void MessageHandler::HandleMessage(FindGroupResponse /*find_group_reponse*/) {}

void MessageHandler::HandleMessage(GetData /*get_data*/) {}

void MessageHandler::HandleMessage(GetDataResponse /* get_data_response */) {}

void MessageHandler::HandleMessage(PutData /*put_data*/) {}

void MessageHandler::HandleMessage(PutDataResponse /*put_data_response*/) {}

void MessageHandler::HandleMessage(ClientPutData /* client_put_data */) {}

// void MessageHandler::HandleMessage(PutKey /* put_key */) {}

void MessageHandler::HandleMessage(Post /*post*/) {}

void MessageHandler::HandleMessage(ClientPost /* client_post */) {}

void MessageHandler::HandleMessage(Request /* request */) {}
void MessageHandler::HandleMessage(ClientRequest /* client_request */) {}
void MessageHandler::HandleMessage(Response /* response */) {}


SourceAddress MessageHandler::OurSourceAddress() const {
  return SourceAddress{connection_manager_.OurId()};
}

}  // namespace routing

}  // namespace maidsafe
