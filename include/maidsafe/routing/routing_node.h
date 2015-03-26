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

#ifndef MAIDSAFE_ROUTING_ROUTING_NODE_H_
#define MAIDSAFE_ROUTING_ROUTING_NODE_H_

#include <chrono>
#include <memory>
#include <utility>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "asio/io_service.hpp"
#include "asio/post.hpp"
#include "asio/use_future.hpp"
#include "asio/ip/udp.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/expected/expected.hpp"

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/types.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/containers/lru_cache.h"
#include "maidsafe/common/data_types/data.h"
#include "maidsafe/crux/socket.hpp"
#include "maidsafe/passport/types.h"

#include "maidsafe/routing/bootstrap_handler.h"
#include "maidsafe/routing/connection_manager.h"
#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/messages/messages.h"
#include "maidsafe/routing/endpoint_pair.h"
#include "maidsafe/routing/sentinel.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

template <typename Child>
class RoutingNode {
 private:
  using SendHandler = std::function<void(asio::error_code)>;

 public:
  RoutingNode();
  RoutingNode(const RoutingNode&) = delete;
  RoutingNode(RoutingNode&&) = delete;
  RoutingNode& operator=(const RoutingNode&) = delete;
  RoutingNode& operator=(RoutingNode&&) = delete;

  // normal bootstrap mechanism
  template <typename CompletionToken>
  BootstrapReturn<CompletionToken> Bootstrap(CompletionToken token);
  // used where we wish to pass a specific node to bootstrap from
  template <typename CompletionToken>
  BootstrapReturn<CompletionToken> Bootstrap(Endpoint endpoint, CompletionToken&& token);

  // will return with the data
  template <typename CompletionToken>
  GetReturn<CompletionToken> Get(Data::NameAndTypeId name_and_type_id, CompletionToken&& token);
  // will return with allowed or not (error_code only)
  template <typename CompletionToken>
  PutReturn<CompletionToken> Put(std::shared_ptr<Data> data, CompletionToken&& token);
  // will return with allowed or not (error_code only)
  template <typename FunctorType, typename CompletionToken>
  PostReturn<CompletionToken> Post(Address to, FunctorType functor, CompletionToken&& token);

  void AddBootstrapContact(Contact bootstrap_contact) {
    bootstrap_handler_.AddBootstrapContacts(std::vector<Contact>(1, bootstrap_contact));
    // FIXME bootstrap handler may be need to be updated to take one entry always
  }

 private:
  // tries to bootstrap to network
  void StartBootstrap();

  void HandleMessage(Connect connect, MessageHeader original_header);
  // like connect but add targets endpoint
  void HandleMessage(ConnectResponse connect_response);
  // sent by routing nodes to a network Address
  void HandleMessage(FindGroup find_group, MessageHeader original_header);
  // each member of the group close to network Address fills in their node_info and replies
  void HandleMessage(FindGroupResponse find_group_reponse, MessageHeader original_header);
  // may be directly sent to a network Address
  void HandleMessage(GetData get_data, MessageHeader original_header);
  // Each node wiht the data sends it back to the originator
  void HandleMessage(GetDataResponse get_data_response) {
    static_cast<Child*>(this)->HandleGetDataResponse(get_data_response);
  }
  // sent by a client to store data, client does information dispersal and sends a part to each of
  // its close group
  void HandleMessage(PutData put_data, MessageHeader original_header);
  void HandleMessage(PutDataResponse put_data, MessageHeader original_header);
  // each member of a group needs to send this to the network address (recieveing needs a Quorum)
  // filling in public key again.
  // each member of a group needs to send this to the network Address (recieveing needs a Quorum)
  // filling in public key again.
  void HandleMessage(routing::Post post, MessageHeader original_header);

  template <typename MessageType>
  void SendSwarmOrParallel(const DestinationAddress& destination, const SourceAddress& source,
                           const MessageType& message, Authority authority);
  void SendSwarmOrParallel(const Address& destination, const SerialisedMessage& serialised_message);
  template <typename MessageType>
  void SendToBootstrapNode(const DestinationAddress& destination, const SourceAddress& source,
                           const MessageType& message, Authority authority);
  void SendToBootstrapNode(const SerialisedMessage& serialised_message);
  void SendToNonRoutingNode(const Address& destination,
                            const SerialisedMessage& serialised_message);
  bool TryCache(MessageTypeTag tag, MessageHeader header, Address name);
  Authority OurAuthority(const Address& element, const MessageHeader& header) const;
  void MessageReceived(Address peer_id, SerialisedMessage serialised_message);
  void ConnectionLost(boost::optional<CloseGroupDifference>, Address peer);
  void OnCloseGroupChanged(CloseGroupDifference close_group_difference);
  SourceAddress OurSourceAddress() const;
  SourceAddress OurSourceAddress(GroupAddress) const;

  void OnBootstrap(asio::error_code, Contact, std::function<void(asio::error_code, Contact)>);
  void PutOurPublicPmid();

  EndpointPair NextEndpointPair();
  // this innocuous looking call will bootstrap the node and also be used if we spot close group
  // nodes appering or vanishing so its pretty important.
  void ConnectToCloseGroup();
  Address OurId() const { return Address(our_fob_.name()); }

  using unique_identifier = std::pair<Address, uint32_t>;
  AsioService asio_service_;
  passport::Pmid our_fob_;
  std::atomic<MessageId> message_id_;
  boost::optional<Address> bootstrap_node_;
  boost::optional<Endpoint> our_external_endpoint_;
  BootstrapHandler bootstrap_handler_;
  ConnectionManager connection_manager_;
  LruCache<unique_identifier, void> filter_;
  Sentinel sentinel_;
  LruCache<Data::NameAndTypeId, SerialisedData> cache_;
  std::shared_ptr<boost::none_t> destroy_indicator_;
};

template <typename Child>
RoutingNode<Child>::RoutingNode()
    : asio_service_(1),
      our_fob_(passport::CreatePmidAndSigner().first),
      message_id_(RandomUint32()),
      bootstrap_node_(boost::none),
      bootstrap_handler_(),
      connection_manager_(asio_service_.service(), our_fob_.name(),
                          [=](Address address, SerialisedMessage msg) {
                            MessageReceived(std::move(address), std::move(msg));
                          },
                          [=](boost::optional<CloseGroupDifference> diff, Address peer_id) {
                            ConnectionLost(std::move(diff), std::move(peer_id));
                          }),
      filter_(std::chrono::minutes(20)),
      sentinel_([](Address) {}, [](GroupAddress) {}),
      cache_(std::chrono::minutes(60)),
      destroy_indicator_(new boost::none_t) {
  LOG(kInfo) << OurId() << " RoutingNode created";
  // store this to allow other nodes to get our ID on startup. IF they have full routing tables they
  // need Quorum number of these signed anyway.
  passport::PublicPmid our_public_pmid(our_fob_);
  cache_.Add(our_public_pmid.NameAndType(), Serialise(our_public_pmid));
  StartBootstrap();
}

template <typename Child>
void RoutingNode<Child>::StartBootstrap() {
  // try connect to any local nodes (5483) Expect to be told Node_Id
  Endpoint live_port_ep(GetLocalIp(), kLivePort);

  // skip trying to bootstrap off self
  if (connection_manager_.AcceptingPort() == kLivePort) {
    return;
  }

  connection_manager_.Connect(live_port_ep,
    [=](asio::error_code error, Address peer_addr, Endpoint our_public_endpoint) {
      if (error) {
        LOG(kWarning) << "Cannot connect to bootstrap endpoint < " << peer_addr << " >"
                      << error.message();
        // TODO(Team): try connect to bootstrap contacts and other options
        // (hardcoded endpoints).on failure keep retrying all options forever
        return;
      }
      LOG(kInfo) << OurId() << " Bootstrapped with " << peer_addr << " his ep:" << live_port_ep;
      // FIXME(Team): Thread safety.
      bootstrap_node_ = peer_addr;
      our_external_endpoint_ = our_public_endpoint;
      // bootstrap_endpoint_ = our_endpoint; this will not required if
      // connection manager has this connection
//      PutOurPublicPmid();                                                              // DISABLED FOR TESTING UNCOMMENT
      ConnectToCloseGroup();
    });

  // auto bootstrap_contacts = bootstrap_handler_.ReadBootstrapContacts();
}

template <typename Child>
void RoutingNode<Child>::PutOurPublicPmid() {
  assert(bootstrap_node_);
  std::shared_ptr<Data> our_public_pmid{new passport::PublicPmid(our_fob_)};
  auto name = our_public_pmid->Name();
  auto type_id = our_public_pmid->TypeId();
  asio::post(asio_service_.service(), [=] {
    // FIXME(Prakash) request should be signed and may be sent to ClientManager
    PutData put_data_message(type_id, Serialise(our_public_pmid));
    SendToBootstrapNode(std::make_pair(Destination(name), boost::none), OurSourceAddress(),
                        put_data_message, Authority::client);
  });
}

template <typename Child>
EndpointPair RoutingNode<Child>::NextEndpointPair() {
  auto port = connection_manager_.AcceptingPort();

  return EndpointPair(Endpoint(GetLocalIp(), port),
                      our_external_endpoint_ ? Endpoint(our_external_endpoint_->address(), port)
                                             : Endpoint());
}

template <typename Child>
template <typename CompletionToken>
GetReturn<CompletionToken> RoutingNode<Child>::Get(Data::NameAndTypeId name_and_type_id,
                                                   CompletionToken&& token) {
  GetHandler<CompletionToken> handler(std::forward<decltype(token)>(token));
  asio::async_result<decltype(handler)> result(handler);
  asio::post(asio_service_.service(), [=] {
    MessageHeader our_header(std::make_pair(Destination(name_and_type_id.name), boost::none),
                             OurSourceAddress(), ++message_id_, Authority::node);
    GetData request(name_and_type_id, OurSourceAddress());
    auto message(Serialise(our_header, MessageToTag<GetData>::value(), request));
    for (const auto& target : connection_manager_.GetTarget(name_and_type_id.name)) {
      connection_manager_.Send(target.id, message, [](asio::error_code) {});
    }
  });
  return result.get();
}

// As this is a routing_node this should be renamed to PutPublicPmid one time
// and possibly it should be a single type it deals with rather than Put<DataType> as this call is
// special
// amongst all node types and is the only unauthorised Put anywhere
// nodes have no reason to Put anywhere else
template <typename Child>
template <typename CompletionToken>
PutReturn<CompletionToken> RoutingNode<Child>::Put(std::shared_ptr<Data> data,
                                                   CompletionToken&& token) {
  PutHandler<CompletionToken> handler(std::forward<decltype(token)>(token));
  asio::async_result<decltype(handler)> result(handler);

  asio::post(asio_service_.service(), [=]() mutable {
    MessageHeader our_header(
        std::make_pair(Destination(OurId()), boost::none),  // send to ClientMgr
        OurSourceAddress(), ++message_id_, Authority::client);
    PutData request(data->TypeId(), Serialise(data));
    // FIXME(dirvine) For client in real put this needs signed :08/02/2015
    auto message(Serialise(our_header, MessageToTag<PutData>::value(), request));
    auto targets(connection_manager_.GetTarget(OurId()));
    for (const auto& target : targets) {
      connection_manager_.Send(target.id, message, [](asio::error_code) {});
    }
    if (targets.empty() && bootstrap_node_) {
      connection_manager_.Send(*bootstrap_node_, message,
                               [handler](std::error_code ec) mutable { handler(ec); });
    } else {
      handler(make_error_code(RoutingErrors::not_connected));
    }
  });
  return result.get();
}

template <typename Child>
template <typename FunctorType, typename CompletionToken>
PostReturn<CompletionToken> RoutingNode<Child>::Post(Address to, FunctorType functor,
                                                     CompletionToken&& token) {
  PostHandler<CompletionToken> handler(std::forward<decltype(token)>(token));
  asio::async_result<decltype(handler)> result(handler);
  asio::post(asio_service_.service(), [=] {
    MessageHeader our_header(std::make_pair(Destination(to), boost::none), OurSourceAddress(),
                             ++message_id_, Authority::node);
    PutData request(FunctorType::Tag::kValue, functor);
    // FIXME(dirvine) This needs signed :08/02/2015
    auto message(Serialise(our_header, MessageToTag<routing::Post>::value(), request));

    for (const auto& target : connection_manager_.GetTarget(to)) {
      connection_manager_.Send(target.id, message, [](asio::error_code) {});
    }
    handler();
  });
  return result.get();
}

template <typename Child>
void RoutingNode<Child>::ConnectToCloseGroup() {
  FindGroup find_group_message(NodeAddress(OurId()), OurId());
  if (bootstrap_node_) {  // TODO(Team) cleanup
    SendToBootstrapNode(std::make_pair(Destination(OurId()), boost::none), OurSourceAddress(),
                        find_group_message, Authority::node);
  } else {
    SendSwarmOrParallel(std::make_pair(Destination(OurId()), boost::none), OurSourceAddress(),
                        find_group_message, Authority::node);
  }
}


template <typename Child>
void RoutingNode<Child>::MessageReceived(Address peer_id, SerialisedMessage serialised_message) {
  InputVectorStream binary_input_stream{serialised_message};
  MessageHeader header;
  MessageTypeTag tag;
  try {
    Parse(binary_input_stream, header, tag);
  } catch (const std::exception&) {
    LOG(kError) << "header failure." << boost::current_exception_diagnostic_information();
    return;
  }

  if (filter_.Check(header.FilterValue()))
    return;  // already seen
  // add to filter as soon as posible
  filter_.Add({header.FilterValue()});

  LOG(kVerbose) << OurId()
                << " Msg from peer " << peer_id
                << " tag:" << static_cast<std::underlying_type<MessageTypeTag>::type>(tag)
                << " " << header;

  // We add these to cache
  if (tag == MessageTypeTag::GetDataResponse) {
    auto data = Parse<GetDataResponse>(binary_input_stream);
    if (data.data()) {
      std::shared_ptr<Data> parsed(Parse<std::shared_ptr<Data>>(*data.data()));
      cache_.Add(parsed->NameAndType(), *data.data());
    }
  }
  // if we can satisfy request from cache we do
  if (tag == MessageTypeTag::GetData) {
    auto data = Parse<GetData>(binary_input_stream);
    auto test = cache_.Get(data.name_and_type_id());
    // FIXME(dirvine) move to upper lauer :09/02/2015
    // if (test) {
    //   GetDataResponse response(data.name(), test);
    //   auto message(Serialise(MessageHeader(header.Destination(), OurSourceAddress(),
    //                                        header.MessageId(), Authority::node),
    //                          MessageTypeTag::GetDataResponse, response));
    //   for (const auto& target : connection_manager_.GetTarget(header.FromNode()))
    //     rudp_.Send(target.id, message, [](asio::error_code error) {
    //       if (error) {
    //         LOG(kWarning) << "rudp cannot send" << error.message();
    //       }
    //     });
    //   return;
    // }
  }

  // send to next node(s) even our close group (swarm mode)
  SendSwarmOrParallel(header.Destination().first, serialised_message);

  // TODO(Prakash) cleanup aim to abstract relay logic here and may be use term routed message for
  // response messages

  bool relay_request = (header.Source().reply_to_address &&
                        (header.Source().node_address.data == OurId()) &&
                        (header.Destination().first.data != OurId()));
  if (relay_request) { // relay request
    LOG(kVerbose) << "relay request already fwded";
//    return; // already fwded // Can not return here. THis could be a group message and I might be group member
  }

  bool relay_response = (header.Destination().second &&
                         (header.Destination().first.data == OurId()));

  if (relay_response) {  // relay response
    //LOG(kVerbose) << OurId() << " relay response try to send to nrt " << (*header.ReplyToAddress()).data;
    std::set<Address> connected_non_routing_nodes{connection_manager_.GetNonRoutingNodes()};
    if (std::any_of(std::begin(connected_non_routing_nodes), std::end(connected_non_routing_nodes),
                    [&header](const Address& node) { return node == header.ReplyToAddress()->data;
        })) {
    // send message to connected node
    connection_manager_.SendToNonRoutingNode(*header.ReplyToAddress(), serialised_message,
                                             [](asio::error_code /*error*/) {});
    }
    return;  // no point progressing further as I was destination and I don't have the replyto node connected
  }

//  if (header.RelayedMessage() && (hjeader.FromNode().data != OurId())) {  // skip outgoing msgs
//    std::set<Address> connected_non_routing_nodes{connection_manager_.GetNonRoutingNodes()};
//    if (std::any_of(
//            std::begin(connected_non_routing_nodes), std::end(connected_non_routing_nodes),
//            [&header](const Address& node) { return node == (*header.ReplyToAddress()).data; })) {
//      // send message to connected node
//      connection_manager_.SendToNonRoutingNode(*header.ReplyToAddress(), serialised_message,
//                                               [](asio::error_code /*error*/) {});
//      return;
//    }
//  }

  if (!connection_manager_.AddressInCloseGroupRange(header.Destination().first)) {
    LOG(kVerbose) << "not for us";
    return;  // not for us
  }

  // Drop message before Sentinel check if it is a direct message type (Connect, ConnectResponse)
  // and this node is in the group but the message destination is another group member node.

  if ((tag == MessageTypeTag::Connect) || (tag == MessageTypeTag::ConnectResponse)) {
    if (header.Destination().first.data != OurId()) {  // not for me
      if ((!header.Destination().second.is_initialized()))
        return;
      if ((header.Destination().second) && (*header.Destination().second).data != OurId()) {
        LOG(kVerbose) << "not for me";
        return;
      }
    }
  }

  // FIXME(dirvine) Sentinel check here!!  :19/01/2015
  //  auto result = sentinel_.Add(header, tag, serialised_message);
  //  if (!result)
  //    return;

  switch (tag) {
    case MessageTypeTag::Connect:
      HandleMessage(Parse<Connect>(binary_input_stream), std::move(header));
      break;
    case MessageTypeTag::ConnectResponse:
      HandleMessage(Parse<ConnectResponse>(binary_input_stream));
      break;
    case MessageTypeTag::FindGroup:
      HandleMessage(Parse<FindGroup>(binary_input_stream), std::move(header));
      break;
    case MessageTypeTag::FindGroupResponse:
      HandleMessage(Parse<FindGroupResponse>(binary_input_stream), std::move(header));
      break;
    case MessageTypeTag::GetData:
      static_cast<Child*>(this)
          ->HandleMessage(Parse<GetData>(binary_input_stream), std::move(header));
      break;
    case MessageTypeTag::GetDataResponse:
      // static_cast<Child*>(this)
      //     ->HandleMessage(Parse<GetDataResponse>(binary_input_stream), std::move(header));
      break;
    case MessageTypeTag::PutData:
      HandleMessage(Parse<PutData>(binary_input_stream), std::move(header));
      break;
    case MessageTypeTag::Post:
      HandleMessage(Parse<routing::Post>(binary_input_stream), std::move(header));
      break;
    default:
      LOG(kWarning) << "Received message of unknown type.";
      break;
  }
}

template <typename Child>
Authority RoutingNode<Child>::OurAuthority(const Address& element,
                                           const MessageHeader& header) const {
  if (!header.FromGroup() && connection_manager_.AddressInCloseGroupRange(header.FromNode()) &&
      header.Destination().first.data != element)
    return Authority::client_manager;
  else if (connection_manager_.AddressInCloseGroupRange(element) &&
           header.Destination().first.data == element)
    return Authority::nae_manager;
  else if (header.FromGroup() &&
           connection_manager_.AddressInCloseGroupRange(header.Destination().first) &&
           header.Destination().first.data != OurId())
    return Authority::node_manager;
  else if (header.FromGroup() &&
           connection_manager_.AddressInCloseGroupRange(*header.FromGroup()) &&
           header.Destination().first.data == OurId())
    return Authority::managed_node;
  LOG(kWarning) << "Unknown Authority type";
  BOOST_THROW_EXCEPTION(MakeError(CommonErrors::invalid_argument));
}

template <typename Child>
void RoutingNode<Child>::ConnectionLost(boost::optional<CloseGroupDifference> diff, Address) {
  // auto change = connection_manager_.LostNetworkConnection(peer);
  if (diff)
    static_cast<Child*>(this)->HandleChurn(*diff);
}

// reply with our details;
template <typename Child>
void RoutingNode<Child>::HandleMessage(Connect connect, MessageHeader original_header) {
  LOG(kInfo) << OurId() << " HandleMessage " << connect;
  if (!connection_manager_.SuggestNodeToAdd(connect.requester_id()))
    return;

  ConnectResponse respond(connect.requester_endpoints(), NextEndpointPair(), connect.requester_id(),
                          OurId(), passport::PublicPmid(our_fob_));

  assert(connect.receiver_id() == OurId());

  MessageHeader header(DestinationAddress(original_header.ReturnDestinationAddress()),
                       SourceAddress(OurSourceAddress()), original_header.MessageId(),
                       Authority::node,
                       asymm::Sign(asymm::PlainText(Serialise(respond)), our_fob_.private_key()));
  auto message = Serialise(header, MessageToTag<ConnectResponse>::value(), respond);

  if (bootstrap_node_) {  // TODO cleanup
    SendToBootstrapNode(message);
    return;
  }

  SendSwarmOrParallel(connect.requester_id(), message);
  if (original_header.ReplyToAddress())
    SendToNonRoutingNode((*original_header.ReplyToAddress()).data, message);

  std::weak_ptr<boost::none_t> destroy_guard = destroy_indicator_;

  connection_manager_.AddNodeAccept(
      NodeInfo(connect.requester_id(), connect.requester_fob(), true),
      connect.requester_endpoints(),
      [=](asio::error_code error, boost::optional<CloseGroupDifference> added) {
        LOG(kError) << " AddNodeAccept ";
        if (!destroy_guard.lock())
          return;
        if (!error && added)
          static_cast<Child*>(this)->HandleChurn(*added);
      });
}

template <typename Child>
void RoutingNode<Child>::HandleMessage(ConnectResponse connect_response) {
  LOG(kInfo) << OurId() << " HandleMessage " << connect_response;
  if (!connection_manager_.SuggestNodeToAdd(connect_response.receiver_id()))
    return;

  std::weak_ptr<boost::none_t> destroy_guard = destroy_indicator_;

  // Workaround because ConnectResponse isn't copyconstructibe.
  auto response_ptr = std::make_shared<ConnectResponse>(std::move(connect_response));
  LOG(kError) << OurId() << " calling AddNode " << response_ptr->receiver_id() << " " << response_ptr->receiver_endpoints();
  connection_manager_.AddNode(
      NodeInfo(response_ptr->receiver_id(), response_ptr->receiver_fob(), false),
      response_ptr->receiver_endpoints(),
      [=](asio::error_code error, boost::optional<CloseGroupDifference> added) {
        if (!destroy_guard.lock())
          return;

        auto target = response_ptr->requester_id();
        if (!error && added)
          static_cast<Child*>(this)->HandleChurn(*added);
        if (connection_manager_.Size() >= QuorumSize) {
          // rudp_.Remove(*bootstrap_node_, asio::use_future).get(); // FIXME (Prakash)
          bootstrap_node_ = boost::none;
        }
      });
}

template <typename Child>
void RoutingNode<Child>::HandleMessage(FindGroup find_group, MessageHeader original_header) {
  LOG(kInfo) << OurId() << " HandleMessage " << find_group;
  auto close_group = connection_manager_.OurCloseGroup();
  // add ourselves
  std::vector<passport::PublicPmid> group;
  group.reserve(close_group.size() + 1);
  for (auto& node_info : close_group) {
    group.push_back(std::move(node_info.dht_fob));
  }
  group.emplace_back(passport::PublicPmid(our_fob_));
  FindGroupResponse response(find_group.target_id(), std::move(group));
  MessageHeader header(DestinationAddress(original_header.ReturnDestinationAddress()),
                       SourceAddress(OurSourceAddress(GroupAddress(find_group.target_id()))),
                       original_header.MessageId(), Authority::nae_manager,
                       asymm::Sign(asymm::PlainText(Serialise(response)), our_fob_.private_key()));
  auto message(Serialise(header, MessageToTag<FindGroupResponse>::value(), response));
  if (bootstrap_node_) {
    SendToBootstrapNode(message);
  } else {
    SendSwarmOrParallel(original_header.FromNode(), message);
  }
  // if node in my group && in non routing list send it to non_routnig list as well
  if (original_header.ReplyToAddress())
    SendToNonRoutingNode((*original_header.ReplyToAddress()).data, message);
}

template <typename Child>
void RoutingNode<Child>::HandleMessage(FindGroupResponse find_group_response,
                                       MessageHeader /* original_header */) {
  // this is called to get our group on bootstrap, we will try and connect to each of these nodes
  // Only other reason is to allow the sentinel to check signatures and those calls will just fall
  // through here.
  LOG(kInfo) << OurId() << " HandleMessage " << find_group_response;
  for (const auto node_pmid : find_group_response.group()) {
    Address node_id(node_pmid.Name());
                                                                //DELETE ME DEBUG CODE
                                                                if ((find_group_response.group().size() == 2) &&(node_id == *bootstrap_node_)) {
                                                                  LOG(kWarning) << "skipping bootstrap guy to connect";
                                                                  continue;
                                                                }
    if (!connection_manager_.SuggestNodeToAdd(node_id))
      continue;
    Connect connect_message(NextEndpointPair(), OurId(), node_id, passport::PublicPmid(our_fob_));
    if (bootstrap_node_) {  // TODO(Team) cleanup
      SendToBootstrapNode(std::make_pair(Destination(node_id), boost::none), OurSourceAddress(),
                          connect_message, Authority::nae_manager);
    } else {
      SendSwarmOrParallel(std::make_pair(Destination(node_id), boost::none), OurSourceAddress(),
                          connect_message, Authority::nae_manager);
    }
  }
}

template <typename Child>
void RoutingNode<Child>::HandleMessage(GetData get_data, MessageHeader header) {
  auto result = static_cast<Child*>(this)->HandleGet(
      header.Source(), header.FromAuthority(),
      OurAuthority(get_data.name_and_type_id().name, header), get_data.name_and_type_id());
  if (!result) {
    // send back error
    return;
  }
  if (result->which() == 0u) {
    // send on
  } else if (result->which() == 1u) {
    // send back the data
  }
}

template <typename Child>
void RoutingNode<Child>::HandleMessage(PutData put_data, MessageHeader original_header) {
  std::shared_ptr<const Data> parsed(Parse<std::shared_ptr<const Data>>(put_data.data()));
  cache_.Add(parsed->NameAndType(), put_data.data());
  auto result = static_cast<Child*>(this)
                    ->HandlePut(original_header.Source(), original_header.FromAuthority(),
                                OurAuthority(parsed->NameAndType().name, original_header), parsed);
  if (result) {
    // TODO(Fraser#5#): 2015-03-20 - Return error somehow.
  }
}

template <typename Child>
void RoutingNode<Child>::HandleMessage(PutDataResponse /*put_data_response*/,
                                       MessageHeader /* original_header */) {}

template <typename Child>
void RoutingNode<Child>::HandleMessage(routing::Post /* post */,
                                       MessageHeader /* original_header */) {}

template <typename Child>
SourceAddress RoutingNode<Child>::OurSourceAddress() const {
  if (bootstrap_node_)
    return SourceAddress(NodeAddress(*bootstrap_node_), boost::none, ReplyToAddress(OurId()));
  else
    return SourceAddress(NodeAddress(OurId()), boost::none, boost::none);
}

template <typename Child>
SourceAddress RoutingNode<Child>::OurSourceAddress(GroupAddress group) const {
  return SourceAddress(NodeAddress(OurId()), group, boost::none);
}


template <typename Child>
template <typename MessageType>
void RoutingNode<Child>::SendSwarmOrParallel(const DestinationAddress& destination,
                                             const SourceAddress& source,
                                             const MessageType& message, Authority authority) {
  MessageHeader header(destination, source, ++message_id_, authority);
  for (const auto& target : connection_manager_.GetTarget(destination.first.data)) {
    auto wrapped_message = Serialise(header, MessageToTag<MessageType>::value(), message);
    connection_manager_.Send(target.id, std::move(wrapped_message), [](asio::error_code error) {
      if (error) {
        LOG(kWarning) << "Connection manager cannot send" << error.message();
      }
    });
  }
}

template <typename Child>
void RoutingNode<Child>::SendSwarmOrParallel(const Address& destination,
                                             const SerialisedMessage& serialised_message) {
  for (const auto& target : connection_manager_.GetTarget(destination)) {
    connection_manager_.Send(target.id, serialised_message, [](asio::error_code error) {
      if (error) {
        LOG(kWarning) << "Connection manager cannot send" << error.message();
      }
    });
  }
}

template <typename Child>
template <typename MessageType>
void RoutingNode<Child>::SendToBootstrapNode(const DestinationAddress& destination,
                                             const SourceAddress& source,
                                             const MessageType& message, Authority authority) {
  // assert(source.SourceAddress()== NodeAddress(*bootstrap_node_));  //FIXME(prakash)
  MessageHeader header(destination, source, ++message_id_, authority);
  auto wrapped_message = Serialise(header, MessageToTag<MessageType>::value(), message);
  connection_manager_.Send(
      *bootstrap_node_, std::move(wrapped_message), [](asio::error_code error) {
        if (error) {
          LOG(kWarning) << "Connection manager cannot send to bootstrap node, " << error.message();
        }
      });
}

template <typename Child>
void RoutingNode<Child>::SendToBootstrapNode(const SerialisedMessage& serialised_message) {
  auto destination = *bootstrap_node_;
  connection_manager_.Send(
      destination, serialised_message, [=](asio::error_code error) {
    if (error) {
      LOG(kWarning) << "Connection manager cannot send to bootstrap node " << destination
                    << " , error : " << error.message();
    }
  });
}

template <typename Child>
void RoutingNode<Child>::SendToNonRoutingNode(const Address& destination,
                                              const SerialisedMessage& serialised_message) {
  connection_manager_.SendToNonRoutingNode(destination, serialised_message,
                                           [=](asio::error_code error) {
      if (error) {
        LOG(kWarning) << "Connection manager cannot send to destination " << destination
                      << " , error : " << error.message();
      }
  });
}

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_NODE_H_
