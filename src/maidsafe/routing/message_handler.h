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

#include <atomic>
#include <string>

#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/response_handler.h"
#include "maidsafe/routing/cache_manager.h"
#include "maidsafe/routing/api_config.h"


namespace maidsafe {

namespace routing {

namespace protobuf { class Message; }

class CacheManager;
class NetworkUtils;
class NonRoutingTable;
class RoutingTable;
class Timer;

class MessageHandler {
 public:
  MessageHandler(AsioService& asio_service,
                 RoutingTable& routing_table,
                 NonRoutingTable& non_routing_table,
                 NetworkUtils& network,
                 Timer& timer_ptr);
  void ProcessMessage(protobuf::Message& message);
  bool CheckCacheData(protobuf::Message& message);
  bool CheckAndSendToLocalClients(protobuf::Message& message);
  void set_message_received_functor(MessageReceivedFunctor message_received);
  void set_request_public_key_functor(RequestPublicKeyFunctor node_validation);
  void set_tearing_down();
  bool tearing_down();

 private:
  MessageHandler(const MessageHandler&);
  MessageHandler(const MessageHandler&&);
  MessageHandler& operator=(const MessageHandler&);
  void DirectMessage(protobuf::Message& message);
  void RoutingMessage(protobuf::Message& message);
  void NodeLevelMessageForMe(protobuf::Message& message);
  void CloseNodesMessage(protobuf::Message& message);
  void ProcessRelayRequest(protobuf::Message& message);
  bool RelayDirectMessageIfNeeded(protobuf::Message& message);
  void ClientMessage(protobuf::Message& message);
  void GroupMessage(protobuf::Message& message);

  AsioService& asio_service_;
  RoutingTable& routing_table_;
  NonRoutingTable& non_routing_table_;
  NetworkUtils& network_;
  Timer& timer_ptr_;
  CacheManager cache_manager_;
  ResponseHandler response_handler_;
  MessageReceivedFunctor message_received_functor_;
  RequestPublicKeyFunctor request_public_key_functor_;
  std::atomic<bool> tearing_down_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGE_HANDLER_H_
