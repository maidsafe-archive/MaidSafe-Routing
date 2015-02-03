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
#include <string>
#include <vector>

#include "asio/io_service.hpp"
#include "asio/use_future.hpp"
#include "asio/ip/udp.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/expected/expected.hpp"

#include "maidsafe/common/types.h"
#include "maidsafe/common/containers/lru_cache.h"
#include "maidsafe/passport/types.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/types.h"

#include "maidsafe/routing/bootstrap_handler.h"
#include "maidsafe/routing/connection_manager.h"
#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/messages/messages.h"
#include "maidsafe/routing/sentinel.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

template <typename Child>
class RoutingNode : public std::enable_shared_from_this<RoutingNode<Child>>,
                    public rudp::ManagedConnections::Listener {
 private:
  using SendHandler = std::function<void(asio::error_code)>;

 public:
  RoutingNode(asio::io_service& io_service, boost::filesystem::path db_location,
              const passport::Pmid& pmid);
  RoutingNode(const RoutingNode&) = delete;
  RoutingNode(RoutingNode&&) = delete;
  RoutingNode& operator=(const RoutingNode&) = delete;
  RoutingNode& operator=(RoutingNode&&) = delete;
  ~RoutingNode() = default;

  // // will return with the data
  template <typename T, typename CompletionToken>
  GetReturn<CompletionToken> Get(Identity key, Address to, CompletionToken token);
  // will return with allowed or not (error_code only)
  template <typename DataType, typename CompletionToken>
  PutReturn<CompletionToken> Put(Address to, DataType data, CompletionToken token);
  // will return with allowed or not (error_code only)
  template <typename FunctorType, typename CompletionToken>
  PostReturn<CompletionToken> Post(Address key, FunctorType functor, CompletionToken token);

  void AddBootstrapContact(rudp::Contact bootstrap_contact) {
    bootstrap_handler_.AddBootstrapContacts(std::vector<rudp::Contact>{bootstrap_contact});
  }

 private:
  void HandleMessage(Connect connect, MessageHeader orig_header);
  // like connect but add targets endpoint
  void HandleMessage(ConnectResponse connect_response);
  // sent by routing nodes to a network Address
  void HandleMessage(FindGroup find_group, MessageHeader orig_header);
  // each member of the group close to network Address fills in their node_info and replies
  void HandleMessage(FindGroupResponse find_group_reponse, MessageHeader orig_header);
  // may be directly sent to a network Address
  void HandleMessage(GetData get_data, MessageHeader orig_header);
  // Each node wiht the data sends it back to the originator
  void HandleMessage(GetDataResponse get_data_response, MessageHeader orig_header);
  // sent by a client to store data, client does information dispersal and sends a part to each of
  // its close group
  void HandleMessage(PutData put_data, MessageHeader orig_header);
  void HandleMessage(PutDataResponse put_data, MessageHeader orig_header);
  // each member of a group needs to send this to the network address (recieveing needs a Quorum)
  // filling in public key again.
  // each member of a group needs to send this to the network Address (recieveing needs a Quorum)
  // filling in public key again.
  void HandleMessage(PostMessage post, MessageHeader orig_header);
  bool TryCache(MessageTypeTag tag, MessageHeader header, Address data_key);
  Authority OurAuthority(const Address& element, const MessageHeader& header) const;
  virtual void MessageReceived(NodeId peer_id,
                               rudp::ReceivedMessage serialised_message) override final;
  virtual void ConnectionLost(NodeId peer) override final;
  void OnCloseGroupChanged(CloseGroupDifference close_group_difference);
  SourceAddress OurSourceAddress() const;
  SourceAddress OurSourceAddress(GroupAddress) const;

  void OnBootstrap(asio::error_code, rudp::Contact,
                   std::function<void(asio::error_code, rudp::Contact)>);

  template <class Message>
  void SendDirect(NodeId, Message, SendHandler);
  EndpointPair NextEndpointPair() {  // TODO(dirvine)   :23/01/2015
    return rudp::EndpointPair();
  }
  // this innocuous looking call will bootstrap the node and also be used if we spot close group
  // nodes appering or vanishing so its pretty important.
  void ConnectToCloseGroup();
  Address OurId() const { return Address(our_fob_.name()); }

 private:
  using unique_identifier = std::pair<Address, uint32_t>;
  asio::io_service& io_service_;
  passport::Pmid our_fob_;
  boost::optional<Address> bootstrap_node_;
  std::atomic<MessageId> message_id_{RandomUint32()};
  rudp::ManagedConnections rudp_;
  BootstrapHandler bootstrap_handler_;
  ConnectionManager connection_manager_;
  LruCache<unique_identifier, void> filter_;
  Sentinel sentinel_;
  LruCache<Address, SerialisedMessage> cache_;
  std::vector<Address> connected_nodes_;
};

template <typename Child>
RoutingNode<Child>::RoutingNode(asio::io_service& io_service, boost::filesystem::path db_location,
                                const passport::Pmid& pmid)
    : io_service_(io_service),
      our_fob_(pmid),
      bootstrap_node_(boost::none),
      rudp_(),
      bootstrap_handler_(std::move(db_location)),
      connection_manager_(io_service, rudp_, Address(pmid.name()->string())),
      filter_(std::chrono::minutes(20)),
      sentinel_(io_service_),
      cache_(std::chrono::minutes(60)) {
  // store this to allow other nodes to get our ID on startup. IF they have full routing tables they
  // need Quorum number of these signed anyway.
  cache_.Add(Address(pmid.name()->string()), Serialise(passport::PublicPmid(our_fob_)));
  // try an connect to any local nodes (5483) Expect to be told Node_Id
  auto temp_id(Address(RandomString(Address::kSize)));
  rudp_.Add(rudp::Contact(temp_id, rudp::EndpointPair{rudp::Endpoint{GetLocalIp(), 5483},
                                                      rudp::Endpoint{GetLocalIp(), 5433}},
                          our_fob_.public_key()),
            [this, temp_id](asio::error_code error) {
    if (!error) {
      bootstrap_node_ = temp_id;
      ConnectToCloseGroup();
      return;
    }
  });
  for (auto& node : bootstrap_handler_.ReadBootstrapContacts()) {
    rudp_.Add(node, [node, this](asio::error_code error) {
      if (!error) {
        bootstrap_node_ = node.id;
        ConnectToCloseGroup();
        return;
      }
    });
    if (bootstrap_node_)
      break;
  }
}

template <typename Child>
template <typename DataType, typename CompletionToken>
SendReturn<CompletionToken> RoutingNode<Child>::Send(NodeId peer_id,
                                                     MessageId message_id,
                                                     SerialisedData message,
                                                     CompletionToken token) {
  /* A unique message_id would be needed per send, which means a unique header
     must be serialised per send? */
  template<typename Handler>
  struct SendRoutine {
    struct Frame {
      std::reference_wrapper<RoutingNode<Child>> routing_node;
      NodeId peer_id;
      MessageId message_id;
      SerialisedData message;
      boost::expected<SerialisedData, std::error_code> result;
      Handler handler;
      std::once_flag once;
    };

    template<Handler>
    struct SendBridge {
      void operator()(int result) const {
        if (result == error ) {
          handler(boost::make_unexpected(...));
        }
      }

      Handler handler;
    };

    static SendBridge<Handler> MakeSendBridge(Handler handler) {
      return {std::move(handler)};
    }

    void operator()(coroutine<SendRoutine<Handler>, Frame>& coro) const {
      ASIO_CORO_REENTER(coro) {
        ASIO_CORO_YIELD {
          auto& this_node = coro.frame().routing_node.get();

          // call_once synchronizes with send and receive async operation
          auto store_result = action::CallOnce(
              std::ref(coro.frame().once),
              action::Store(std::ref(coro.frame().result)).Then(action::Resume(coro)));
          {
            const std::lock_guard<std::mutex> lock{this_node.mutex_};
            this_node.expected_messages[std::move(coro.frame().message_id)] = store_result;
          }

          this_node.rudp_.Send(
                std::move(coro.frame().peer_id),
                std::move(coro.frame.message),
                MakeSendBridge(std::move(store_result)));
        } // yield

        if (coro.frame().result) {
          // process ?
          coro.frame().handler(DataType{});
        } else {
          // retry (burned once_flag ... crap, theres a better way obviously)
          coro.frame().handler(boost::make_unexpected(...));
        }
      }
    }
  };

  SendHandler<CompletionToken> handler(std::forward<decltype(token)>(token));
  asio::async_result<decltype(handler)> result(handler);

  auto coro = MakeCoroutine<SendRoutine<Handler>>(
      std::ref(*this),
      std::move(peer_id),
      std::move(message_id),
      std::move(message),
      boost::expected<SerialisedData, std::error_code>{},
      std::move(handler),
      std::once_flag{});

  return result.get();
}

template <typename Child>
template <typename DataType, typename CompletionToken>
GetReturn<CompletionToken> RoutingNode<Child>::Get(Identity key, Address to,
                                                   CompletionToken token) {
  template<typename Handler>
  struct GetRoutine {
    struct Frame {
      std::reference_wrapper<RoutingNode<Child>> routing_node;
      Address to;
      MessageId message_id;
      SerialisedData message;
      std::vector<boost::expected<DataType, std::error_code>> results;
      std::atomic<std::size_t> count;
      Handler handler;
    };

    void operator()(coroutine<GetRoutine<Handler>, Frame>& coro) const {
      ASIO_CORO_REENTER(coro) {
        ASIO_CORO_YIELD {
          const auto& targets =
            coro.frame().routine_node.get().connection_manager_.GetTarget(coro.frame().to);
          coro.frame().count = targets.size();
          coro.frame().results.resize(targets.size());

          std::size_t count{};
          for (const auto& target : targets) {
            coro.frame().routing_node.get().Send<DataType>(
                target.id,
                std::move(coro.frame().message_id),
                std::move(coro.frame().message),
                action::Store(std::ref(coro.frame().results[count])).Then(action::Resume(coro)));
            ++count;
          }
        } // yield

        if (--(coro.frame().count) == 0) {
          coro.frame().handler(coro.frame().sentinel(coro.frame().results));
        }
      }
    }
  };

  GetHandler<CompletionToken> handler(std::forward<decltype(token)>(token));
  asio::async_result<decltype(handler)> result(handler);

  auto current_id = ++message_id_;
  MessageHeader our_header(std::make_pair(Destination(to), boost::none), OurSourceAddress(),
                           current_id);
  auto message(Serialise(our_header, MessageToTag<GetData>::value(),
                         OurAuthority(Address(key.string()), our_header), DataType::Tag::kValue,
                         coro.frame().key));

  auto coro = MakeCoroutine<GetRoutine<Handler>>(
      std::ref(*this),
      std::move(to),
      std::move(current_id);
      std::move(message),
      std::vector<boost::expected<ImmutableData, std::error_code>>{},
      std::atomic<std::size_t>{},
      std::move(handler));
  coro.Execute();

  return result.get();
}

template <typename Child>
template <typename DataType, typename CompletionToken>
PutReturn<CompletionToken> RoutingNode<Child>::Put(Address key, DataType data,
                                                   CompletionToken token) {
  PutHandler<CompletionToken> handler(std::forward<decltype(token)>(token));
  asio::async_result<decltype(handler)> result(handler);
  io_service_.post([=] {
    auto message(Serialise(MessageHeader(std::make_pair(Destination(key), boost::none),
                                         OurSourceAddress(), ++message_id_),
                           MessageToTag<PutData>::value(), DataType::Tag::kValue,
                           data.Serialise()));  //
    // FIXME(dirvine) need serialiseable data types  :29/01/2015 // , data));
    for (const auto& target : connection_manager_.GetTarget(key)) {
      rudp_.Send(target.id, message, handler);
    }
  });
  return result.get();
}

template <typename Child>
template <typename FunctorType, typename CompletionToken>
PostReturn<CompletionToken> RoutingNode<Child>::Post(Address key, FunctorType functor,
                                                     CompletionToken token) {
  PostHandler<CompletionToken> handler(std::forward<decltype(token)>(token));
  asio::async_result<decltype(handler)> result(handler);
  io_service_.post([=] {
    auto message(Serialise(MessageHeader(std::make_pair(Destination(key), boost::none),
                                         OurSourceAddress(), ++message_id_),
                           MessageToTag<PostMessage>::value(), FunctorType::Tag::kValue, functor));

    for (const auto& target : connection_manager_.GetTarget(key)) {
      rudp_.Send(target, message, handler);
    }
  });
  return result.get();
}

template <typename Child>
void RoutingNode<Child>::ConnectToCloseGroup() {
  FindGroup message(NodeAddress(OurId()), OurId());
  MessageHeader header(DestinationAddress(std::make_pair(Destination(OurId()), boost::none)),
                       SourceAddress{OurSourceAddress()}, ++message_id_);
  if (bootstrap_node_) {
    rudp_.Send(*bootstrap_node_, Serialise(header, MessageToTag<FindGroup>::value(), message),
               [](asio::error_code error) {
      if (error) {
        LOG(kWarning) << "rudp cannot send via bootstrap node" << error.message();
      }
    });

    return;
  }
  for (const auto& target : connection_manager_.GetTarget(OurId()))
    rudp_.Send(target.id, Serialise(header, MessageToTag<Connect>::value(), message),
               [](asio::error_code error) {
      if (error) {
        LOG(kWarning) << "rudp cannot send" << error.message();
      }
    });
}
template <typename Child>
void RoutingNode<Child>::MessageReceived(NodeId /* peer_id */,
                                         rudp::ReceivedMessage serialised_message) {
  InputVectorStream binary_input_stream{serialised_message};
  MessageHeader header;
  MessageTypeTag tag;
  Authority from_authority;
  typename Child::DataType data_tag;
  Identity key;
  try {
    Parse(binary_input_stream, header, tag);
  } catch (const std::exception&) {
    LOG(kError) << "header failure." << boost::current_exception_diagnostic_information(true);
    return;
  }

  if (filter_.Check(header.FilterValue()))
    return;  // already seen
  // add to filter as soon as posible
  filter_.Add({header.FilterValue()});

  // We add these to cache
  if (tag == MessageTypeTag::GetDataResponse) {
    auto data = Parse<GetDataResponse>(binary_input_stream);
    cache_.Add(data.get_key(), data.get_data());
  }
  // if we can satisfy request from cache we do
  if (tag == MessageTypeTag::GetData) {
    auto data = Parse<GetData>(binary_input_stream);
    auto test = cache_.Get(data.get_key());
    if (test) {
      GetDataResponse response(data.get_key(), test.value());
      auto message(Serialise(
          MessageHeader(header.GetDestination(), OurSourceAddress(), header.GetMessageId()),
          MessageTypeTag::GetDataResponse, response));
      for (const auto& target : connection_manager_.GetTarget(header.FromNode()))
        rudp_.Send(target.id, message, [](asio::error_code error) {
          if (error) {
            LOG(kWarning) << "rudp cannot send" << error.message();
          }
        });
      return;
    }
  }

  // send to next node(s) even our close group (swarm mode)
  for (const auto& target : connection_manager_.GetTarget(header.GetDestination().first))
    rudp_.Send(target.id, serialised_message, [](asio::error_code error) {
      if (error) {
        LOG(kWarning) << "rudp cannot send" << error.message();
      }
    });
  // FIXME(dirvine) We need new rudp for this :26/01/2015
  if (header.RelayedMessage() &&
      std::any_of(std::begin(connected_nodes_), std::end(connected_nodes_),
                  [&header](const Address& node) { return node == *header.RelayedMessage(); })) {
    // send message to connected node
    return;
  }

  if (!connection_manager_.AddressInCloseGroupRange(header.GetDestination().first))
    return;  // not for us

  // FIXME(dirvine) Sentinel check here!!  :19/01/2015
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
      Parse(binary_input_stream, from_authority, data_tag, key);
      static_cast<Child*>(this)->HandleGet(header.GetSource(), from_authority,
                                           OurAuthority(Address(key.string()), header), data_tag,
                                           key);
      break;
    case MessageTypeTag::GetDataResponse:
      HandleMessage(Parse<GetDataResponse>(binary_input_stream), std::move(header));
      break;
    case MessageTypeTag::PutData:
      HandleMessage(Parse<PutData>(binary_input_stream), std::move(header));
      break;
    case MessageTypeTag::PostMessage:
      HandleMessage(Parse<PostMessage>(binary_input_stream), std::move(header));
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
      header.GetDestination().first.data != element)
    return Authority::client_manager;
  else if (connection_manager_.AddressInCloseGroupRange(element) &&
           header.GetDestination().first.data == element)
    return Authority::nae_manager;
  else if (header.FromGroup() &&
           connection_manager_.AddressInCloseGroupRange(header.GetDestination().first) &&
           header.GetDestination().first.data != OurId())
    return Authority::node_manager;
  else if (header.FromGroup() &&
           connection_manager_.AddressInCloseGroupRange(*header.FromGroup()) &&
           header.GetDestination().first.data == OurId())
    return Authority::managed_node;
  LOG(kWarning) << "Unknown Authority type";
  BOOST_THROW_EXCEPTION(MakeError(CommonErrors::invalid_parameter));
}

template <typename Child>
void RoutingNode<Child>::ConnectionLost(NodeId peer) {
  connection_manager_.LostNetworkConnection(peer);
}

// reply with our details;
template <typename Child>
void RoutingNode<Child>::HandleMessage(Connect connect, MessageHeader orig_header) {
  if (!connection_manager_.SuggestNodeToAdd(connect.get_requester_id()))
    return;
  auto targets(connection_manager_.GetTarget(connect.get_requester_id()));
  ConnectResponse respond(connect.get_requester_endpoints(), NextEndpointPair(),
                          connect.get_requester_id(), OurId(), passport::PublicPmid(our_fob_));
  assert(connect.get_receiver_id() == OurId());

  MessageHeader header(DestinationAddress(orig_header.ReturnDestinationAddress()),
                       SourceAddress(OurSourceAddress()), orig_header.GetMessageId(),
                       asymm::Sign(Serialise(respond), our_fob_.private_key()));
  // FIXME(dirvine) Do we need to pass a shared_from_this type object or this may segfault on
  // shutdown
  // :24/01/2015
  for (auto& target : targets) {
    rudp_.Send(target.id, Serialise(header, MessageToTag<ConnectResponse>::value(), respond),
               [connect, this](asio::error_code error_code) {
      if (error_code)
        return;
    });
  }
  auto added =
      connection_manager_.AddNode(NodeInfo(connect.get_requester_id(), connect.get_requester_fob()),
                                  connect.get_requester_endpoints());

  rudp_.Add(rudp::Contact(connect.get_requester_id(), connect.get_requester_endpoints(),
                          connect.get_requester_fob().public_key()),
            [connect, added, this](asio::error_code error) mutable {
    if (error) {
      auto target(connect.get_requester_id());
      this->connection_manager_.DropNode(target);
      return;
    }
  });
  if (added)
    static_cast<Child*>(this)->HandleChurn(*added);
}

template <typename Child>
void RoutingNode<Child>::HandleMessage(ConnectResponse connect_response) {
  if (!connection_manager_.SuggestNodeToAdd(connect_response.get_requester_id()))
    return;
  auto added = connection_manager_.AddNode(
      NodeInfo(connect_response.get_requester_id(), connect_response.get_receiver_fob()),
      connect_response.get_receiver_endpoints());
  auto target = connect_response.get_requester_id();
  rudp_.Add(
      rudp::Contact(connect_response.get_receiver_id(), connect_response.get_receiver_endpoints(),
                    connect_response.get_receiver_fob().public_key()),
      [target, added, this](asio::error_code error) {
        if (error) {
          this->connection_manager_.DropNode(target);
          return;
        }
        if (added)
          static_cast<Child*>(this)->HandleChurn(*added);
        if (connection_manager_.Size() >= QuorumSize) {
          rudp_.Remove(*bootstrap_node_, asio::use_future).get();
          bootstrap_node_ = boost::none;
        }
      });
}
template <typename Child>
void RoutingNode<Child>::HandleMessage(FindGroup find_group, MessageHeader orig_header) {
  auto node_infos = std::move(connection_manager_.OurCloseGroup());
  // add ourselves
  node_infos.emplace_back(NodeInfo(OurId(), passport::PublicPmid(our_fob_)));
  FindGroupResponse response(find_group.get_target_id(), node_infos);
  MessageHeader header(DestinationAddress(orig_header.ReturnDestinationAddress()),
                       SourceAddress(OurSourceAddress(GroupAddress(find_group.get_target_id()))),
                       orig_header.GetMessageId(),
                       asymm::Sign(Serialise(response), our_fob_.private_key()));
  auto message(Serialise(header, MessageToTag<FindGroupResponse>::value(), response));
  for (const auto& node : connection_manager_.GetTarget(orig_header.FromNode())) {
    rudp_.Send(node.id, message, asio::use_future).get();
  }
}

template <typename Child>
void RoutingNode<Child>::HandleMessage(FindGroupResponse find_group_reponse,
                                       MessageHeader /* orig_header */) {
  // this is called to get our group on bootstrap, we will try and connect to each of these nodes
  // Only other reason is to allow the sentinel to check signatures and those calls will just fall
  // through here.
  for (const auto node : find_group_reponse.get_node_infos()) {
    if (!connection_manager_.SuggestNodeToAdd(node.id))
      continue;
    Connect message(NextEndpointPair(), OurId(), node.id, passport::PublicPmid(our_fob_));
    MessageHeader header(DestinationAddress(std::make_pair(Destination(node.id), boost::none)),
                         SourceAddress{OurSourceAddress()}, ++message_id_);
    for (const auto& target : connection_manager_.GetTarget(node.id))
      rudp_.Send(target.id, Serialise(header, MessageToTag<Connect>::value(), message),
                 [](asio::error_code error) {
        if (error) {
          LOG(kWarning) << "rudp cannot send" << error.message();
        }
      });
  }
}

template <typename Child>
void RoutingNode<Child>::HandleMessage(GetData get_data, MessageHeader /* orig_header */) {
  auto result = Child::HandleGet(get_data.get_key());
  if (result) {
  }
}

template <typename Child>
void RoutingNode<Child>::HandleMessage(GetDataResponse /* get_data_response */,
                                       MessageHeader /* orig_header */) {}

template <typename Child>
void RoutingNode<Child>::HandleMessage(PutData /*put_data*/, MessageHeader /* orig_header */) {}

template <typename Child>
void RoutingNode<Child>::HandleMessage(PutDataResponse /*put_data_response*/,
                                       MessageHeader /* orig_header */) {}

template <typename Child>
void RoutingNode<Child>::HandleMessage(PostMessage /* post */, MessageHeader /* orig_header */) {}

template <typename Child>
SourceAddress RoutingNode<Child>::OurSourceAddress() const {
  if (bootstrap_node_)
    return std::make_tuple(NodeAddress(*bootstrap_node_), boost::none, ReplyToAddress(OurId()));
  else
    return std::make_tuple(NodeAddress(OurId()), boost::none, boost::none);
}

template <typename Child>
SourceAddress RoutingNode<Child>::OurSourceAddress(GroupAddress group) const {
  return std::make_tuple(NodeAddress(OurId()), group, boost::none);
}

// template <class Message>
// void RoutingNode::SendDirect(Address target, Message message, SendHandler handler) {
//   MessageHeader header(DestinationAddress(std::make_pair(Destination(target), boost::none)),
//                        SourceAddress{OurSourceAddress()}, ++message_id_);
//
//   rudp_.Send(target, Serialise(header, MessageToTag<Message>::value(), message), handler);
// }
//
// void RoutingNode::OnBootstrap(asio::error_code error, rudp::Contact contact,
//                               std::function<void(asio::error_code, rudp::Contact)> handler) {
//   if (error) {
//     return handler(error, contact);
//   }
//
//   SendDirect(contact.id, FindGroup(OurId(), contact.id),
//              [=](asio::error_code error) { handler(error, contact); });
// }


}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_NODE_H_
