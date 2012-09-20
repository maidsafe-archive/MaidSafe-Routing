/*******************************************************************************
 *  Copyright 2012 maidsafe.net limited                                        *
 *                                                                             *
 *  The following source code is property of maidsafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the licence   *
 *  file licence.txt found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of maidsafe.net. *
 ******************************************************************************/

#ifndef MAIDSAFE_ROUTING_MESSAGE_HANDLER_H_
#define MAIDSAFE_ROUTING_MESSAGE_HANDLER_H_

#include <string>

#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/response_handler.h"
#include "maidsafe/routing/service.h"
#include "maidsafe/routing/cache_manager.h"
#include "maidsafe/routing/api_config.h"


namespace maidsafe {

namespace routing {

namespace protobuf { class Message; }

namespace test { class MessageHandlerTest; }

class NetworkUtils;
class NonRoutingTable;
class RoutingTable;
class Timer;

enum class MessageType : int32_t {
  kPing = 1,
  kConnect = 2,
  kFindNodes = 3,
  kProxyConnect = 4,
  kConnectSuccess = 5,
  kMaxRouting = 100,
  kNodeLevel = 101
};

class MessageHandler {
 public:
  MessageHandler(AsioService& asio_service,
                 RoutingTable& routing_table,
                 NonRoutingTable& non_routing_table,
                 NetworkUtils& network,
                 Timer& timer);
  void HandleMessage(protobuf::Message& message);
  void set_message_received_functor(MessageReceivedFunctor message_received_functor);
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
  void HandleGroupMessage(protobuf::Message& message);
  void HandleRelayRequest(protobuf::Message& message);
  void HandleGroupMessageToSelfId(protobuf::Message& message);
  bool IsRelayResponseForThisNode(protobuf::Message& message);
  bool IsGroupMessageRequestToSelfId(protobuf::Message& message);
  bool RelayDirectMessageIfNeeded(protobuf::Message& message);
  void HandleClientMessage(protobuf::Message& message);
  void DecreamentHopsToLive(protobuf::Message& message);
  void HandleMessageForNonRoutingNodes(protobuf::Message& message);
//  bool CheckAndSendToLocalClients(protobuf::Message& message);

  friend class test::MessageHandlerTest;

  AsioService& asio_service_;
  RoutingTable& routing_table_;
  NonRoutingTable& non_routing_table_;
  NetworkUtils& network_;
  Timer& timer_;
  CacheManager cache_manager_;
  std::shared_ptr<ResponseHandler> response_handler_;
  std::shared_ptr<Service> service_;
  MessageReceivedFunctor message_received_functor_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGE_HANDLER_H_
