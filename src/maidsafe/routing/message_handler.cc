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

#include "maidsafe/common/log.h"
#include "maidsafe/common/containers/lru_cache.h"
#include "maidsafe/common/serialisation/binary_archive.h"
#include "maidsafe/common/serialisation/compile_time_mapper.h"

#include "maidsafe/routing/connect.h"
#include "maidsafe/routing/connect_response.h"
#include "maidsafe/routing/connection_manager.h"
#include "maidsafe/routing/find_group.h"
#include "maidsafe/routing/find_group_response.h"
#include "maidsafe/routing/get_data.h"
#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/ping.h"
#include "maidsafe/routing/ping_response.h"
#include "maidsafe/routing/post.h"
#include "maidsafe/routing/put_data.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

namespace {

using MessageMap = GetMap<Ping, PingResponse, FindGroup, FindGroupResponse, Connect,
                          ConnectResponse, GetData, PutData, Post>::Map;

std::pair<MessageHeader, SerialisableTypeTag> ParseHeaderAndTypeEnum(
    InputVectorStream& binary_input_stream) {
  auto result = std::make_pair(MessageHeader{}, SerialisableTypeTag{});
  {
    BinaryInputArchive binary_input_archive(binary_input_stream);
    binary_input_archive(result.first, result.second);
  }
  return result;
}

template <typename MessageType>
MessageType Parse(MessageHeader header, InputVectorStream& binary_input_stream) {
  MessageType parsed_message(std::move(header));
  {
    BinaryInputArchive binary_input_archive(binary_input_stream);
    binary_input_archive(parsed_message);
  }
  return parsed_message;
}

}  // unnamed namespace

MessageHandler::MessageHandler(asio::io_service& io_service,
                               rudp::ManagedConnections& managed_connections,
                               ConnectionManager& connection_manager)
    : io_service_(io_service), rudp_(managed_connections), connection_manager_(connection_manager) {
  (void)io_service_;
  (void)rudp_;
  (void)connection_manager_;
}

void MessageHandler::OnMessageReceived(rudp::ReceivedMessage&& serialised_message) {
  try {
    InputVectorStream binary_input_stream{std::move(serialised_message)};
    auto header_and_type_enum(ParseHeaderAndTypeEnum(binary_input_stream));
    switch (header_and_type_enum.second) {
      case Ping::kSerialisableTypeTag:
        HandleMessage(Parse<Ping>(std::move(header_and_type_enum.first), binary_input_stream));
        break;
      case PingResponse::kSerialisableTypeTag:
        HandleMessage(
            Parse<PingResponse>(std::move(header_and_type_enum.first), binary_input_stream));
        break;
      case FindGroup::kSerialisableTypeTag:
        HandleMessage(Parse<FindGroup>(std::move(header_and_type_enum.first), binary_input_stream));
        break;
      case FindGroupResponse::kSerialisableTypeTag:
        HandleMessage(
            Parse<FindGroupResponse>(std::move(header_and_type_enum.first), binary_input_stream));
        break;
      case Connect::kSerialisableTypeTag:
        HandleMessage(Parse<Connect>(std::move(header_and_type_enum.first), binary_input_stream));
        break;
      case ConnectResponse::kSerialisableTypeTag:
        HandleMessage(
            Parse<ConnectResponse>(std::move(header_and_type_enum.first), binary_input_stream));
        break;
      case GetData::kSerialisableTypeTag:
        HandleMessage(Parse<GetData>(std::move(header_and_type_enum.first), binary_input_stream));
        break;
      case PutData::kSerialisableTypeTag:
        HandleMessage(Parse<PutData>(std::move(header_and_type_enum.first), binary_input_stream));
        break;
      case Post::kSerialisableTypeTag:
        HandleMessage(Parse<Post>(std::move(header_and_type_enum.first), binary_input_stream));
        break;
      default:
        LOG(kWarning) << "Received message of unknown type.";
        break;
    }
  } catch (const std::exception& e) {
    LOG(kWarning) << "Exception while handling incoming message: "
                  << boost::diagnostic_information(e);
  }

  // auto message(Parse<TypeFromMessage>(serialised_message) > (serialised_message));
  //// FIXME (dirvine) Check firewall 19/11/2014
  // HandleMessage(message);
  //// FIXME (dirvine) add to firewall 19/11/2014
}

void MessageHandler::HandleMessage(Ping&& /*ping*/) {
  // ping is a single destination message
  // if (ping_msg.header.destination.data() == connection_mgr_.OurId()) {
  //  auto targets(connection_mgr_.get_target(ping_msg.header.source.data()));
  //  for (const auto& target : targets)
  //    rudp_.Send(target.id, Serialise(PingResponse(ping_msg)));
  //}
  // else {  // scatter
  //  auto targets(connection_mgr_.get_target(ping_msg.header.destination.data()));
  //  for (const auto& target : targets)
  //    rudp_.Send(target.id, Serialise(PingResponse(ping_msg)));
  //  else rudp_.Send(target.id, Serialise(ping_msg));
  //}
}

void MessageHandler::HandleMessage(PingResponse&& /*ping_response*/) {
  // TODO(dirvine): 2014-11-19 FIXME set a future or some async return type in node or
  // client
}

void MessageHandler::HandleMessage(FindGroup&& /*find_group*/) {}

void MessageHandler::HandleMessage(FindGroupResponse&& /*find_group_reponse*/) {}

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

void MessageHandler::HandleMessage(ConnectResponse&& /*connect_response*/) {}

void MessageHandler::HandleMessage(GetData&& /*get_data*/) {}

void MessageHandler::HandleMessage(PutData&& /*put_data*/) {}

void MessageHandler::HandleMessage(Post&& /*post*/) {}

}  // namespace routing

}  // namespace maidsafe
