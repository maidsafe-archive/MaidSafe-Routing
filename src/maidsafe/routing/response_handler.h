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

#ifndef MAIDSAFE_ROUTING_RESPONSE_HANDLER_H_
#define MAIDSAFE_ROUTING_RESPONSE_HANDLER_H_

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/api_config.h"


namespace maidsafe {

namespace routing {

namespace protobuf { class Message; }

class NetworkUtils;
class NonRoutingTable;
class RoutingTable;

class ResponseHandler {
 public:
  ResponseHandler(AsioService &io_service,
                  RoutingTable &routing_table,
                  NonRoutingTable &non_routing_table,
                  NetworkUtils &network);

  void Ping(protobuf::Message &message);

  void Connect(protobuf::Message &message);

  void FindNode(const protobuf::Message &message);

  void ProxyConnect(protobuf::Message& message);
  void set_request_public_key_functor(RequestPublicKeyFunctor request_public_key);

 private:
  void ReSendFindNodeRequest();

  AsioService &io_service_;
  RoutingTable &routing_table_;
  NonRoutingTable &non_routing_table_;
  NetworkUtils &network_;
  RequestPublicKeyFunctor request_public_key_functor_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_RESPONSE_HANDLER_H_
