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

#include "maidsafe/routing/vault_node.h"

#include <utility>

#include "maidsafe/common/serialisation/binary_archive.h"
#include "maidsafe/common/serialisation/compile_time_mapper.h"
#include "maidsafe/common/serialisation/serialisation.h"

#include "maidsafe/routing/messages/messages.h"
#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

namespace {

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

VaultNode::VaultNode(asio::io_service& io_service, boost::filesystem::path db_location,
                     const passport::Pmid& pmid, std::shared_ptr<Listener> listener_ptr)
    : io_service_(io_service),
      our_id_(pmid.name().value.string()),
      keys_([&pmid]() -> asymm::Keys {
        asymm::Keys keys;
        keys.private_key = pmid.private_key();
        keys.public_key = pmid.public_key();
        return keys;
      }()),
      rudp_(),
      bootstrap_handler_(std::move(db_location)),
      connection_manager_(io_service, rudp_, our_id_,
                          [this](CloseGroupDifference close_group_difference) {
        OnCloseGroupChanged(std::move(close_group_difference));
      }),
      rudp_listener_(std::make_shared<RudpListener>()),
      message_handler_listener_(std::make_shared<MessageHandlerListener>()),
      listener_ptr_(listener_ptr),
      message_handler_(io_service, rudp_, connection_manager_, message_handler_listener_),
      filter_(std::chrono::minutes(20)) {}

void VaultNode::OnMessageReceived(rudp::ReceivedMessage&& serialised_message) {
  try {
    InputVectorStream binary_input_stream{std::move(serialised_message)};
    auto header_and_type_enum(ParseHeaderAndTypeEnum(binary_input_stream));
    switch (header_and_type_enum.second) {
      case Join::kSerialisableTypeTag:
        message_handler_.HandleMessage(
            Parse<Join>(std::move(header_and_type_enum.first), binary_input_stream));
        break;
      case JoinResponse::kSerialisableTypeTag:
        message_handler_.HandleMessage(
            Parse<JoinResponse>(std::move(header_and_type_enum.first), binary_input_stream));
        break;
      case Connect::kSerialisableTypeTag:
        message_handler_.HandleMessage(
            Parse<Connect>(std::move(header_and_type_enum.first), binary_input_stream));
        break;
      case ForwardConnect::kSerialisableTypeTag:
        message_handler_.HandleMessage(
            Parse<ForwardConnect>(std::move(header_and_type_enum.first), binary_input_stream));
        break;
      case FindGroup::kSerialisableTypeTag:
        message_handler_.HandleMessage(
            Parse<FindGroup>(std::move(header_and_type_enum.first), binary_input_stream));
        break;
      case FindGroupResponse::kSerialisableTypeTag:
        message_handler_.HandleMessage(
            Parse<FindGroupResponse>(std::move(header_and_type_enum.first), binary_input_stream));
        break;
      case GetData::kSerialisableTypeTag:
        message_handler_.HandleMessage(
            Parse<GetData>(std::move(header_and_type_enum.first), binary_input_stream));
        break;
      case GetDataResponse::kSerialisableTypeTag:
        message_handler_.HandleMessage(
            Parse<GetDataResponse>(std::move(header_and_type_enum.first), binary_input_stream));
        break;
      case PutData::kSerialisableTypeTag:
        message_handler_.HandleMessage(
            Parse<PutData>(std::move(header_and_type_enum.first), binary_input_stream));
        break;
      case PutDataResponse::kSerialisableTypeTag:
        message_handler_.HandleMessage(
            Parse<PutDataResponse>(std::move(header_and_type_enum.first), binary_input_stream));
        break;
      case Post::kSerialisableTypeTag:
        message_handler_.HandleMessage(
            Parse<routing::Post>(std::move(header_and_type_enum.first), binary_input_stream));
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

}  // namespace routing

}  // namespace maidsafe
