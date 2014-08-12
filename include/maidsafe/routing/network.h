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
#include "boost/date_time/posix_time/posix_time_config.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/rudp/return_codes.h"
#include "maidsafe/rudp/managed_connections.h"


#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/timer.h"
#include "maidsafe/routing/bootstrap_file_operations.h"
#include "maidsafe/routing/bootstrap_utils.h"
#include "maidsafe/routing/client_routing_table.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/utils.h"
#include "maidsafe/routing/utils2.h"

namespace maidsafe {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;
typedef boost::shared_lock<boost::shared_mutex> SharedLock;
typedef boost::unique_lock<boost::shared_mutex> UniqueLock;

}  // anonymous namespace


namespace routing {

class ClientRoutingTable;

namespace test {
class GenericNode;
template <typename NodeType>
class MockNetwork;
}

template <typename NodeType>
class Network {
 public:
  Network(Connections<NodeType>& connections);
  virtual ~Network();
  int Bootstrap(const rudp::MessageReceivedFunctor& message_received_functor,
                const rudp::ConnectionLostFunctor& connection_lost_functor);
  int ZeroStateBootstrap(const rudp::MessageReceivedFunctor& message_received_functor,
                         const rudp::ConnectionLostFunctor& connection_lost_functor,
                         LocalEndpoint local_endpoint);
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
  virtual void SendToDirect(const protobuf::Message& message, const NodeId& peer_node_id,
                            const NodeId& peer_connection_id);
  void SendToDirectAdjustedRoute(protobuf::Message& message, const NodeId& peer_node_id,
                                 const NodeId& peer_connection_id);
  // Handles relay response messages.  Also leave destination ID empty if needs to send as a relay
  // response message
  virtual void SendToClosestNode(const protobuf::Message& message);
  void AddToBootstrapFile(const boost::asio::ip::udp::endpoint& endpoint);
  void clear_bootstrap_connection_info();
  void set_new_bootstrap_contact_functor(NewBootstrapContactFunctor new_bootstrap_contact);
  NodeId bootstrap_connection_id() const;
  NodeId this_node_relay_connection_id() const;
  rudp::NatType nat_type() const;

  friend class test::GenericNode;
  template <typename Type>
  friend class test::MockNetwork;

 private:
  Network(const Network&);
  Network(const Network&&);
  Network& operator=(const Network&);

  // For zero-state, local_endpoint can be default-constructed.
  int DoBootstrap(const rudp::MessageReceivedFunctor& message_received_functor,
                  const rudp::ConnectionLostFunctor& connection_lost_functor,
                  const BootstrapContacts& bootstrap_contacts,
                  LocalEndpoint local_endpoint = LocalEndpoint(boost::asio::ip::udp::endpoint()));

  void RudpSend(const NodeId& peer_id, const protobuf::Message& message,
                const rudp::MessageSentFunctor& message_sent_functor);
  void SendTo(const protobuf::Message& message, const NodeId& peer_node_id,
              const NodeId& peer_connection_id);
  void RecursiveSendOn(protobuf::Message message, NodeInfo last_node_attempted = NodeInfo(),
                       int attempt_count = 0);
  void AdjustRouteHistory(protobuf::Message& message);

  bool running_;
  std::mutex running_mutex_;
  uint16_t bootstrap_attempt_;
  BootstrapContacts bootstrap_contacts_;
  NodeId bootstrap_connection_id_;
  NodeId this_node_relay_connection_id_;
  Connections<NodeType>& connections_;
  rudp::NatType nat_type_;
  NewBootstrapContactFunctor new_bootstrap_contact_;
  rudp::ManagedConnections rudp_;
};

template <typename NodeType>
Network<NodeType>::Network(Connections<NodeType>& connections)
    : running_(true),
      running_mutex_(),
      bootstrap_attempt_(0),
      bootstrap_contacts_(),
      bootstrap_connection_id_(),
      this_node_relay_connection_id_(),
      connections_(connections),
      nat_type_(rudp::NatType::kUnknown),
      new_bootstrap_contact_(),
      rudp_() {}

template <typename NodeType>
Network<NodeType>::~Network() {
  std::lock_guard<std::mutex> lock(running_mutex_);
  running_ = false;
}

template <typename NodeType>
int Network<NodeType>::Bootstrap(const rudp::MessageReceivedFunctor& message_received_functor,
                                      const rudp::ConnectionLostFunctor& connection_lost_functor) {
  BootstrapContacts bootstrap_contacts{GetBootstrapContacts(NodeType::value)};
  return DoBootstrap(message_received_functor, connection_lost_functor, bootstrap_contacts);
}

template <typename NodeType>
int Network<NodeType>::ZeroStateBootstrap(
    const rudp::MessageReceivedFunctor& message_received_functor,
    const rudp::ConnectionLostFunctor& connection_lost_functor, LocalEndpoint local_endpoint) {
  BootstrapContacts bootstrap_contacts{GetZeroStateBootstrapContacts(local_endpoint.data)};
  for (const auto& bootstrap : bootstrap_contacts)
    LOG(kVerbose) << bootstrap.port();
  return DoBootstrap(message_received_functor, connection_lost_functor, bootstrap_contacts,
                     local_endpoint);
}

template <typename NodeType>
int Network<NodeType>::DoBootstrap(
    const rudp::MessageReceivedFunctor& message_received_functor,
    const rudp::ConnectionLostFunctor& connection_lost_functor,
    const BootstrapContacts& bootstrap_contacts, LocalEndpoint local_endpoint) {
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return kNetworkShuttingDown;
  }

  if (bootstrap_contacts.empty())
    return kInvalidBootstrapContacts;

  assert(connection_lost_functor && "Must provide a valid functor");
  assert(bootstrap_connection_id_.IsZero() && "bootstrap_connection_id_ must be empty");
  auto private_key(std::make_shared<asymm::PrivateKey>(connections_.routing_table.kPrivateKey()));
  auto public_key(std::make_shared<asymm::PublicKey>(connections_.routing_table.kPublicKey()));

  int result(rudp_.Bootstrap(/* sorted_ */ bootstrap_contacts, message_received_functor,
                             connection_lost_functor, connections_.kConnectionId(), private_key,
                             public_key, bootstrap_connection_id_, nat_type_, local_endpoint.data));
  // RUDP will return a kZeroId for zero state !!
  if (result != kSuccess || bootstrap_connection_id_.IsZero()) {
    LOG(kError) << "No Online Bootstrap Node found.";
    return kNoOnlineBootstrapContacts;
  }

  this_node_relay_connection_id_ = connections_.kConnectionId();
  LOG(kInfo) << "Bootstrap successful, bootstrap connection id - "
             << DebugId(bootstrap_connection_id_);
  return kSuccess;
}

template <typename NodeType>
int Network<NodeType>::GetAvailableEndpoint(const NodeId& peer_id,
                                                 const rudp::EndpointPair& peer_endpoint_pair,
                                                 rudp::EndpointPair& this_endpoint_pair,
                                                 rudp::NatType& this_nat_type) {
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return kNetworkShuttingDown;
  }
  return rudp_.GetAvailableEndpoint(peer_id, peer_endpoint_pair, this_endpoint_pair, this_nat_type);
}

template <typename NodeType>
int Network<NodeType>::Add(const NodeId& peer_id, const rudp::EndpointPair& peer_endpoint_pair,
                                const std::string& validation_data) {
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return kNetworkShuttingDown;
  }
  return rudp_.Add(peer_id, peer_endpoint_pair, validation_data);
}

template <typename NodeType>
int Network<NodeType>::MarkConnectionAsValid(const NodeId& peer_id) {
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return kNetworkShuttingDown;
  }
  Endpoint new_bootstrap_endpoint;
  int ret_val(rudp_.MarkConnectionAsValid(peer_id, new_bootstrap_endpoint));
  if ((ret_val == kSuccess) && !new_bootstrap_endpoint.address().is_unspecified()) {
    LOG(kVerbose) << "Found usable endpoint for bootstrapping : " << new_bootstrap_endpoint;
    // TODO(Prakash): Is separate thread needed here ?
    if (new_bootstrap_contact_)
      new_bootstrap_contact_(new_bootstrap_endpoint);
  }
  return ret_val;
}

template <typename NodeType>
void Network<NodeType>::Remove(const NodeId& peer_id) {
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
  }
  rudp_.Remove(peer_id);
}

template <typename NodeType>
void Network<NodeType>::RudpSend(const NodeId& peer_id, const protobuf::Message& message,
                                      const rudp::MessageSentFunctor& message_sent_functor) {
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
  }
  rudp_.Send(peer_id, message.SerializeAsString(), message_sent_functor);
  LOG(kVerbose) << "  [" << connections_.kNodeId() << "] send : " << MessageTypeString(message)
                << " to   " << DebugId(peer_id) << "   (id: " << message.id() << ")"
                << " --To Rudp--";
}

template <typename NodeType>
void Network<NodeType>::SendToDirect(const protobuf::Message& message,
                                          const NodeId& peer_connection_id,
                                          const rudp::MessageSentFunctor& message_sent_functor) {
  RudpSend(peer_connection_id, message, message_sent_functor ? message_sent_functor : nullptr);
}

template <typename NodeType>
void Network<NodeType>::SendToDirect(const protobuf::Message& message,
                                          const NodeId& peer_node_id,
                                          const NodeId& peer_connection_id) {
  SendTo(message, peer_node_id, peer_connection_id);
}

template <typename NodeType>
void Network<NodeType>::SendToDirectAdjustedRoute(protobuf::Message& message,
                                                       const NodeId& peer_node_id,
                                                       const NodeId& peer_connection_id) {
  AdjustRouteHistory(message);
  SendTo(message, peer_node_id, peer_connection_id);
}

template <typename NodeType>
void Network<NodeType>::SendToClosestNode(const protobuf::Message& message) {
  // Normal messages
  if (message.has_destination_id() && !message.destination_id().empty()) {
    auto client_routing_nodes(
        connections_.client_routing_table.GetNodesInfo(NodeId(message.destination_id())));
    // have the destination ID in non-routing table
    if (!client_routing_nodes.empty() && message.direct()) {
      if (IsClientToClientMessageWithDifferentNodeIds(message, true)) {
        LOG(kWarning) << "This node [" << connections_.kNodeId()
                      << " Dropping message as client to client message not allowed."
                      << PrintMessage(message);
        return;
      }
      LOG(kVerbose) << "This node [" << connections_.kNodeId() << "] has "
                    << client_routing_nodes.size()
                    << " destination node(s) in its non-routing table."
                    << " id: " << message.id();

      for (const auto& i : client_routing_nodes) {
        LOG(kVerbose) << "Sending message to NRT node with ID " << message.id() << " node_id "
                      << DebugId(i.id) << " connection id " << DebugId(i.connection_id);
        SendTo(message, i.id, i.connection_id);
      }
    } else if (connections_.routing_table.size() > 0) {  // getting closer nodes from routing table
      RecursiveSendOn(message);
    } else {
      LOG(kError) << " No endpoint to send to; aborting send.  Attempt to send a type "
                  << MessageTypeString(message) << " message to " << HexSubstr(message.source_id())
                  << " from " << connections_.kNodeId() << " id: " << message.id();
    }
    return;
  }

  // Relay message responses only
  if (message.has_relay_id() /*&& (IsResponse(message))*/) {
    protobuf::Message relay_message(message);
    relay_message.set_destination_id(message.relay_id());  // so that peer identifies it as direct
    SendTo(relay_message, NodeId(relay_message.relay_id()),
           NodeId(relay_message.relay_connection_id()));
  } else {
    LOG(kError) << "Unable to work out destination; aborting send."
                << " id: " << message.id() << " message.has_relay_id() ; " << std::boolalpha
                << message.has_relay_id() << " Isresponse(message) : " << std::boolalpha
                << IsResponse(message) << " message.has_relay_connection_id() : " << std::boolalpha
                << message.has_relay_connection_id();
  }
}

template <>
void Network<ClientNode>::SendToClosestNode(const protobuf::Message& message);

template <typename NodeType>
void Network<NodeType>::SendTo(const protobuf::Message& message, const NodeId& peer_node_id,
                                    const NodeId& peer_connection_id) {
  const std::string kThisId(connections_.kNodeId()->string());
  rudp::MessageSentFunctor message_sent_functor = [=](int message_sent) {
    if (rudp::kSuccess == message_sent) {
      LOG(kVerbose) << "  [" << HexSubstr(kThisId) << "] sent : " << MessageTypeString(message)
                    << " to   " << DebugId(peer_node_id) << "   (id: " << message.id() << ")";
    } else {
      LOG(kError) << "Sending type " << MessageTypeString(message) << " message from "
                  << HexSubstr(kThisId) << " to " << DebugId(peer_node_id) << " failed with code "
                  << message_sent << " id: " << message.id();
    }
  };
  LOG(kVerbose) << " >>>>>>>>> rudp send message to connection id " << DebugId(peer_connection_id);
  RudpSend(peer_connection_id, message, message_sent_functor);
}

template <typename NodeType>
void Network<NodeType>::RecursiveSendOn(protobuf::Message message,
                                             NodeInfo last_node_attempted, int attempt_count) {
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
  }
  if (attempt_count >= 3) {
    LOG(kWarning) << " Retry attempts failed to send to ["
                  << HexSubstr(last_node_attempted.id.string())
                  << "] will drop this node now and try with another node."
                  << " id: " << message.id();
    attempt_count = 0;
    {
      std::lock_guard<std::mutex> lock(running_mutex_);
      if (!running_)
        return;
      rudp_.Remove(last_node_attempted.connection_id);
      LOG(kWarning) << " Routing -> removing connection " << last_node_attempted.id.string();
      // FIXME Should we remove this node or let rudp handle that?
      connections_.routing_table.DropNode(last_node_attempted.connection_id, false);
      connections_.client_routing_table.DropConnection(last_node_attempted.connection_id);
    }
  }

  if (attempt_count > 0)
    Sleep(std::chrono::milliseconds(50));

  const std::string kThisId(connections_.kNodeId()->string());
  bool ignore_exact_match(!IsDirect(message));
  std::vector<std::string> route_history;
  NodeInfo peer;
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
    if (message.route_history().size() > 1)
      route_history = std::vector<std::string>(
          message.route_history().begin(),
          message.route_history().end() -
              static_cast<size_t>(!(message.has_visited() && message.visited())));
    else if ((message.route_history().size() == 1) &&
             (message.route_history(0) != connections_.kNodeId()->string()))
      route_history.push_back(message.route_history(0));

    peer = connections_.routing_table.GetClosestNode(NodeId(message.destination_id()),
                                                     ignore_exact_match, route_history);
    if (peer.id == NodeId() && connections_.routing_table.size() != 0) {
      peer = connections_.routing_table.GetClosestNode(NodeId(message.destination_id()),
                                                       ignore_exact_match);
    }
    if (peer.id == NodeId()) {
      LOG(kError) << "This node's routing table is empty now.  Need to re-bootstrap.";
      return;
    }
    AdjustRouteHistory(message);
  }

  rudp::MessageSentFunctor message_sent_functor = [=](int message_sent) {
    {
      std::lock_guard<std::mutex> lock(running_mutex_);
      if (!running_)
        return;
    }
    if (rudp::kSuccess == message_sent) {
      LOG(kVerbose) << "  [" << HexSubstr(kThisId) << "] sent : " << MessageTypeString(message)
                    << " to   " << HexSubstr(peer.id.string()) << "   (id: " << message.id() << ")"
                    << " dst : " << HexSubstr(message.destination_id());
    } else if (rudp::kSendFailure == message_sent) {
      LOG(kError) << "Sending type " << MessageTypeString(message) << " message from "
                  << connections_.kNodeId() << " to " << HexSubstr(peer.id.string())
                  << " with destination ID " << HexSubstr(message.destination_id())
                  << " failed with code " << message_sent
                  << ".  Will retry to Send.  Attempt count = " << attempt_count + 1
                  << " id: " << message.id();
      RecursiveSendOn(message, peer, attempt_count + 1);
    } else {
      LOG(kError) << "Sending type " << MessageTypeString(message) << " message from "
                  << HexSubstr(kThisId) << " to " << HexSubstr(peer.id.string())
                  << " with destination ID " << HexSubstr(message.destination_id())
                  << " failed with code " << message_sent << "  Will remove node."
                  << " message id: " << message.id();
      {
        std::lock_guard<std::mutex> lock(running_mutex_);
        if (!running_)
          return;
        rudp_.Remove(last_node_attempted.connection_id);
      }
      LOG(kWarning) << " Routing-> removing connection " << DebugId(peer.connection_id);
      connections_.routing_table.DropNode(peer.id, false);
      connections_.client_routing_table.DropConnection(peer.connection_id);
      RecursiveSendOn(message);
    }
  };
  LOG(kVerbose) << "Rudp recursive send message to " << DebugId(peer.connection_id);
  RudpSend(peer.connection_id, message, message_sent_functor);
}

template <>
void Network<ClientNode>::RecursiveSendOn(protobuf::Message message,
                                               NodeInfo last_node_attempted, int attempt_count);


template <typename NodeType>
void Network<NodeType>::set_new_bootstrap_contact_functor(
    NewBootstrapContactFunctor new_bootstrap_contact) {
  new_bootstrap_contact_ = new_bootstrap_contact;
}

template <typename NodeType>
void Network<NodeType>::clear_bootstrap_connection_info() {
  bootstrap_connection_id_ = NodeId();
  this_node_relay_connection_id_ = NodeId();
}

template <typename NodeType>
maidsafe::NodeId Network<NodeType>::bootstrap_connection_id() const {
  if (running_)
    return bootstrap_connection_id_;
  return NodeId();
}

template <typename NodeType>
maidsafe::NodeId Network<NodeType>::this_node_relay_connection_id() const {
  return this_node_relay_connection_id_;
}

template <typename NodeType>
rudp::NatType Network<NodeType>::nat_type() const {
  return nat_type_;
}

template <typename NodeType>
void Network<NodeType>::AdjustRouteHistory(protobuf::Message& message) {
  if (Parameters::hops_to_live == message.hops_to_live() &&
      NodeId(message.source_id()) == connections_.kNodeId())
    return;
  assert(message.route_history().size() <= Parameters::max_route_history);
  if (std::find(message.route_history().begin(), message.route_history().end(),
                connections_.kNodeId()->string()) == message.route_history().end()) {
    message.add_route_history(connections_.kNodeId()->string());
    if (message.route_history().size() > Parameters::max_route_history) {
      std::vector<std::string> route_history(message.route_history().begin() + 1,
                                             message.route_history().end());
      message.clear_route_history();
      for (const auto& route : route_history) {
        if (!NodeId(route).IsZero())
          message.add_route_history(route);
      }
    }
  }
  assert(message.route_history().size() <= Parameters::max_route_history);
}


}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_NETWORK_H_
