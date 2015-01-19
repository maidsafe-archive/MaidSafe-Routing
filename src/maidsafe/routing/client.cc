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

#include "maidsafe/routing/messages/messages.h"
#include "maidsafe/routing/messages/messages_fwd.h"
#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {


Client::Client(asio::io_service& io_service, boost::filesystem::path db_location, Identity our_id,
               const asymm::Keys& keys)
    : io_service_(io_service),
      our_id_(our_id),
      keys_(keys),
      rudp_(),
      bootstrap_handler_(std::move(db_location)),
      filter_(std::chrono::minutes(20)),
      accumulator_(std::chrono::minutes(10)) {}

void Client::OnMessageReceived(NodeId peer_id, rudp::ReceivedMessage serialised_message) {
  InputVectorStream binary_input_stream{std::move(serialised_message)};
  MessageHeader header;
  MessageTypeTag tag;
  Parse(binary_input_stream, header, tag);

  if (!header.GetSource().first->IsValid() || header.GetSource().first.data != peer_id) {
    LOG(kError) << "Invalid header.";
    return;
  }

  if (filter_.Check({header.GetSource(), header.GetMessageId()}))
    return;  // already seen
  // add to filter as soon as posible
  filter_.Add({header.GetSource(), header.GetMessageId()});

  if (header.GetChecksum() &&
      crypto::Hash<crypto::SHA1>(binary_input_stream.vector()) != header.GetChecksum()) {
    LOG(kError) << "Checksum failure.";
    return;
  } else if (header.GetSignature()) {
    // TODO(dirvine) get public key and check signature   :08/01/2015
    LOG(kError) << "Signature failure.";
    return;
  } else {
    LOG(kError) << "No checksum or signature - receive aborted.";
    return;
  }


  switch (tag) {
    case MessageTypeTag::Connect:
      HandleMessage(Parse<Connect>(binary_input_stream));
      break;
    case MessageTypeTag::ConnectResponse:
      HandleMessage(
          Parse<ConnectResponse>(binary_input_stream));
      break;
    case MessageTypeTag::GetDataResponse:
      HandleMessage(
          Parse<GetDataResponse>(binary_input_stream));
      break;
    // case MessageTypeTag::PutDataResponse:
    //   HandleMessage(
    //       Parse<PutDataResponse>(binary_input_stream));
    //   break;
    // case MessageTypeTag::Post:
    //   HandleMessage(Parse<Post>(binary_input_stream));
    //   break;
    // case MessageTypeTag::Request:
    //   HandleMessage(Parse<Request>(binary_input_stream));
    //   break;
    // case MessageTypeTag::Response:
    //   HandleMessage(Parse<MessageTypeTag::Response>(binary_input_stream));
    //   break;
    default:
      LOG(kWarning) << "Received message of unknown type.";
      break;
  }
}

void Client::HandleMessage(GetDataResponse /* get_data_response */) {}
// void Client::HandleMessage(PutDataResponse /* put_data */) {}
void Client::HandleMessage(Post /* post */) {}
void Client::HandleMessage(Request /* request */) {}
void Client::HandleMessage(Response /* response */) {}

void Client::MessageReceived(NodeId peer_id, rudp::ReceivedMessage message) {
  OnMessageReceived(peer_id, std::move(message));
}

void ConnectionLost(NodeId /* peer */) { /*LostNetworkConnection(peer);*/
}

}  // namespace routing

}  // namespace maidsafe
