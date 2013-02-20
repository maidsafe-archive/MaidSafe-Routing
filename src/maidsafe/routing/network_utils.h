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

#ifndef MAIDSAFE_ROUTING_NETWORK_UTILS_H_
#define MAIDSAFE_ROUTING_NETWORK_UTILS_H_

#include <mutex>
#include <string>
#include <vector>

#include "boost/asio/ip/udp.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/timer.h"


namespace maidsafe {

namespace routing {

namespace protobuf { class Message; }

class ClientRoutingTable;
class RoutingTable;

namespace test {
  class GenericNode;
  class MockNetworkUtils;
}

class NetworkUtils {
 public:
  NetworkUtils(RoutingTable& routing_table, ClientRoutingTable& client_routing_table);
  virtual ~NetworkUtils();
  int Bootstrap(const std::vector<boost::asio::ip::udp::endpoint>& bootstrap_endpoints,
                const rudp::MessageReceivedFunctor& message_received_functor,
                const rudp::ConnectionLostFunctor& connection_lost_functor,
                boost::asio::ip::udp::endpoint local_endpoint = boost::asio::ip::udp::endpoint());
  virtual int GetAvailableEndpoint(const NodeId& peer_id,
                                   const rudp::EndpointPair& peer_endpoint_pair,
                                   rudp::EndpointPair& this_endpoint_pair,
                                   rudp::NatType& this_nat_type);
  virtual int Add(const NodeId& peer_id,
                  const rudp::EndpointPair& peer_endpoint_pair,
                  const std::string& validation_data);
  virtual int MarkConnectionAsValid(const NodeId& peer_id);
  void Remove(const NodeId& peer_id);
  // For sending relay requests, message with empty source ID may be provided, along with
  // direct endpoint.
  void SendToDirect(const protobuf::Message& message,
                    const NodeId& peer_connection_id,
                    const rudp::MessageSentFunctor& message_sent_functor);
  virtual void SendToDirect(const protobuf::Message& message,
                            const NodeId& peer_node_id,
                            const NodeId& peer_connection_id);
  // Handles relay response messages.  Also leave destination ID empty if needs to send as a relay
  // response message
  virtual void SendToClosestNode(const protobuf::Message& message);
  void AddToBootstrapFile(const boost::asio::ip::udp::endpoint& endpoint);
  void clear_bootstrap_connection_info();
  void set_new_bootstrap_endpoint_functor(NewBootstrapEndpointFunctor new_bootstrap_endpoint);
  NodeId bootstrap_connection_id() const;
  NodeId this_node_relay_connection_id() const;
  rudp::NatType nat_type() const;

  friend class test::GenericNode;
  friend class test::MockNetworkUtils;

 private:
  NetworkUtils(const NetworkUtils&);
  NetworkUtils(const NetworkUtils&&);
  NetworkUtils& operator=(const NetworkUtils&);

  void RudpSend(const NodeId& peer_id,
                const protobuf::Message& message,
                const rudp::MessageSentFunctor& message_sent_functor);
  void SendTo(const protobuf::Message& message,
              const NodeId& peer_node_id,
              const NodeId& peer_connection_id);
  void RecursiveSendOn(protobuf::Message message,
                       NodeInfo last_node_attempted = NodeInfo(),
                       int attempt_count = 0);
  void AdjustRouteHistory(protobuf::Message& message);

  bool running_;
  std::mutex running_mutex_;
  uint16_t bootstrap_attempt_;
  std::vector<boost::asio::ip::udp::endpoint> bootstrap_endpoints_;
  NodeId bootstrap_connection_id_;
  NodeId this_node_relay_connection_id_;
  RoutingTable& routing_table_;
  ClientRoutingTable& client_routing_table_;
  rudp::NatType nat_type_;
  NewBootstrapEndpointFunctor new_bootstrap_endpoint_;
  rudp::ManagedConnections rudp_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_NETWORK_UTILS_H_
