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

#include "maidsafe/routing/cache_manager.h"
#include "maidsafe/routing/api_config.h"

namespace bs2 = boost::signals2;

namespace maidsafe {

namespace routing {

namespace protobuf { class Message;}  // namespace protobuf

class RoutingTable;
class Timer;
class CacheManager;

class MessageHandler {
 public:
  MessageHandler(std::shared_ptr<AsioService> asio_service,
                 RoutingTable &routing_table,
                 rudp::ManagedConnections &rudp,
                 Timer &timer_ptr,
                 MessageReceivedFunctor message_received_functor,
                 RequestPublicKeyFunctor node_validation_functor);
  void ProcessMessage(protobuf::Message &message);
  bool CheckCacheData(protobuf::Message &message);
  bool CheckAndSendToLocalClients(protobuf::Message &message);
  void Send(protobuf::Message &message);
  void set_bootstrap_endpoint(Endpoint endpoint);
  void set_my_relay_endpoint(Endpoint endpoint);
  void set_message_received_functor(MessageReceivedFunctor message_received);
  void set_node_validation_functor(RequestPublicKeyFunctor node_validation);
  Endpoint bootstrap_endpoint();

 private:
  MessageHandler(const MessageHandler&);  // no copy
  MessageHandler(const MessageHandler&&);  // no move
  MessageHandler& operator=(const MessageHandler&);  // no assign
  void DirectMessage(protobuf::Message &message);
  void RoutingMessage(protobuf::Message &message);
  void NodeLevelMessageForMe(protobuf::Message &message);
  void CloseNodesMessage(protobuf::Message &message);
  void ProcessRelayRequest(protobuf::Message &message);
  bool RelayDirectMessageIfNeeded(protobuf::Message &message);
  void GroupMessage(protobuf::Message &message);
  std::shared_ptr<AsioService> asio_service_;
  RoutingTable &routing_table_;
  rudp::ManagedConnections &rudp_;
  Endpoint bootstrap_endpoint_;
  Endpoint my_relay_endpoint_;
  Timer &timer_ptr_;
  CacheManager cache_manager_;
  MessageReceivedFunctor message_received_functor_;
  RequestPublicKeyFunctor node_validation_functor_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGE_HANDLER_H_
