/*  Copyright 2012 MaidSafe.net limited

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

#ifndef MAIDSAFE_ROUTING_MESSAGE_HANDLER_H_
#define MAIDSAFE_ROUTING_MESSAGE_HANDLER_H_

#include <string>

#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/cache_manager.h"
#include "maidsafe/routing/response_handler.h"
#include "maidsafe/routing/service.h"
#include "maidsafe/routing/timer.h"
#include "maidsafe/routing/utils.h"
#include "maidsafe/routing/network_utils.h"

namespace maidsafe {

namespace routing {

namespace protobuf {
class Message;
}

namespace test {
class GenericNode;
class MessageHandlerTest;
class MessageHandlerTest_BEH_HandleInvalidMessage_Test;
class MessageHandlerTest_BEH_HandleRelay_Test;
class MessageHandlerTest_DISABLED_BEH_HandleGroupMessage_Test;
class MessageHandlerTest_BEH_HandleNodeLevelMessage_Test;
class MessageHandlerTest_BEH_ClientRoutingTable_Test;
}

namespace detail {
struct TypedMessageRecievedFunctors {
  std::function<void(const SingleToSingleMessage& /*message*/)> single_to_single;
  std::function<void(const SingleToGroupMessage& /*message*/)> single_to_group;
  std::function<void(const GroupToSingleMessage& /*message*/)> group_to_single;
  std::function<void(const GroupToGroupMessage& /*message*/)> group_to_group;
  std::function<void(const SingleToGroupRelayMessage& /*message*/)> single_to_group_relay;
};

}  // unnamed detail

class Network;
struct NetworkUtils;
class ClientRoutingTable;
class RoutingTable;
class NetworkStatistics;

enum class MessageType : int32_t {
  kPing = 1,
  kConnect = 2,
  kFindNodes = 3,
  kConnectSuccess = 4,
  kConnectSuccessAcknowledgement = 5,
  kGetGroup = 6,
  kInformClientOfNewCloseNode = 7,
  kAcknowledgement = 8,
  kMaxRouting = 100,
  kNodeLevel = 101
};

class MessageHandler {
 public:
  MessageHandler(RoutingTable& routing_table, ClientRoutingTable& client_routing_table,
                 Network& network, Timer<std::string>& timer,
                 NetworkUtils& network_utils, AsioService& asio_service);
  void HandleMessage(protobuf::Message& message);
  void set_typed_message_and_caching_functor(TypedMessageAndCachingFunctor functors);
  void set_message_and_caching_functor(MessageAndCachingFunctors functors);
  void set_request_public_key_functor(RequestPublicKeyFunctor request_public_key_functor);

 private:
  MessageHandler(const MessageHandler&);
  MessageHandler(const MessageHandler&&);
  MessageHandler& operator=(const MessageHandler&);
  bool CheckCacheData(protobuf::Message& message);
  void HandleRoutingMessage(protobuf::Message& message);
  void HandleNodeLevelMessageForThisNode(protobuf::Message& message);
  void HandleMessageForThisNode(protobuf::Message& message);
  void HandleMessageAsClosestNode(protobuf::Message& message);
  void HandleDirectMessageAsClosestNode(protobuf::Message& message);
  void HandleGroupMessageAsClosestNode(protobuf::Message& message);
  void HandleMessageAsFarNode(protobuf::Message& message);
  void HandleRelayRequest(protobuf::Message& message);
  void HandleGroupMessageToSelfId(protobuf::Message& message);
  bool IsRelayResponseForThisNode(protobuf::Message& message);
  bool IsGroupMessageRequestToSelfId(protobuf::Message& message);
  bool RelayDirectMessageIfNeeded(protobuf::Message& message);
  void HandleClientMessage(protobuf::Message& message);
  void HandleMessageForNonRoutingNodes(protobuf::Message& message);
  void HandleDirectRelayRequestMessageAsClosestNode(protobuf::Message& message);
  void HandleGroupRelayRequestMessageAsClosestNode(protobuf::Message& message);
  bool HandleCacheLookup(protobuf::Message& message);
  void StoreCacheCopy(const protobuf::Message& message);
  bool IsValidCacheableGet(const protobuf::Message& message);
  bool IsValidCacheablePut(const protobuf::Message& message);
  void InvokeTypedMessageReceivedFunctor(const protobuf::Message& proto_message);
  friend class test::MessageHandlerTest;
  friend class test::MessageHandlerTest_BEH_HandleInvalidMessage_Test;
  friend class test::MessageHandlerTest_BEH_HandleRelay_Test;
  friend class test::MessageHandlerTest_DISABLED_BEH_HandleGroupMessage_Test;
  friend class test::MessageHandlerTest_BEH_HandleNodeLevelMessage_Test;
  friend class test::MessageHandlerTest_BEH_ClientRoutingTable_Test;
  friend class test::GenericNode;


  RoutingTable& routing_table_;
  ClientRoutingTable& client_routing_table_;
  NetworkUtils& network_utils_;
  Network& network_;
  std::unique_ptr<CacheManager> cache_manager_;
  Timer<std::string>& timer_;
  PublicKeyHolder public_key_holder_;
  std::shared_ptr<ResponseHandler> response_handler_;
  std::shared_ptr<Service> service_;
  MessageReceivedFunctor message_received_functor_;
  detail::TypedMessageRecievedFunctors typed_message_received_functors_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGE_HANDLER_H_
