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

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/cache_manager.h"
#include "maidsafe/routing/response_handler.h"
#include "maidsafe/routing/service.h"


namespace maidsafe {

namespace routing {

namespace protobuf { class Message; }

namespace test {
  class MessageHandlerTest;
  class MessageHandlerTest_BEH_HandleInvalidMessage_Test;
  class MessageHandlerTest_BEH_HandleRelay_Test;
  class MessageHandlerTest_BEH_HandleGroupMessage_Test;
  class MessageHandlerTest_BEH_HandleNodeLevelMessage_Test;
  class MessageHandlerTest_BEH_ClientRoutingTable_Test;
}


class NetworkUtils;
class NonRoutingTable;
class RoutingTable;
class Timer;
class AckTimer;
class RemoveFurthestNode;
class GroupChangeHandler;
class NetworkStatistics;


enum class MessageType : int32_t {
  kPing = 1,
  kConnect = 2,
  kFindNodes = 3,
  kConnectSuccess = 4,
  kConnectSuccessAcknowledgement = 5,
  kRemove = 6,
  kClosestNodesUpdate = 7,
  kClosestNodesUpdateSubscribe = 8,
  kGetGroup = 9,
  kAck = 10,
  kMaxRouting = 100,
  kNodeLevel = 101
};

class MessageHandler {
 public:
  MessageHandler(RoutingTable& routing_table,
                 NonRoutingTable& non_routing_table,
                 NetworkUtils& network,
                 Timer& timer,
                 AckTimer& ack_timer,
                 RemoveFurthestNode& remove_node,
                 GroupChangeHandler& group_change_handler,
                 NetworkStatistics& network_statistics);
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
  void HandleRelayRequest(protobuf::Message& message);
  void HandleGroupMessageToSelfId(protobuf::Message& message);
  bool IsRelayResponseForThisNode(protobuf::Message& message);
  bool IsGroupMessageRequestToSelfId(protobuf::Message& message);
  bool RelayDirectMessageIfNeeded(protobuf::Message& message);
  void HandleClientMessage(protobuf::Message& message);
  void HandleMessageForNonRoutingNodes(protobuf::Message& message);
  void HandleDirectRelayRequestMessageAsClosestNode(protobuf::Message& message);
  void HandleGroupRelayRequestMessageAsClosestNode(protobuf::Message& message);
  void HandleCacheLookup(protobuf::Message& message);
  void StoreCacheCopy(const protobuf::Message& message);
  bool IsCacheableRequest(const protobuf::Message& message);
  bool IsCacheableResponse(const protobuf::Message& message);
  friend class test::MessageHandlerTest;
  friend class test::MessageHandlerTest_BEH_HandleInvalidMessage_Test;
  friend class test::MessageHandlerTest_BEH_HandleRelay_Test;
  friend class test::MessageHandlerTest_BEH_HandleGroupMessage_Test;
  friend class test::MessageHandlerTest_BEH_HandleNodeLevelMessage_Test;
  friend class test::MessageHandlerTest_BEH_ClientRoutingTable_Test;

  RoutingTable& routing_table_;
  NonRoutingTable& non_routing_table_;
  NetworkStatistics& network_statistics_;
  NetworkUtils& network_;
  RemoveFurthestNode& remove_furthest_node_;
  GroupChangeHandler& group_change_handler_;
  std::unique_ptr<CacheManager> cache_manager_;
  Timer& timer_;
  AckTimer& ack_timer_;
  std::shared_ptr<ResponseHandler> response_handler_;
  std::shared_ptr<Service> service_;
  MessageReceivedFunctor message_received_functor_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGE_HANDLER_H_
