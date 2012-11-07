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

#include <mutex>
#include <string>
#include <vector>

#include "boost/asio/deadline_timer.hpp"
#include "boost/date_time/posix_time/ptime.hpp"

#include "maidsafe/common/rsa.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/api_config.h"


namespace maidsafe {

namespace routing {

namespace protobuf { class Message; }

namespace test {
class ResponseHandlerTest_BEH_ConnectAttempts_Test;
}  // namespace test

class NetworkUtils;
class NonRoutingTable;
class RoutingTable;

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Weffc++"
#endif
class ResponseHandler : public std::enable_shared_from_this<ResponseHandler> {
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

 public:
  ResponseHandler(RoutingTable& routing_table,
                  NonRoutingTable& non_routing_table,
                  NetworkUtils& network);
  virtual ~ResponseHandler();
  virtual void Ping(protobuf::Message& message);
  virtual void Connect(protobuf::Message& message);
  virtual void FindNodes(const protobuf::Message& message);
  virtual void ConnectSuccessAcknowledgement(protobuf::Message& message);
  void set_request_public_key_functor(RequestPublicKeyFunctor request_public_key);
  RequestPublicKeyFunctor request_public_key_functor() const;

  friend class test::ResponseHandlerTest_BEH_ConnectAttempts_Test;

 private:
  void SendConnectRequest(const NodeId peer_node_id);
  void CheckAndSendConnectRequest(const NodeId& node_id);
  void HandleSuccessAcknowledgementAsRequestor(std::vector<NodeId> close_ids);
  void HandleSuccessAcknowledgementAsReponder(NodeInfo peer, const bool& client,
                                              std::vector<NodeId> close_ids);

  mutable std::mutex mutex_;
  RoutingTable& routing_table_;
  NonRoutingTable& non_routing_table_;
  NetworkUtils& network_;
  RequestPublicKeyFunctor request_public_key_functor_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_RESPONSE_HANDLER_H_
