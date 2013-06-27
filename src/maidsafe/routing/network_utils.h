/* Copyright 2012 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

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
  void SendToDirectAdjustedRoute(protobuf::Message& message,
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
