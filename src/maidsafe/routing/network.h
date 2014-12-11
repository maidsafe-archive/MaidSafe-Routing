/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#ifndef MAIDSAFE_ROUTING_NETWORK_H_
#define MAIDSAFE_ROUTING_NETWORK_H_

#include <mutex>
#include <string>
#include <vector>

#include "boost/asio/ip/udp.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/bootstrap_file_operations.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/timer.h"

namespace maidsafe {

namespace routing {

namespace protobuf {
class Message;
}

class ClientRoutingTable;
class RoutingTable;
class Acknowledgement;

namespace test {
class GenericNode;
class MockNetwork;
}

class Network {
 public:
  Network(RoutingTable& routing_table, ClientRoutingTable& client_routing_table,
          Acknowledgement& acknowledgement);
  virtual ~Network();
  int Bootstrap(const rudp::MessageReceivedFunctor& message_received_functor,
                const rudp::ConnectionLostFunctor& connection_lost_functor);
  int ZeroStateBootstrap(const rudp::MessageReceivedFunctor& message_received_functor,
                         const rudp::ConnectionLostFunctor& connection_lost_functor,
                         boost::asio::ip::udp::endpoint local_endpoint);
  virtual int GetAvailableEndpoint(const NodeId& peer_id,
                                   const rudp::EndpointPair& peer_endpoint_pair,
                                   rudp::EndpointPair& this_endpoint_pair,
                                   rudp::NatType& this_nat_type);
  virtual int Add(const NodeId& peer_id, const rudp::EndpointPair& peer_endpoint_pair,
                  const std::string& validation_data);
  virtual int MarkConnectionAsValid(const NodeId& peer_id);
  void Remove(const NodeId& peer_id);
  // For sending relay requests, message with empty source ID may be provided, along with
  // direct endpoint.
  void SendToDirect(const protobuf::Message& message, const NodeId& peer_connection_id,
                    const rudp::MessageSentFunctor& message_sent_functor);
  void SendAck(const protobuf::Message message);
  void AdjustAckHistory(protobuf::Message& message);
  virtual void SendToDirect(protobuf::Message& message, const NodeId& peer_node_id,
                            const NodeId& peer_connection_id);
  void SendToDirectAdjustedRoute(protobuf::Message& message, const NodeId& peer_node_id,
                                 const NodeId& peer_connection_id);
  // Handles relay response messages.  Also leave destination ID empty if needs to send as a relay
  // response message
  virtual void SendToClosestNode(const protobuf::Message& message);
  void SendToClosestNode(protobuf::Message& message, const std::vector<NodeId>& exclude);
  void AddToBootstrapFile(const boost::asio::ip::udp::endpoint& endpoint);
  void clear_bootstrap_connection_info();
  NodeId bootstrap_connection_id() const;
  NodeId this_node_relay_connection_id() const;
  rudp::NatType nat_type() const;

  friend class test::GenericNode;
  friend class test::MockNetwork;

 private:
  Network(const Network&);
  Network(const Network&&);
  Network& operator=(const Network&);

  // For zero-state, local_endpoint can be default-constructed.
  int DoBootstrap(const rudp::MessageReceivedFunctor& message_received_functor,
                  const rudp::ConnectionLostFunctor& connection_lost_functor,
                  const BootstrapContacts& bootstrap_contacts,
                  boost::asio::ip::udp::endpoint local_endpoint = boost::asio::ip::udp::endpoint());
  void RudpSend(const NodeId& peer_id, const protobuf::Message& message,
                const rudp::MessageSentFunctor& message_sent_functor);
  void SendTo(const protobuf::Message& message, const NodeId& peer_node_id,
              const NodeId& peer_connection_id, bool no_ack_timer = false);
  void RecursiveSendOn(protobuf::Message message, NodeInfo last_node_attempted = NodeInfo(),
                       int attempt_count = 0);
  void AdjustRouteHistory(protobuf::Message& message);

  bool running_;
  std::mutex running_mutex_;
  unsigned int bootstrap_attempt_;
  NodeId bootstrap_connection_id_;
  NodeId this_node_relay_connection_id_;
  RoutingTable& routing_table_;
  ClientRoutingTable& client_routing_table_;
  Acknowledgement& acknowledgement_;
  rudp::NatType nat_type_;
  rudp::ManagedConnections rudp_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_NETWORK_H_
