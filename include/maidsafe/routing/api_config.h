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

#ifndef MAIDSAFE_ROUTING_API_CONFIG_H_
#define MAIDSAFE_ROUTING_API_CONFIG_H_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "boost/asio/ip/udp.hpp"

#include "maidsafe/common/rsa.h"
#include "maidsafe/rudp/managed_connections.h"


namespace maidsafe {

namespace routing {

class NodeId;
struct NodeInfo;

// If using boost::bind or std::bind, use **shared_from_this** pointers to preserve lifetimes of
// functors. The ResponseFunctor WILL ensure functors are deleted when the system times out.
typedef std::function<void(const std::vector<std::string>& /*message*/)> ResponseFunctor;

// They are passed as a parameter by MessageReceivedFunctor and should be called for responding to
// the received message. Passing an empty message will mean you don't want to reply.
typedef std::function<void(const std::string& /*message*/)> ReplyFunctor;

// This is called on any message received that is NOT a reply to a request made by the Send method.
typedef std::function<void(const std::string& /*message*/,
                           const NodeId& /*group claim*/,
                           ReplyFunctor /*reply functor*/)> MessageReceivedFunctor;

// This is fired to validate a new peer node. User is supposed to validate the node and call
// ValidateThisNode() method with valid public key.
typedef std::function<void(asymm::PublicKey /*public_key*/)> GivePublicKeyFunctor;
typedef std::function<void(NodeId /*node Id*/, GivePublicKeyFunctor)> RequestPublicKeyFunctor;

typedef std::function<bool(const std::string& /*data*/)> HaveCacheDatafunctor;
typedef std::function<void(const std::string& /* data*/)> StoreCacheDataFunctor;

// This functor fires a number from 0 to 100 and represents % network health.
typedef std::function<void(const int& /*network_health*/)> NetworkStatusFunctor;

// This functor fires a validated endpoint which is usable for bootstrapping
typedef std::function<void(const boost::asio::ip::udp::endpoint& /*new_endpoint*/)>
    NewBootstrapEndpointFunctor;

// This functor fires when a new close node is inserted in routing table. Upper layers responsible
// for storing key/value pairs should send all key/values between itself and the new node's address
// to the new node. Keys further than the furthest node can safely be deleted (if any).
typedef std::function<void(const std::vector<NodeInfo> /*new_close_nodes*/)>
    CloseNodeReplacedFunctor;

struct Functors {
  Functors()
      : message_received(),
        network_status(),
        close_node_replaced(),
        set_public_key(),
        request_public_key(),
        have_cache_data(),
        store_cache_data(),
        new_bootstrap_endpoint() {}
  MessageReceivedFunctor message_received;
  NetworkStatusFunctor network_status;
  CloseNodeReplacedFunctor close_node_replaced;
  GivePublicKeyFunctor set_public_key;
  RequestPublicKeyFunctor request_public_key;
  HaveCacheDatafunctor have_cache_data;
  StoreCacheDataFunctor store_cache_data;
  NewBootstrapEndpointFunctor new_bootstrap_endpoint;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_API_CONFIG_H_
