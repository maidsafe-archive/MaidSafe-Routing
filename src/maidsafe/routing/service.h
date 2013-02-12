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

#ifndef MAIDSAFE_ROUTING_SERVICE_H_
#define MAIDSAFE_ROUTING_SERVICE_H_

#include <memory>

#include "maidsafe/routing/api_config.h"


namespace maidsafe {

namespace routing {

namespace protobuf { class Message; }

class NetworkUtils;
class ClientRoutingTable;
class RoutingTable;

class Service {
 public:
  Service(RoutingTable& routing_table,
          ClientRoutingTable& non_routing_table,
          NetworkUtils& network);
  virtual ~Service();
  // Handle all incoming requests and send back reply
  virtual void Ping(protobuf::Message& message);
  virtual void Connect(protobuf::Message& message);
  virtual void FindNodes(protobuf::Message& message);
  virtual void ConnectSuccess(protobuf::Message& message);
  virtual void GetGroup(protobuf::Message& message);
  void set_request_public_key_functor(RequestPublicKeyFunctor request_public_key);
  RequestPublicKeyFunctor request_public_key_functor() const;

 private:
  void ConnectSuccessFromRequester(NodeInfo& peer);
  void ConnectSuccessFromResponder(NodeInfo& peer, const bool &client);
  bool CheckPriority(const NodeId& this_node, const NodeId& peer_node);

  RoutingTable& routing_table_;
  ClientRoutingTable& non_routing_table_;
  NetworkUtils& network_;
  RequestPublicKeyFunctor request_public_key_functor_;
};

}  // namespace routing

}  // namespace maidsafe


#endif  // MAIDSAFE_ROUTING_SERVICE_H_
