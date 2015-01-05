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

#include "maidsafe/routing/client.h"

#include <utility>
#include "asio/use_future.hpp"
#include "asio/io_service.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/expected/expected.hpp"

#include "maidsafe/common/serialisation/binary_archive.h"
#include "maidsafe/common/serialisation/serialisation.h"
#include "maidsafe/common/types.h"

#include "maidsafe/routing/compile_time_mapper.h"
#include "maidsafe/routing/messages/messages.h"
#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {


Client::Client(asio::io_service& io_service, boost::filesystem::path db_location, Identity our_id,
               const asymm::Keys& keys, std::shared_ptr<Listener> listener_ptr)
    : node_ptr_(shared_from_this()),
      io_service_(io_service),
      our_id_(our_id),
      keys_(keys),
      rudp_(),
      bootstrap_handler_(std::move(db_location)),
      connection_manager_(io_service, rudp_, our_id_),
      rudp_listener_(std::make_shared<RudpListener>(node_ptr_)),
      listener_ptr_(listener_ptr),
      message_handler_(io_service, rudp_, connection_manager_),
      filter_(std::chrono::minutes(20)),
      accumulator_(std::chrono::minutes(10)) {}

void Client::OnMessageReceived(rudp::ReceivedMessage&& serialised_message, NodeId peer_id) {
  try {
    InputVectorStream binary_input_stream{std::move(serialised_message)};
    MessageHeader header;
    MessageTypeTag tag;
    Parse(binary_input_stream, header, tag);
    if (!header.source->IsValid()) {
      LOG(kError) << "Invalid header.";
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
    }
    if (header.destination != our_id_) {
      LOG(kError) << "Message received not for us";
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::unable_to_handle_request));
    }

    if (crypto::Hash<crypto::SHA1>(binary_input_stream.vector()) != header.checksums.front()) {
      LOG(kError) << "Checksum failure.";
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
    }

    if (filter_.Check({header.source, header.message_id}))
      return;  // already seen
               // add to filter as soon as posible
    filter_.Add({header.source, header.message_id});

    switch (tag) {
      case MessageTypeTag::Connect:
        message_handler_.HandleMessage(
            Parse<GivenTagFindType_t<MessageTypeTag::Connect>>(binary_input_stream));
        break;
      case MessageTypeTag::ConnectResponse:
        message_handler_.HandleMessage(
            Parse<GivenTagFindType_t<MessageTypeTag::ConnectResponse>>(binary_input_stream));
        break;
      case MessageTypeTag::GetDataResponse:
        message_handler_.HandleMessage(
            Parse<GivenTagFindType_t<MessageTypeTag::GetDataResponse>>(binary_input_stream));
        break;
      case MessageTypeTag::PutDataResponse:
        message_handler_.HandleMessage(
            Parse<GivenTagFindType_t<MessageTypeTag::PutDataResponse>>(binary_input_stream));
        break;
      case MessageTypeTag::Post:
        message_handler_.HandleMessage(
            Parse<GivenTagFindType_t<MessageTypeTag::Post>>(binary_input_stream));
        break;
      case MessageTypeTag::Request:
        message_handler_.HandleMessage(
            Parse<GivenTagFindType_t<MessageTypeTag::Request>>(binary_input_stream));
        break;
      case MessageTypeTag::Response:
        message_handler_.HandleMessage(
            Parse<GivenTagFindType_t<MessageTypeTag::Response>>(binary_input_stream));
        break;
      default:
        LOG(kWarning) << "Received message of unknown type.";
        break;
    }
  } catch (const std::exception& e) {
    LOG(kWarning) << "Exception while handling incoming message: "
                  << boost::diagnostic_information(e);
  }
}

void HandleMessage(GetDataResponse /* get_data_response */) {}
void HandleMessage(PutDataResponse /* put_data */) {}
void HandleMessage(Post /* post */) {}
void HandleMessage(Request /* request */) {}
void HandleMessage(Response /* response */) {}

void Client::RudpListener::MessageReceived(NodeId peer_id, rudp::ReceivedMessage message) {
  node_ptr_->OnMessageReceived(std::move(message), peer_id);
}

void Client::RudpListener::ConnectionLost(NodeId peer) {
  node_ptr_->connection_manager_.LostNetworkConnection(peer);
}

}  // namespace routing

}  // namespace maidsafe
