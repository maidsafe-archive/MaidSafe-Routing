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

#include "maidsafe/routing/messages/messages.h"
#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/sentinel.h"
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
      filter_(std::chrono::minutes(20)),
      sentinel_(io_service_),
      cache_(std::chrono::minutes(10)) {}

RoutingNode::~RoutingNode() {}

void RoutingNode::MessageReceived(NodeId /* peer_id */, rudp::ReceivedMessage serialised_message) {
  InputVectorStream binary_input_stream{std::move(serialised_message)};
  MessageHeader header;
  MessageTypeTag tag;
  try {
    Parse(binary_input_stream, header, tag);
  } catch (std::exception& e) {
    LOG(kError) << "header failure." << boost::current_exception_diagnostic_information(true);
    return;
  }

  if (filter_.Check({header.NetworkAddressablElement(), header.GetMessageId()}))
    return;  // already seen
  // add to filter as soon as posible
  filter_.Add({header.NetworkAddressablElement(), header.GetMessageId()});

  // We add these to cache
  if (tag == MessageTypeTag::GetDataResponse) {
    auto data = Parse<GetDataResponse>(binary_input_stream);
    cache_.Add(data.key, data.data);
  }
  // if we can satisfy request from cache we do
  if (tag == MessageTypeTag::GetData) {
    auto data = Parse<GetData>(binary_input_stream);
    auto test = cache_.Get(data.key);
    if (test) {
      GetDataResponse response(data.key, test.value());
      if (data.relay_node)
        response.relay_node = data.relay_node;
      for (auto& hdr : CreateHeaders(data.key, crypto::Hash<crypto::SHA1>(data.key.string()),
                                        header.GetMessageId())) {
        for (const auto& target : connection_manager_.GetTarget(hdr.GetDestination()))
          rudp_.Send(target.id, Serialise(hdr, MessageTypeTag::GetDataResponse, response),
                     asio::use_future).get();
      }
      return;
    }
  }

  // send to next node(s) even our close group (swarm mode)
  for (const auto& target : connection_manager_.GetTarget(header.GetDestination()))
    rudp_.Send(target.id, serialised_message, asio::use_future).get();

  if (!connection_manager_.AddressInCloseGroupRange(header.GetDestination()))
    return;  // not for us

  // FIXME(dirvine) Sentinel check here!!  :19/01/2015
  switch (tag) {
    case MessageTypeTag::Connect:
      HandleMessage(Parse<Connect>(binary_input_stream), header.GetMessageId());
      break;
    case MessageTypeTag::ConnectResponse:
      HandleMessage(Parse<ConnectResponse>(binary_input_stream));
      break;
    case MessageTypeTag::FindGroup:
      HandleMessage(Parse<FindGroup>(binary_input_stream));
      break;
    case MessageTypeTag::FindGroupResponse:
      HandleMessage(Parse<FindGroupResponse>(binary_input_stream));
      break;
    case MessageTypeTag::GetData:
      HandleMessage(Parse<GetData>(binary_input_stream));
      break;
    case MessageTypeTag::GetDataResponse:
      HandleMessage(Parse<GetDataResponse>(binary_input_stream));
      break;
    case MessageTypeTag::PutData:
      HandleMessage(Parse<PutData>(binary_input_stream));
      break;
    case MessageTypeTag::PutDataResponse:
      HandleMessage(Parse<PutDataResponse>(binary_input_stream));
      break;
    // case MessageTypeTag::Post:
    //   HandleMessage(Parse<GivenTagFindType_t<MessageTypeTag::Post>>(binary_input_stream));
    //   break;
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

// reply with our details;
void RoutingNode::HandleMessage(Connect connect, MessageId message_id) {
  if (!connection_manager_.SuggestNodeToAdd(connect.requester_id))
    return;
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
        rudp_.Send(connect.receiver_id,
                   Serialise(header, MessageToTag<ConnectResponse>::value(), respond),
                   asio::use_future).get();
      });
  // TODO(dirvine)  We need to add rudp_connect here and if sucessfull call
  // connection_manager_.Add(XX) which adds the node to the routing table  :19/01/2015
}

void RoutingNode::HandleMessage(ConnectResponse connect_response) {
  if (!connection_manager_.SuggestNodeToAdd(connect_response.requester_id))
    return;
  // TODO(dirvine) We have confirmed this node is still one we want and now we need to connect to
  // him and add to routing table as in connect above   :19/01/2015
}
void RoutingNode::HandleMessage(FindGroup /*find_group*/) {
  // create header and message and send via rudp
}

void RoutingNode::HandleMessage(FindGroupResponse /*find_group_reponse*/) {
  // this is called to get our group on bootstrap, we will try and connect to each of these nodes
}

void RoutingNode::HandleMessage(GetData /*get_data*/) {}

void RoutingNode::HandleMessage(GetDataResponse /* get_data_response */) {}

void RoutingNode::HandleMessage(PutData /*put_data*/) {}

void RoutingNode::HandleMessage(PutDataResponse /*put_data_response*/) {}

// void RoutingNode::HandleMessage(Post /*post*/) {}

SourceAddress RoutingNode::OurSourceAddress() const {
  return std::make_pair(NodeAddress(connection_manager_.OurId()), boost::optional<GroupAddress>());
}
}  // namespace routing

}  // namespace maidsafe
