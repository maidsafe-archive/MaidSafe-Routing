/*  Copyright 2015 MaidSafe.net limited

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

#include <chrono>

#include "asio/use_future.hpp"

#include "maidsafe/common/utils.h"
#include "maidsafe/common/serialisation/binary_archive.h"
#include "maidsafe/common/serialisation/serialisation.h"

#include "maidsafe/routing/utils.h"
#include "maidsafe/routing/messages/messages.h"

namespace maidsafe {

namespace routing {

Client::Client(asio::io_service& io_service, Identity our_id, asymm::Keys our_keys)
    : crux_asio_service_(1),
      io_service_(io_service),
      our_id_(std::move(our_id)),
      our_keys_(std::move(our_keys)),
      bootstrap_node_(),
      message_id_(RandomUint32()),
      bootstrap_handler_(),
      filter_(std::chrono::minutes(20)),
      sentinel_(io_service) {}

Client::Client(asio::io_service& io_service, const passport::Maid& maid)
    : crux_asio_service_(1),
      io_service_(io_service),
      our_id_(maid.name()->string()),
      our_keys_([&]() -> asymm::Keys {
        asymm::Keys keys;
        keys.private_key = maid.private_key();
        keys.public_key = maid.public_key();
        return keys;
      }()),
      bootstrap_node_(),
      message_id_(RandomUint32()),
      bootstrap_handler_(),
      filter_(std::chrono::minutes(20)),
      sentinel_(io_service) {}

Client::Client(asio::io_service& io_service, const passport::Mpid& mpid)
    : crux_asio_service_(1),
      io_service_(io_service),
      our_id_(mpid.name()->string()),
      our_keys_([&]() -> asymm::Keys {
        asymm::Keys keys;
        keys.private_key = mpid.private_key();
        keys.public_key = mpid.public_key();
        return keys;
      }()),
      bootstrap_node_(),
      message_id_(RandomUint32()),
      bootstrap_handler_(),
      filter_(std::chrono::minutes(20)),
      sentinel_(io_service) {}

void Client::MessageReceived(NodeId /*peer_id*/, std::vector<byte> serialised_message) {
  InputVectorStream binary_input_stream(std::move(serialised_message));
  MessageHeader header;
  MessageTypeTag tag;
  try {
    Parse(binary_input_stream, header, tag);
  }
  catch (const std::exception&) {
    LOG(kError) << "header failure: " << boost::current_exception_diagnostic_information();
    return;
  }

  if (filter_.Check(header.FilterValue()))
    return;  // already seen
  // add to filter as soon as posible
  filter_.Add(header.FilterValue());

  switch (tag) {
    case MessageTypeTag::ConnectResponse:
      HandleMessage(Parse<ConnectResponse>(binary_input_stream));
      break;
    case MessageTypeTag::GetDataResponse:
      HandleMessage(Parse<GetDataResponse>(binary_input_stream));
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

void Client::HandleMessage(ConnectResponse&& /*connect_response*/) {
}

void Client::HandleMessage(GetDataResponse&& /*get_data_response*/) {
}

void Client::HandleMessage(routing::Post&& /*post*/) {
}

void Client::HandleMessage(PostResponse&& /*post_response*/) {
}

// void ConnectionLost(NodeId /* peer */) { /*LostNetworkConnection(peer);*/ }

}  // namespace routing
}  // namespace maidsafe
