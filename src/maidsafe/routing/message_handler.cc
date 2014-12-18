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

#include "asio/use_future.hpp"

#include "maidsafe/common/log.h"
#include "maidsafe/common/containers/lru_cache.h"
#include "maidsafe/common/serialisation/binary_archive.h"
#include "maidsafe/common/serialisation/compile_time_mapper.h"

#include "maidsafe/routing/connection_manager.h"
#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/messages/messages.h"

namespace maidsafe {

namespace routing {

namespace {

using MessageMap = GetMap<Join, JoinResponse, Connect, ForwardConnect, FindGroup, FindGroupResponse,
                          GetData, PutData, Post>::Map;

}  // unnamed namespace

MessageHandler::MessageHandler(asio::io_service& io_service,
                               rudp::ManagedConnections& managed_connections,
                               ConnectionManager& connection_manager,
                               std::shared_ptr<Listener> listener)
    : io_service_(io_service),
      rudp_(managed_connections),
      connection_manager_(connection_manager),
      cache_(std::chrono::hours(1)),
      accumulator_(std::chrono::minutes(10)),
      listener_(listener) {
  (void)io_service_;
  (void)cache_;
  (void)accumulator_;
}

void MessageHandler::HandleMessage(Join&& /*join*/) {}

void MessageHandler::HandleMessage(JoinResponse&& /*join_response*/) {}

void MessageHandler::HandleMessage(Connect&& /*connect*/) {
  // if (connect_msg.header.destination.data() == connection_mgr_.OurId()) {
  //  if (connection_mgr_.suggest_node(connect_msg.header.source)) {
  //    rudp_.GetNextAvailableEndpoint(
  //        connect_msg.header.source,
  //        [this](maidsafe_error error, rudp::endpoint_pair endpoint_pair) {
  //          if (!error)
  //            auto targets(connection_mgr_.get_target(connect_msg.header.source));
  //          for (const auto& target : targets)
  //            // FIXME (dirvine) Check connect parameters and why no response type 19/11/2014
  //        rudp_.Send(target.id, Serialise(forward_connect());
  //        });
  //  }
  //} else {
  //  auto targets(connection_mgr_.get_target(connect_msg.header.destination.data()));
  //  for (const auto& target : targets)
  //    rudp_.Send(target.id, Serialise(Connect(connect_msg)));
  //}
}

void MessageHandler::HandleMessage(ForwardConnect&& /*forward_connect*/) {}

void MessageHandler::HandleMessage(FindGroup&& /*find_group*/) {}

void MessageHandler::HandleMessage(FindGroupResponse&& /*find_group_reponse*/) {}

void MessageHandler::HandleMessage(GetData&& /*get_data*/) {}

void MessageHandler::HandleMessage(GetDataResponse&& /*get_data_response*/) {}

void MessageHandler::HandleMessage(PutData&& /*put_data*/) {}

void MessageHandler::HandleMessage(PutDataResponse&& /*put_data_response*/) {}

void MessageHandler::HandleMessage(Post&& /*post*/) {}

}  // namespace routing

}  // namespace maidsafe
