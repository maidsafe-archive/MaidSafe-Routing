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

#include "maidsafe/routing/network.h"

#include "boost/date_time/posix_time/posix_time_config.hpp"
#include "boost/filesystem/path.hpp"

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/bootstrap_file_operations.h"
#include "maidsafe/routing/bootstrap_utils.h"
#include "maidsafe/routing/client_routing_table.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/utils.h"
#include "maidsafe/routing/acknowledgement.h"
#include "maidsafe/routing/rpcs.h"

namespace bptime = boost::posix_time;

namespace maidsafe {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // anonymous namespace

namespace routing {

Network::Network(RoutingTable& routing_table, ClientRoutingTable& client_routing_table,
                 Acknowledgement& acknowledgement)
    : running_(true),
      running_mutex_(),
      bootstrap_attempt_(0),
      bootstrap_connection_id_(),
      this_node_relay_connection_id_(),
      routing_table_(routing_table),
      client_routing_table_(client_routing_table),
      acknowledgement_(acknowledgement),
      nat_type_(rudp::NatType::kUnknown),
      rudp_() {}

Network::~Network() {
  std::lock_guard<std::mutex> lock(running_mutex_);
  running_ = false;
}

int Network::Bootstrap(const rudp::MessageReceivedFunctor& message_received_functor,
                            const rudp::ConnectionLostFunctor& connection_lost_functor) {
  BootstrapContacts bootstrap_contacts{ GetBootstrapContacts(routing_table_.client_mode()) };
  return DoBootstrap(message_received_functor, connection_lost_functor, bootstrap_contacts);
}

int Network::ZeroStateBootstrap(const rudp::MessageReceivedFunctor& message_received_functor,
                                     const rudp::ConnectionLostFunctor& connection_lost_functor,
                                     boost::asio::ip::udp::endpoint local_endpoint) {
  BootstrapContacts bootstrap_contacts{ GetZeroStateBootstrapContacts(local_endpoint) };
  return DoBootstrap(message_received_functor, connection_lost_functor, bootstrap_contacts,
                     local_endpoint);
}

int Network::DoBootstrap(const rudp::MessageReceivedFunctor& message_received_functor,
                              const rudp::ConnectionLostFunctor& connection_lost_functor,
                              const BootstrapContacts& bootstrap_contacts,
                              boost::asio::ip::udp::endpoint local_endpoint) {
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return kNetworkShuttingDown;
  }

  if (bootstrap_contacts.empty())
    return kInvalidBootstrapContacts;

  assert(connection_lost_functor && "Must provide a valid functor");
  assert(bootstrap_connection_id_.IsZero() && "bootstrap_connection_id_ must be empty");
  auto private_key(std::make_shared<asymm::PrivateKey>(routing_table_.kPrivateKey()));
  auto public_key(std::make_shared<asymm::PublicKey>(routing_table_.kPublicKey()));

  int result(rudp_.Bootstrap(/* sorted_ */ bootstrap_contacts, message_received_functor,
                             connection_lost_functor, routing_table_.kConnectionId(), private_key,
                             public_key, bootstrap_connection_id_, nat_type_, local_endpoint));
  // RUDP will return a kZeroId for zero state !!
  if (result != kSuccess || bootstrap_connection_id_.IsZero()) {
    LOG(kError) << "No Online Bootstrap Node found.";
    return kNoOnlineBootstrapContacts;
  }

  this_node_relay_connection_id_ = routing_table_.kConnectionId();
  return kSuccess;
}

int Network::GetAvailableEndpoint(const NodeId& peer_id,
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

int Network::Add(const NodeId& peer_id, const rudp::EndpointPair& peer_endpoint_pair,
                      const std::string& validation_data) {
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return kNetworkShuttingDown;
  }
  return rudp_.Add(peer_id, peer_endpoint_pair, validation_data);
}

int Network::MarkConnectionAsValid(const NodeId& peer_id) {
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return kNetworkShuttingDown;
  }
  Endpoint new_bootstrap_endpoint;
  int ret_val(rudp_.MarkConnectionAsValid(peer_id, new_bootstrap_endpoint));
  if ((ret_val == kSuccess) && !new_bootstrap_endpoint.address().is_unspecified()) {
    InsertOrUpdateBootstrapContact(new_bootstrap_endpoint, routing_table_.client_mode());
  }
  return ret_val;
}

void Network::Remove(const NodeId& peer_id) {
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
  }
  rudp_.Remove(peer_id);
}

void Network::RudpSend(const NodeId& peer_id, const protobuf::Message& message,
                            const rudp::MessageSentFunctor& message_sent_functor) {
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
  }
  rudp_.Send(peer_id, message.SerializeAsString(), message_sent_functor);
}

void Network::SendToDirect(const protobuf::Message& message, const NodeId& peer_connection_id,
                           const rudp::MessageSentFunctor& message_sent_functor) {
  RudpSend(peer_connection_id, message, message_sent_functor ? message_sent_functor : nullptr);
}

void Network::SendToDirect(protobuf::Message& message, const NodeId& peer_node_id,
                           const NodeId& peer_connection_id) {
  AdjustRouteHistory(message);
  SendTo(message, peer_node_id, peer_connection_id);
}

void Network::SendToDirectAdjustedRoute(protobuf::Message& message, const NodeId& peer_node_id,
                                             const NodeId& peer_connection_id) {
  AdjustRouteHistory(message);
  SendTo(message, peer_node_id, peer_connection_id);
}

void Network::SendToClosestNode(const protobuf::Message& message) {
  // Normal messages
  if (message.has_destination_id() && !message.destination_id().empty()) {
    auto client_routing_nodes(client_routing_table_.GetNodesInfo(NodeId(message.destination_id())));
    // have the destination ID in non-routing table
    if (!client_routing_nodes.empty() && message.direct()) {
      if (IsClientToClientMessageWithDifferentNodeIds(message, true)) {
        LOG(kWarning) << "This node [" << DebugId(routing_table_.kNodeId())
                      << " Dropping message as client to client message not allowed."
                      << PrintMessage(message);
        return;
      }
      for (const auto& i : client_routing_nodes) {
        SendTo(message, i.id, i.connection_id);
      }
    } else if (routing_table_.size() > 0) {  // getting closer nodes from routing table
      RecursiveSendOn(message);
    } else {
      LOG(kError) << " No endpoint to send to; aborting send.  Attempt to send a type "
                  << MessageTypeString(message) << " message to " << HexSubstr(message.source_id())
                  << " from " << DebugId(routing_table_.kNodeId()) << " id: " << message.id();
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

void Network::SendTo(const protobuf::Message& message, const NodeId& peer_node_id,
                          const NodeId& peer_connection_id,  bool no_ack_timer) {
  const std::string kThisId(routing_table_.kNodeId().string());
  rudp::MessageSentFunctor message_sent_functor = [=](int message_sent) {
    if (rudp::kSuccess == message_sent) {
      SendAck(message);
    } else {
      LOG(kError) << "Sending type " << MessageTypeString(message) << " message from "
                  << HexSubstr(kThisId) << " to " << peer_node_id << " failed with code "
                  << message_sent << " id: " << message.id();
    }
  };

  if (!no_ack_timer && acknowledgement_.NeedsAck(message, peer_connection_id)) {
    acknowledgement_.Add(message,
                         [=](const boost::system::error_code& error) {
                           {
                             std::lock_guard<std::mutex> lock(running_mutex_);
                             if (!running_)
                               return;
                           }
                           if (!error)
                             SendTo(message, peer_node_id, peer_connection_id);
                         }, Parameters::ack_timeout);
  }
  RudpSend(peer_connection_id, message, message_sent_functor);
}

void Network::RecursiveSendOn(protobuf::Message message, NodeInfo last_node_attempted,
                                   int attempt_count) {
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
      routing_table_.DropNode(last_node_attempted.connection_id, false);
      client_routing_table_.DropConnection(last_node_attempted.connection_id);
    }
  }

  if (attempt_count > 0)
    Sleep(std::chrono::milliseconds(50));

  const std::string kThisId(routing_table_.kNodeId().string());
  bool ignore_exact_match(!IsDirect(message));
  std::vector<std::string> route_history;
  NodeInfo peer;
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
    if (message.route_history().size() > 1)
      route_history = std::vector<std::string>(
          message.route_history().begin(), message.route_history().end());
    else if ((message.route_history().size() == 1) &&
             (message.route_history(0) != routing_table_.kNodeId().string()))
      route_history.push_back(message.route_history(0));

    peer = routing_table_.GetClosestNode(NodeId(message.destination_id()), ignore_exact_match,
                                         route_history);
    if (peer.id == NodeId() && routing_table_.size() != 0) {
      peer = routing_table_.GetClosestNode(NodeId(message.destination_id()), ignore_exact_match);
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
      SendAck(message);
    } else if (rudp::kSendFailure == message_sent) {
      LOG(kError) << "Sending type " << MessageTypeString(message) << " message from "
                  << HexSubstr(routing_table_.kNodeId().string()) << " to "
                  << HexSubstr(peer.id.string()) << " with destination ID "
                  << HexSubstr(message.destination_id()) << " failed with code " << message_sent
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
      routing_table_.DropNode(peer.id, false);
      client_routing_table_.DropConnection(peer.connection_id);
      RecursiveSendOn(message);
    }
  };

  if (acknowledgement_.NeedsAck(message, peer.id)) {
    acknowledgement_.Add(message,
                        [=](const boost::system::error_code& error) {
                          if (error.value() == boost::system::errc::success)
                            RecursiveSendOn(message);
                        }, Parameters::ack_timeout);
  }
  RudpSend(peer.connection_id, message, message_sent_functor);
}

void Network::AdjustRouteHistory(protobuf::Message& message) {
  if (message.source_id().empty())
    return;

  acknowledgement_.AdjustAckHistory(message);

  if (Parameters::hops_to_live == static_cast<unsigned int>(message.hops_to_live()) &&
      NodeId(message.source_id()) == routing_table_.kNodeId())
    return;
  assert(static_cast<unsigned int>(message.route_history().size()) <=
             Parameters::max_routing_table_size);
  if (std::find(message.route_history().begin(), message.route_history().end(),
                routing_table_.kNodeId().string()) == message.route_history().end()) {
    message.add_route_history(routing_table_.kNodeId().string());
    if (static_cast<unsigned int>(message.route_history().size()) > Parameters::max_route_history) {
      std::vector<std::string> route_history(message.route_history().begin() + 1,
                                             message.route_history().end());
      message.clear_route_history();
      for (const auto& route : route_history) {
        if (!NodeId(route).IsZero())
          message.add_route_history(route);
      }
    }
  }
  assert(static_cast<unsigned int>(message.route_history().size()) <=
             Parameters::max_routing_table_size);
}


void Network::clear_bootstrap_connection_info() {
  bootstrap_connection_id_ = NodeId();
  this_node_relay_connection_id_ = NodeId();
}

maidsafe::NodeId Network::bootstrap_connection_id() const {
  if (running_)
    return bootstrap_connection_id_;
  return NodeId();
}

maidsafe::NodeId Network::this_node_relay_connection_id() const {
  return this_node_relay_connection_id_;
}

rudp::NatType Network::nat_type() const { return nat_type_; }

void Network::SendAck(const protobuf::Message message) {
  if (message.ack_id() == 0)
    return;

  if (IsAck(message) || IsConnectSuccessAcknowledgement(message))
    return;

  if (message.source_id() == routing_table_.kNodeId().string())
    return;

  if (message.direct() && (message.destination_id() == message.source_id()))
    return;

  std::vector<std::string> ack_node_ids(message.ack_node_ids().begin(),
                                        message.ack_node_ids().end());
  if (message.ack_node_ids_size() == 0)
    return;

  if ((message.relay_id() == routing_table_.kNodeId().string()) ||
      message.source_id() == routing_table_.kNodeId().string())
    return;

  if (std::find(std::begin(ack_node_ids), std::end(ack_node_ids),
                routing_table_.kNodeId().string()) != std::end(ack_node_ids)) {
    acknowledgement_.Remove(message.ack_id());
  }

  protobuf::Message ack_message(rpcs::Ack(NodeId(message.ack_node_ids(0)), routing_table_.kNodeId(),
                                          message.ack_id()));
  SendToClosestNode(ack_message);
}

}  // namespace routing

}  // namespace maidsafe
