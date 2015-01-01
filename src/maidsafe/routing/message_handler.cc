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

#include "maidsafe/common/log.h"
#include "maidsafe/common/containers/lru_cache.h"
#include "maidsafe/common/serialisation/binary_archive.h"
#include "maidsafe/routing/compile_time_mapper.h"

#include "maidsafe/routing/connection_manager.h"
#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/messages/messages.h"

namespace maidsafe {

namespace routing {

MessageHandler::MessageHandler(asio::io_service& io_service,
                               rudp::ManagedConnections& managed_connections,
                               ConnectionManager& connection_manager)
    : io_service_(io_service),
      rudp_(managed_connections),
      connection_manager_(connection_manager),
      cache_(std::chrono::hours(1)),
      accumulator_(std::chrono::minutes(10)) {
  (void)rudp_;
  (void)connection_manager_;
  (void)io_service_;
  (void)cache_;
  (void)accumulator_;
}

void MessageHandler::HandleMessage(Connect /*connect*/) {
//  if (auto requester_public_key = connection_manager_.GetPublicKey(connect.header.source.data)) {
//    ForwardConnect forward_connect{std::move(connect), OurSourceAddress(), *requester_public_key};
//    // rudp_.
//  }

  //    rudp_.GetNextAvailableEndpoint(
  //        connect_msg.header.source,
  //        [this](maidsafe_error error, rudp::endpoint_pair endpoint_pair) {
  //          if (!error)
  //            auto targets(connection_mgr_.get_target(connect_msg.header.source));
  //          for (const auto& target : targets)
  //            // FIXME (dirvine) Check connect parameters and why no response type 19/11/2014
  //        rudp_.Send(target.id, Serialise(forward_connect());
  //        });
}

void MessageHandler::HandleMessage(ForwardConnect /*forward_connect*/) {
//  auto& source_id = forward_connect.header.source.data;
//  if (source_id == forward_connect.requester.id) {
//    LOG(kWarning) << "A peer can't send his own ForwardConnect - potential attack attempt.";
//    return;
//  }

//  if (!connection_manager_.SuggestNodeToAdd(source_id))
//    return;

//  asio::spawn(io_service_, [&](asio::yield_context yield) {
//    std::error_code error;
//    auto endpoints = rudp_.GetAvailableEndpoints(source_id, yield[error]);
//    if (error) {
//      LOG(kError) << "Failed to get available endpoints from RUDP: " << error.message();
//      return;
//    }
//    (void)endpoints;
//    // if this is a response to our own connect request, we just need to add them to rudp_
//    // otherwise we send our own connect request to them and then add them to rudp_.
//  });
}

void MessageHandler::HandleMessage(ConnectResponse /* connect_response */) {}

//void MessageHandler::HandleMessage(ForwardConnectResponse /* forward_connect_response */) {}

void MessageHandler::HandleMessage(FindGroup /*find_group*/) {}

void MessageHandler::HandleMessage(FindGroupResponse /*find_group_reponse*/) {}

void MessageHandler::HandleMessage(GetData /*get_data*/) {}

void MessageHandler::HandleMessage(GetDataResponse /* get_data_response */) {}

void MessageHandler::HandleMessage(PutData /*put_data*/) {}

void MessageHandler::HandleMessage(PutDataResponse /*put_data_response*/) {}

void MessageHandler::HandleMessage(ForwardPutData /* forward_put_data */) {}

// void MessageHandler::HandleMessage(PutKey /* put_key */) {}

void MessageHandler::HandleMessage(Post /*post*/) {}

void MessageHandler::HandleMessage(ForwardPost /* forward_post */) {}

void MessageHandler::HandleMessage(Request /* request */) {}
void MessageHandler::HandleMessage(ForwardRequest /* forward_request */) {}
void MessageHandler::HandleMessage(Response /* response */) {}


SourceAddress MessageHandler::OurSourceAddress() const {
  return SourceAddress{connection_manager_.OurId()};
}

}  // namespace routing

}  // namespace maidsafe
