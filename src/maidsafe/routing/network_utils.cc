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

#include "maidsafe/common/utils.h"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/utils.h"


namespace maidsafe {

namespace routing {

namespace {

void SendOn(protobuf::Message message,
            rudp::ManagedConnections &rudp,
            RoutingTable &routing_table,
            const NodeId &node_id,
            const Endpoint &endpoint) {
  Endpoint peer_endpoint = endpoint;
  NodeId peer_node_id = node_id;
  const std::string my_node_id(HexSubstr(routing_table.kKeys().identity));
  rudp::MessageSentFunctor message_sent_functor = [=](bool message_sent) {
      if (message_sent)
        LOG(kInfo) << " Message sent, type: " << message.type()
                   << " to "
                   << HexSubstr(peer_node_id.String())
                   << " I am " << my_node_id
                   << " [destination id : "
                   << HexSubstr(message.destination_id())
                   << "]";
      else
        LOG(kError) << " Failed to send message, type: " << message.type()
                    << " to "
                    << HexSubstr(peer_node_id.String())
                    << " I am " << my_node_id
                    << " [destination id : "
                    << HexSubstr(message.destination_id())
                    << "]";
    };

  LOG(kVerbose) << " >>>>>>>>> rudp send message to " << peer_endpoint << " <<<<<<<<<<<<<<<<<<<<";
  rudp.Send(peer_endpoint, message.SerializeAsString(), message_sent_functor);
}

void RecursiveSendOn(protobuf::Message message,
                     rudp::ManagedConnections &rudp,
                     RoutingTable &routing_table,
                     NodeInfo last_node_attempted = NodeInfo(),
                     int attempt_count = 0) {
  NodeInfo closest_node;

  if (attempt_count >= 3) {
    LOG(kWarning) << " Retry attempts failed to send to ["
                  << HexSubstr(last_node_attempted.node_id.String())
                  << "] will drop this node now and try with another node";
    attempt_count = 0;
    // TODO (Prakash) : To move this to a free function.
    routing_table.DropNode(last_node_attempted.endpoint);
    rudp.Remove(last_node_attempted.endpoint);
  }

  closest_node = routing_table.GetClosestNode(NodeId(message.destination_id()));
  if (closest_node.node_id == NodeId()) {
    LOG(kError) << " My RT is empty now. Need to rebootstrap.";
    return;
  }

  const std::string my_node_id(HexSubstr(routing_table.kKeys().identity));

  rudp::MessageSentFunctor message_sent_functor = [=, &routing_table, &rudp](bool message_sent) {
      if (message_sent) {
        LOG(kInfo) << " Message sent, type: " << message.type()
                   << " to "
                   << HexSubstr(closest_node.node_id.String())
                   << " I am " << my_node_id
                   << " [ destination id : "
                   << HexSubstr(message.destination_id())
                   << "]";
      } else {
        LOG(kError) << " Failed to send message, type: " << message.type()
                    << " to "
                    << HexSubstr(closest_node.node_id.String())
                    << " I am " << my_node_id
                    << " [ destination id : "
                    << HexSubstr(message.destination_id())
                    << "]"
                    << " Will retry to Send. Attempt count = "
                    << attempt_count + 1;
        RecursiveSendOn(message, rudp, routing_table, closest_node, attempt_count + 1);
      }
    };
  LOG(kVerbose) << " >>>>>>> rudp recursive send message to " << closest_node.endpoint << " <<<<<";
  rudp.Send(closest_node.endpoint, message.SerializeAsString(), message_sent_functor);
  // TODO(Prakash) :  if send functor returns kNotConnected then DropNode(node);
}

}  // anonymous namespace


void ProcessSend(protobuf::Message message,
                 rudp::ManagedConnections &rudp,
                 RoutingTable &routing_table,
                 NonRoutingTable &non_routing_table,
                 Endpoint direct_endpoint) {
  std::string signature;
  asymm::Sign(message.data(), routing_table.kKeys().private_key, &signature);
  message.set_signature(signature);

  // Direct endpoint message
  if (!direct_endpoint.address().is_unspecified()) {  // direct endpoint provided
    SendOn(message, rudp, routing_table, NodeId(), direct_endpoint);
    return;
  }

  // Normal messages
  if (message.has_destination_id()) {  // message has destination id
    std::vector<NodeInfo>
      non_routing_nodes(non_routing_table.GetNodesInfo(NodeId(message.destination_id())));
    if (!non_routing_nodes.empty()) {  // I have the destination id in my NRT
      LOG(kInfo) <<"I have destination node in my NRT";
      for (auto i : non_routing_nodes) {
        LOG(kVerbose) <<"Sending message to my NRT node with id endpoint : " << i.endpoint;
        SendOn(message, rudp, routing_table, i.node_id, i.endpoint);
      }
    } else if (routing_table.Size() > 0) {  //  getting closer nodes from my RT
      RecursiveSendOn(message, rudp, routing_table);
    } else {
      LOG(kError) << " No Endpoint to send to, Aborting Send!"
                  << " Attempt to send a type : " << message.type() << " message"
                  << " to " << HexSubstr(message.source_id())
                  << " From " << HexSubstr(routing_table.kKeys().identity);
    }
    return;
  }

  // Relay message responses only
  if (message.has_relay_id() && (IsResponse(message)) && message.has_relay()) {
    direct_endpoint = GetEndpointFromProtobuf(message.relay());
    message.set_destination_id(message.relay_id());  // so that peer identifies it as direct msg
    SendOn(message, rudp, routing_table, NodeId(message.relay_id()), direct_endpoint);
  } else {
    LOG(kError) << " Unable to work out destination, Aborting Send!";
  }
}

NetworkUtils::NetworkUtils(RoutingTable &routing_table, NonRoutingTable &non_routing_table)
    : routing_table_(routing_table),
      non_routing_table_(non_routing_table),
      rudp_(),
      tearing_down_(false) {}

Endpoint NetworkUtils::Bootstrap(const std::vector<Endpoint> &bootstrap_endpoints,
                                 rudp::MessageReceivedFunctor message_received_functor,
                                 rudp::ConnectionLostFunctor connection_lost_functor,
                                 Endpoint local_endpoint) {
  return rudp_.Bootstrap(bootstrap_endpoints, message_received_functor, connection_lost_functor,
                         local_endpoint);
  }

int NetworkUtils::GetAvailableEndpoint(const Endpoint &peer_endpoint,
                                       rudp::EndpointPair &this_endpoint_pair) {
  return rudp_.GetAvailableEndpoint(peer_endpoint, this_endpoint_pair);
}

int NetworkUtils::Add(const Endpoint &this_endpoint,
                      const Endpoint &peer_endpoint,
                      const std::string &validation_data) {
  return rudp_.Add(this_endpoint, peer_endpoint, validation_data);
}

void NetworkUtils::Remove(const Endpoint &peer_endpoint) {
  rudp_.Remove(peer_endpoint);
}

void NetworkUtils::RudpSend(protobuf::Message message,
                            Endpoint endpoint,
                            rudp::MessageSentFunctor message_sent_functor) {
  rudp_.Send(endpoint, message.SerializeAsString(), message_sent_functor);
}

void NetworkUtils::SendToDirectEndpoint(const protobuf::Message &message,
                                        Endpoint direct_endpoint,
                                        rudp::MessageSentFunctor message_sent_functor) {
  RudpSend(message, direct_endpoint, message_sent_functor);
}

void NetworkUtils::SendToDirectEndpoint(const protobuf::Message &message,
                                        Endpoint direct_endpoint) {
  NodeId node_id;
  if (message.has_destination_id())
    node_id = NodeId(message.destination_id());
  SendTo(message, node_id, direct_endpoint);
}

void NetworkUtils::SendToClosestNode(protobuf::Message message) {
  // Normal messages
  if (message.has_destination_id()) {  // message has destination id
    std::vector<NodeInfo>
      non_routing_nodes(non_routing_table_.GetNodesInfo(NodeId(message.destination_id())));
    if (!non_routing_nodes.empty()) {  // I have the destination id in my NRT
      LOG(kInfo) <<"I have destination node in my NRT";
      for (auto i : non_routing_nodes) {
        LOG(kVerbose) <<"Sending message to my NRT node with id endpoint : " << i.endpoint;
        SendTo(message, i.node_id, i.endpoint);
      }
    } else if (routing_table_.Size() > 0) {  //  getting closer nodes from my RT
      RecursiveSendOn(message);
    } else {
      LOG(kError) << " No Endpoint to send to, Aborting Send!"
                  << " Attempt to send a type : " << message.type() << " message"
                  << " to " << HexSubstr(message.source_id())
                  << " From " << HexSubstr(routing_table_.kKeys().identity);
    }
    return;
  }

  // Relay message responses only
  if (message.has_relay_id() && (IsResponse(message)) && message.has_relay()) {
    Endpoint direct_endpoint = GetEndpointFromProtobuf(message.relay());
    message.set_destination_id(message.relay_id());  // so that peer identifies it as direct msg
    SendTo(message, NodeId(message.relay_id()), direct_endpoint);
  } else {
    LOG(kError) << " Unable to work out destination, Aborting Send!";
  }
}

void NetworkUtils::SendTo(protobuf::Message message,
                          const NodeId &node_id,
                          const Endpoint &endpoint) {
  NodeId peer_node_id = node_id;
  const std::string my_node_id(HexSubstr(routing_table_.kKeys().identity));
  rudp::MessageSentFunctor message_sent_functor = [=](bool message_sent) {
      if (message_sent)
        LOG(kInfo) << " Message sent, type: " << message.type()
                   << " to "
                   << HexSubstr(peer_node_id.String())
                   << " I am " << my_node_id
                   << " [destination id : "
                   << HexSubstr(message.destination_id())
                   << "]";
      else
        LOG(kError) << " Failed to send message, type: " << message.type()
                    << " to "
                    << HexSubstr(peer_node_id.String())
                    << " I am " << my_node_id
                    << " [destination id : "
                    << HexSubstr(message.destination_id())
                    << "]";
    };
  LOG(kVerbose) << " >>>>>>>>> rudp send message to " << endpoint << " <<<<<<<<<<<<<<<<<<<<";
  RudpSend(message, endpoint, message_sent_functor);
}

void NetworkUtils::RecursiveSendOn(protobuf::Message message,
                                   NodeInfo last_node_attempted,
                                   int attempt_count) {
  NodeInfo closest_node;

  if (attempt_count >= 3) {
    LOG(kWarning) << " Retry attempts failed to send to ["
                  << HexSubstr(last_node_attempted.node_id.String())
                  << "] will drop this node now and try with another node";
    attempt_count = 0;
    // TODO (Prakash) : To move this to a free function.
    routing_table_.DropNode(last_node_attempted.endpoint);
    rudp_.Remove(last_node_attempted.endpoint);
  }

  closest_node = routing_table_.GetClosestNode(NodeId(message.destination_id()));
  if (closest_node.node_id == NodeId()) {
    LOG(kError) << " My RT is empty now. Need to rebootstrap.";
    return;
  }

  const std::string my_node_id(HexSubstr(routing_table_.kKeys().identity));

  rudp::MessageSentFunctor message_sent_functor = [=](bool message_sent) {
      if (message_sent) {
        LOG(kInfo) << " Message sent, type: " << message.type()
                   << " to "
                   << HexSubstr(closest_node.node_id.String())
                   << " I am " << my_node_id
                   << " [ destination id : "
                   << HexSubstr(message.destination_id())
                   << "]";
      } else {
        LOG(kError) << " Failed to send message, type: " << message.type()
                    << " to "
                    << HexSubstr(closest_node.node_id.String())
                    << " I am " << my_node_id
                    << " [ destination id : "
                    << HexSubstr(message.destination_id())
                    << "]"
                    << " Will retry to Send. Attempt count = "
                    << attempt_count + 1;
        RecursiveSendOn(message, closest_node, attempt_count + 1);
      }
    };
  LOG(kVerbose) << " >>>>>>> rudp recursive send message to " << closest_node.endpoint << " <<<<<";
  RudpSend(message, closest_node.endpoint, message_sent_functor);
  // TODO(Prakash) :  if send functor returns kNotConnected then DropNode(node);
}

}  // namespace routing

}  // namespace maidsafe
