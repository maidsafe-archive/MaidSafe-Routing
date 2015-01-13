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

#include "maidsafe/routing/routing_node.h"

#include <utility>
#include "asio/use_future.hpp"
#include "boost/exception/diagnostic_information.hpp"
#include "maidsafe/common/serialisation/binary_archive.h"
#include "maidsafe/common/serialisation/serialisation.h"

#include "maidsafe/routing/compile_time_mapper.h"
#include "maidsafe/routing/messages/messages.h"
#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {


RoutingNode::RoutingNode(asio::io_service& io_service, boost::filesystem::path db_location,
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
      connection_manager_(io_service, rudp_, our_id_),
      listener_ptr_(listener_ptr),
      message_handler_(io_service, rudp_, connection_manager_, keys_),
      filter_(std::chrono::minutes(20)),
      accumulator_(std::chrono::minutes(10)),
      cache_(std::chrono::minutes(10)) {}

void RoutingNode::MessageReceived(NodeId peer_id, rudp::ReceivedMessage serialised_message) {
  InputVectorStream binary_input_stream{std::move(serialised_message)};
  MessageHeader header;
  MessageTypeTag tag;
  try {
    Parse(binary_input_stream, header, tag);
  } catch (std::exception& e) {
    LOG(kError) << "header failure." << boost::current_exception_diagnostic_information(true);
    return;
  }
  if (header.IsDirect() && peer_id != header.GetSource().first.data) {
    LOG(kError) << "header failure.";
    return;
  }

  if (filter_.Check({header.NetworkAddressablElement(), header.GetMessageId()}))
    return;  // already seen
  // add to filter as soon as posible
  filter_.Add({header.NetworkAddressablElement(), header.GetMessageId()});

  // We add these to cache
  if (tag == MessageTypeTag::GetDataResponse) {
    auto data = Parse<GivenTagFindType_t<MessageTypeTag::GetDataResponse>>(binary_input_stream);
    cache_.Add(data.key, data.data);
  }
  // if we can satisfy request from cache we do
  if (tag == MessageTypeTag::GetData) {
    auto data = Parse<GivenTagFindType_t<MessageTypeTag::GetData>>(binary_input_stream);
    auto test = cache_.Get(data.key);
    if (test) {
      GetDataResponse response(data.key, test.value());
      if (data.relay_node)
        response.relay_node = data.relay_node;
      for (auto& header : CreateHeaders(data.key, crypto::Hash<crypto::SHA1>(data.key.string()),
                                        header.GetMessageId())) {
        for (const auto& target : connection_manager_.GetTarget(header.GetDestination()))
          rudp_.Send(target.id, Serialise(header, MessageTypeTag::GetDataResponse, response),
                     asio::use_future).get();
      }
      return;
    }
  }

  // send to next node(s) even our close group (swarm mode)
  for (const auto& target : connection_manager_.GetTarget(header.GetDestination()))
    rudp_.Send(target.id, serialised_message, asio::use_future).get();
  // FIXME(dirvine this could be an unauthenticated put or get `12/01/2015
  if (!connection_manager_.AddressInCloseGroupRange(header.GetDestination()))
    return;  // not for us

  if (header.GetChecksum() &&
      crypto::Hash<crypto::SHA1>(binary_input_stream.vector()) != header.GetChecksum()) {
    LOG(kError) << "Checksum failure.";
    return;
  }

  if (header.GetSignature()) {
    // TODO(dirvine) get public key and check signature   :08/01/2015
    LOG(kError) << "Signature failure.";
    return;
  }

  switch (tag) {
    case MessageTypeTag::Connect:
      message_handler_.HandleMessage(
          Parse<GivenTagFindType_t<MessageTypeTag::Connect>>(binary_input_stream),
          header.GetMessageId());
      break;
    case MessageTypeTag::ConnectResponse:
      message_handler_.HandleMessage(
          Parse<GivenTagFindType_t<MessageTypeTag::ConnectResponse>>(binary_input_stream));
      break;
    case MessageTypeTag::FindGroup:
      message_handler_.HandleMessage(
          Parse<GivenTagFindType_t<MessageTypeTag::FindGroup>>(binary_input_stream));
      break;
    case MessageTypeTag::FindGroupResponse:
      message_handler_.HandleMessage(
          Parse<GivenTagFindType_t<MessageTypeTag::FindGroupResponse>>(binary_input_stream));
      break;
    case MessageTypeTag::GetData:
      message_handler_.HandleMessage(
          Parse<GivenTagFindType_t<MessageTypeTag::GetData>>(binary_input_stream));
      break;
    case MessageTypeTag::GetDataResponse:
      message_handler_.HandleMessage(
          Parse<GivenTagFindType_t<MessageTypeTag::GetDataResponse>>(binary_input_stream));
      break;
    case MessageTypeTag::PutData:
      message_handler_.HandleMessage(
          Parse<GivenTagFindType_t<MessageTypeTag::PutData>>(binary_input_stream));
      break;
    case MessageTypeTag::PutDataResponse:
      message_handler_.HandleMessage(
          Parse<GivenTagFindType_t<MessageTypeTag::PutDataResponse>>(binary_input_stream));
      break;
    //      case MessageTypeTag::PutKey:
    //        message_handler_.HandleMessage(Parse<GivenTagFindType_t<MessageTypeTag::PutKey>>(
    //                                         std::move(header_and_type_enum.first),
    //                                         binary_input_stream));
    //        break;
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
}

std::vector<MessageHeader> RoutingNode::CreateHeaders(Address target, Checksum checksum,
                                                      MessageId message_id) {
  auto targets(connection_manager_.GetTarget(target));
  std::vector<MessageHeader> headers;
  for (const auto& target : targets) {
    headers.emplace_back(
        MessageHeader{DestinationAddress(target.id),
                      SourceAddress{NodeAddress(our_id_), boost::optional<GroupAddress>()},
                      message_id, Checksum{checksum}});
  }
  return headers;
}

std::vector<MessageHeader> RoutingNode::CreateHeaders(Address target, MessageId message_id) {
  auto targets(connection_manager_.GetTarget(target));
  std::vector<MessageHeader> headers;
  for (const auto& target : targets) {
    headers.emplace_back(MessageHeader{
        DestinationAddress(target.id),
        SourceAddress{NodeAddress(our_id_), boost::optional<GroupAddress>()}, message_id});
  }
  return headers;
}

void RoutingNode::ConnectionLost(NodeId peer) { connection_manager_.LostNetworkConnection(peer); }

}  // namespace routing

}  // namespace maidsafe
