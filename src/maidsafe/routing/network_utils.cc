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

#include "maidsafe/routing/network_utils.h"

#include "boost/date_time/posix_time/posix_time_config.hpp"

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/utils.h"


namespace bptime = boost::posix_time;

namespace maidsafe {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;
typedef boost::shared_lock<boost::shared_mutex> SharedLock;
typedef boost::unique_lock<boost::shared_mutex> UniqueLock;

}  // anonymous namespace

namespace routing {

NetworkUtils::NetworkUtils(RoutingTable& routing_table, NonRoutingTable& non_routing_table,
                           Timer& timer)
    : bootstrap_endpoint_(),
      this_node_relay_endpoint_(),
      connection_lost_functor_(),
      routing_table_(routing_table),
      non_routing_table_(non_routing_table),
      timer_(timer),
      rudp_(new rudp::ManagedConnections),
      shared_mutex_(),
      stopped_(false),
      nat_type_(rudp::NatType::kUnknown) {}

void NetworkUtils::Stop() {
  LOG(kVerbose) << "NetworkUtils::Stop()";
  {
    UniqueLock unique_lock(shared_mutex_);
    stopped_ = true;
    boost::this_thread::disable_interruption disable_interruption;
    rudp_.reset();
  }
  LOG(kVerbose) << "NetworkUtils::Stop(), exiting ...";
}

void NetworkUtils::OnConnectionLost(const Endpoint& endpoint) {
  if (connection_lost_functor_)
    connection_lost_functor_(endpoint);
}

int NetworkUtils::Bootstrap(const std::vector<Endpoint> &bootstrap_endpoints,
                            rudp::MessageReceivedFunctor message_received_functor,
                            rudp::ConnectionLostFunctor connection_lost_functor,
                            Endpoint local_endpoint) {
  assert(connection_lost_functor && "Must provide a valid functor");

  std::shared_ptr<asymm::PrivateKey>
      private_key(new asymm::PrivateKey(routing_table_.kKeys().private_key));
  std::shared_ptr<asymm::PublicKey>
      public_key(new asymm::PublicKey(routing_table_.kKeys().public_key));

  connection_lost_functor_ = connection_lost_functor;

  bootstrap_endpoint_ = rudp_->Bootstrap(bootstrap_endpoints,
                                         message_received_functor,
                                         [&](const Endpoint& endpoint) {
                                             OnConnectionLost(endpoint); },
                                         private_key,
                                         public_key,
                                         nat_type_,
                                         local_endpoint);

  if (bootstrap_endpoint_.address().is_unspecified()) {
    LOG(kError) << "No Online Bootstrap Node found.";
    return kNoOnlineBootstrapContacts;
  }

  if (!local_endpoint.address().is_unspecified()) {  // Zero state case
    this_node_relay_endpoint_ = local_endpoint;
    LOG(kVerbose) << "Zero state Bootstrap successful, bootstrap node - " << bootstrap_endpoint_;
    return kSuccess;
  }

  rudp::EndpointPair endpoint_pair;
  if (kSuccess != rudp_->GetAvailableEndpoint(bootstrap_endpoint_, endpoint_pair,
                                              nat_type_)) {
    LOG(kError) << " Failed to get available endpoint for new connections";
    return kFailedtoGetEndpoint;
  }
  LOG(kVerbose) << "Getavailable endpoint returned, endpoint_pair.external - "
                << endpoint_pair.external
                << " , endpoint_pair.local - " << endpoint_pair.local;
  if (!endpoint_pair.local.address().is_unspecified())
    this_node_relay_endpoint_ = endpoint_pair.local;
  else if (!endpoint_pair.external.address().is_unspecified())
    this_node_relay_endpoint_ = endpoint_pair.external;
  else
    return kFailedtoGetEndpoint;

  LOG(kVerbose) << "Bootstrap successful, bootstrap endpoint - " << bootstrap_endpoint_
                << " , my relay endpoint - " << this_node_relay_endpoint_;
  return kSuccess;
}

int NetworkUtils::GetAvailableEndpoint(const Endpoint& peer_endpoint,
                                       rudp::EndpointPair& this_endpoint_pair,
                                       rudp::NatType& this_nat_type) {
  return rudp_->GetAvailableEndpoint(peer_endpoint, this_endpoint_pair, this_nat_type);
}

int NetworkUtils::Add(const Endpoint& this_endpoint,
                      const Endpoint& peer_endpoint,
                      const std::string& validation_data) {
  return rudp_->Add(this_endpoint, peer_endpoint, validation_data);
}

void NetworkUtils::Remove(const Endpoint& peer_endpoint) {
  rudp_->Remove(peer_endpoint);
}

void NetworkUtils::RudpSend(const protobuf::Message& message,
                            Endpoint endpoint,
                            rudp::MessageSentFunctor message_sent_functor) {
  SharedLock shared_lock(shared_mutex_);
  if (!stopped_)
    rudp_->Send(endpoint, message.SerializeAsString(), message_sent_functor);
}

void NetworkUtils::SendToDirectEndpoint(const protobuf::Message& message,
                                        Endpoint direct_endpoint,
                                        rudp::MessageSentFunctor message_sent_functor) {
  RudpSend(message, direct_endpoint, message_sent_functor);
}

void NetworkUtils::SendToDirectEndpoint(const protobuf::Message& message,
                                        Endpoint direct_endpoint) {
  NodeId node_id;
  if (message.has_destination_id())
    node_id = NodeId(message.destination_id());
  SendTo(message, node_id, direct_endpoint);
}

void NetworkUtils::SendToClosestNode(const protobuf::Message& message) {
  // Normal messages
  if (message.has_destination_id()) {  // message has destination ID
    auto non_routing_nodes(non_routing_table_.GetNodesInfo(NodeId(message.destination_id())));
    // have the destination ID in non-routing table
    if (!non_routing_nodes.empty() && IsResponse(message)) {
      LOG(kInfo) << "This node has the destination node in its non-routing table."
                 << " id: " << message.id();
      for (auto i : non_routing_nodes) {
        LOG(kVerbose) << "Sending message to NRT node with ID endpoint " << i.endpoint
                      << " id: " << message.id();
        SendTo(message, i.node_id, i.endpoint);
      }
    } else if (routing_table_.Size() > 0) {  // getting closer nodes from routing table
      RecursiveSendOn(message);
    } else {
      LOG(kError) << " No endpoint to send to; aborting send.  Attempt to send a type "
                  << message.type() << " message to " << HexSubstr(message.source_id())
                  << " from " << HexSubstr(routing_table_.kKeys().identity)
                  << " id: " << message.id();
    }
    return;
  }

  // Relay message responses only
  if (message.has_relay_id() && (IsResponse(message)) && message.has_relay()) {
    Endpoint direct_endpoint = GetEndpointFromProtobuf(message.relay());
    protobuf::Message relay_message(message);
    relay_message.set_destination_id(message.relay_id());  // so that peer identifies it as direct
    SendTo(relay_message, NodeId(relay_message.relay_id()), direct_endpoint);
  } else {
    LOG(kError) << "Unable to work out destination; aborting send." << " id: " << message.id()
    << " message.has_relay_id() ; " << std::boolalpha << message.has_relay_id()
    << " Isresponse(message) : " << std::boolalpha << IsResponse(message)
    << " message.has_relay() : "  << std::boolalpha << message.has_relay();
  }
}

void NetworkUtils::SendTo(const protobuf::Message& message,
                          const NodeId& node_id,
                          const Endpoint& endpoint) {
  const std::string kThisId(HexSubstr(routing_table_.kKeys().identity));
  rudp::MessageSentFunctor message_sent_functor = [=](int message_sent) {
      if (rudp::kSuccess == message_sent) {
        LOG(kInfo) << "Type " << message.type() << " message successfully sent from "
                   << kThisId << " to " << HexSubstr(node_id.String()) << " with destination ID "
                   << HexSubstr(message.destination_id()) << " id: " << message.id();
      } else {
        LOG(kError) << "Sending type " << message.type() << " message from "
                    << kThisId << " to " << HexSubstr(node_id.String()) << " with destination ID "
                    << HexSubstr(message.destination_id()) << " failed with code " << message_sent
                    << " id: " << message.id();
      }
  };
  LOG(kVerbose) << " >>>>>>>>> rudp send message to " << endpoint << " <<<<<<<<<<<<<<<<<<<<";
  RudpSend(message, endpoint, message_sent_functor);
}

void NetworkUtils::RecursiveSendOn(protobuf::Message message,
                                   NodeInfo last_node_attempted,
                                   int attempt_count) {
  {
    SharedLock shared_lock(shared_mutex_);
     if (stopped_)
       return;
  }

  if (attempt_count >= 3) {
    LOG(kWarning) << " Retry attempts failed to send to ["
                  << HexSubstr(last_node_attempted.node_id.String())
                  << "] will drop this node now and try with another node."
                  << " id: " << message.id();
    attempt_count = 0;

    {
      SharedLock shared_lock(shared_mutex_);
      if (stopped_)
        return;
      else
        rudp_->Remove(last_node_attempted.endpoint);
    }
    OnConnectionLost(last_node_attempted.endpoint);
  }

  if (attempt_count > 0)
    Sleep(bptime::milliseconds(50));

  const std::string kThisId(HexSubstr(routing_table_.kKeys().identity));

  bool ignore_exact_match(!IsDirect(message));

  std::vector<std::string> route_history;
  if (message.route_history().size() > 1)
    route_history = std::vector<std::string>(message.route_history().begin(),
                                             message.route_history().end() - 1);
  else if ((message.route_history().size() == 1) &&
           (message.route_history(0) != routing_table_.kKeys().identity))
    route_history.push_back(message.route_history(0));
  auto closest_node(routing_table_.GetClosestNode(NodeId(message.destination_id()), route_history,
                                                  ignore_exact_match, true));
  if (closest_node.node_id == NodeId()) {
    LOG(kError) << "This node's routing table is empty now.  Need to re-bootstrap.";
    return;
  }

  AdjustRouteHistory(message);

  rudp::MessageSentFunctor message_sent_functor = [=](int message_sent) {
      if (rudp::kSuccess == message_sent) {
        LOG(kInfo) << "Type " << MessageTypeString(message) << " message successfully sent from "
                   << kThisId << " to " << HexSubstr(closest_node.node_id.String())
                   << " with destination ID " << HexSubstr(message.destination_id())
                   << " id: " << message.id();
      } else if (rudp::kSendFailure == message_sent) {
        LOG(kError) << "Sending type " << MessageTypeString(message) << " message from "
                    << kThisId << " to " << HexSubstr(closest_node.node_id.String())
                    << " with destination ID " << HexSubstr(message.destination_id())
                    << " failed with code " << message_sent
                    << ".  Will retry to Send.  Attempt count = " << attempt_count + 1
                     << " id: " << message.id();
        RecursiveSendOn(message, closest_node, attempt_count + 1);
      } else {
        LOG(kError) << "Sending type " << MessageTypeString(message) << " message from "
                    << kThisId << " to " << HexSubstr(closest_node.node_id.String())
                    << " with destination ID " << HexSubstr(message.destination_id())
                    << " failed with code " << message_sent << "  Will remove node."
                     << " id: " << message.id();
        {
          SharedLock shared_lock(this->shared_mutex_);
          if (stopped_)
            return;
          else
           rudp_->Remove(closest_node.endpoint);
        }
        OnConnectionLost(closest_node.endpoint);
        RecursiveSendOn(message);
      }
  };
  LOG(kVerbose) << " >>>>>>> rudp recursive send message to " << closest_node.endpoint << " <<<<<";
  RudpSend(message, closest_node.endpoint, message_sent_functor);
}

void NetworkUtils::AdjustRouteHistory(protobuf::Message& message) {
  assert(message.route_history().size() <= Parameters::max_routing_table_size);
  if (std::find(message.route_history().begin(), message.route_history().end(),
                routing_table_.kKeys().identity) == message.route_history().end()) {
    message.add_route_history(routing_table_.kKeys().identity);
    if (message.route_history().size() > Parameters::max_route_history) {
      std::vector<std::string> route_history(message.route_history().begin() + 1,
                                             message.route_history().end());
      message.clear_route_history();
      for (auto route : route_history)
        message.add_route_history(route);
    }
  }
  assert(message.route_history().size() <= Parameters::max_routing_table_size);
}

boost::asio::ip::udp::endpoint NetworkUtils::bootstrap_endpoint() const {
  return bootstrap_endpoint_;
}

boost::asio::ip::udp::endpoint NetworkUtils::this_node_relay_endpoint() const {
  return this_node_relay_endpoint_;
}

rudp::NatType NetworkUtils::nat_type() {
  return nat_type_;
}

Timer& NetworkUtils::timer() {
  return timer_;
}

}  // namespace routing

}  // namespace maidsafe
