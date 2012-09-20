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
class NonRoutingTable;
class RoutingTable;

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Weffc++"
#endif
class Service : public std::enable_shared_from_this<Service> {
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

 public:
  Service(RoutingTable& routing_table,
          NonRoutingTable& non_routing_table,
          NetworkUtils& network);
  virtual ~Service();
  // Handle all incoming requests and send back reply
  void Ping(protobuf::Message& message);
  void Connect(protobuf::Message& message);
  void FindNodes(protobuf::Message& message);
  void ProxyConnect(protobuf::Message& message);
  void set_request_public_key_functor(RequestPublicKeyFunctor request_public_key);
  RequestPublicKeyFunctor request_public_key_functor() const;

 private:
  RoutingTable& routing_table_;
  NonRoutingTable& non_routing_table_;
  NetworkUtils& network_;
  RequestPublicKeyFunctor request_public_key_functor_;

/*void Ping(RoutingTable& routing_table, protobuf::Message& message);

void Connect(RoutingTable& routing_table,
             NonRoutingTable& non_routing_table,
             NetworkUtils& network,
             protobuf::Message& message,
             RequestPublicKeyFunctor request_public_key_functor);

void FindNodes(RoutingTable& routing_table, protobuf::Message& message);

void ProxyConnect(RoutingTable& routing_table, NetworkUtils& network, protobuf::Message& message);
*/
};

}  // namespace routing

}  // namespace maidsafe


#endif  // MAIDSAFE_ROUTING_SERVICE_H_
