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

#include "maidsafe/routing/utils.h"

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/rpcs.h"

namespace maidsafe {

namespace routing {

void ValidateAndAddToRudp(NetworkUtils& network_,
                          const NodeId& this_node_id,
                          const NodeId& peer_id,
                          const asymm::PublicKey& public_key,
                          const rudp::EndpointPair& peer_endpoint,
                          const rudp::EndpointPair& this_endpoint,
                          const bool& client) {
  NodeInfo peer;
  peer.node_id = peer_id;
  peer.public_key = public_key;
  peer.endpoint = peer_endpoint.external;
  protobuf::Message connect_success_external_endpoint(
      rpcs::ConnectSuccess(peer_id, this_node_id, this_endpoint.external, client));
  LOG(kVerbose) << "Calling RUDP::Add on this node's endpoint " << this_endpoint.external
                << ", peer's endpoint " << peer_endpoint.external;
  int result = network_.Add(this_endpoint.external, peer_endpoint.external,
                            connect_success_external_endpoint.SerializeAsString());
  if (result != rudp::kSuccess)
    LOG(kWarning) << "rudp add failed on external endpoint" << result;
  else
    LOG(kVerbose) << "rudp.Add succeeded on external endpoint";

  protobuf::Message connect_success_local_endpoint(
      rpcs::ConnectSuccess(peer_id, this_node_id, this_endpoint.local, client));
  LOG(kVerbose) << "Calling RUDP::Add on this node's endpoint " << this_endpoint.local
                << ", peer's endpoint " << peer_endpoint.local;
  result = network_.Add(this_endpoint.local, peer_endpoint.local,
                        connect_success_local_endpoint.SerializeAsString());

  if (result != rudp::kSuccess)
    LOG(kWarning) << "rudp add failed on local endpoint" << result;
  else
    LOG(kVerbose) << "rudp.Add succeeded on local endpoint";
}

void ValidateAndAddToRoutingTable(NetworkUtils& network_,
                                  RoutingTable& routing_table,
                                  NonRoutingTable& non_routing_table,
                                  const NodeId& peer_id,
                                  const asymm::PublicKey& public_key,
                                  const boost::asio::ip::udp::endpoint& peer_endpoint,
                                  const bool& client) {
  LOG(kVerbose) << "ValidateAndAddToRoutingTable";
  NodeInfo peer;
  peer.node_id = peer_id;
  peer.public_key = public_key;
  peer.endpoint = peer_endpoint;
  bool routing_accepted_node(false);
  if (client) {
    NodeId furthest_close_node_id =
        routing_table.GetNthClosestNode(NodeId(routing_table.kKeys().identity),
                                        Parameters::closest_nodes_size).node_id;

    if (non_routing_table.AddNode(peer, furthest_close_node_id)) {
      routing_accepted_node = true;
      LOG(kVerbose) << "Added client node to non routing table.  Node ID: "
                    << HexSubstr(peer_id.String());
    } else {
      LOG(kVerbose) << "Failed to add client node to non routing table.  Node ID: "
                    << HexSubstr(peer_id.String());
    }
  } else {
    if (routing_table.AddNode(peer)) {
      routing_accepted_node = true;
      LOG(kVerbose) << "[" << HexSubstr(routing_table.kKeys().identity) << "] "
                    << "added node to routing table.  Node ID: " << HexSubstr(peer_id.String());

      // ProcessSend(rpcs::ProxyConnect(node_id, NodeId(routing_table.kKeys().identity),
       //                              their_endpoint),
       //           rudp,
       //           routing_table,
       //           Endpoint());
    } else {
      LOG(kVerbose) << "Failed to add node to routing table.  Node id : "
                    << HexSubstr(peer_id.String());
    }
  }
  if (!routing_accepted_node) {
    LOG(kVerbose) << "Not adding node to " << (client ? "non-" : "") << "routing table.  Node id "
                  << HexSubstr(peer_id.String()) << " just added rudp connection will be removed.";
    network_.Remove(peer_endpoint);
  }
}

bool IsRoutingMessage(const protobuf::Message& message) {
  return (message.type() < static_cast<int32_t>(MessageType::kMaxRouting)) &&
         (message.type() > static_cast<int32_t>(MessageType::kMinRouting));
}

bool IsNodeLevelMessage(const protobuf::Message& message) {
  return !IsRoutingMessage(message);
}

bool IsRequest(const protobuf::Message& message) {
  return (message.type() > 0);
}

bool IsResponse(const protobuf::Message& message) {
  return !IsRequest(message);
}

bool IsDirect(const protobuf::Message& message) {
  return (message.direct() == static_cast<int32_t>(ConnectType::kSingle));
}

bool ValidateMessage(const protobuf::Message &message) {
  if (!IsRoutingMessage(message))
    return true;

  if (!NodeId(message.destination_id()).IsValid()) {
    LOG(kWarning) << "Message should have valid destination id.";
    return false;
  }

  if (!(message.has_source_id() || (message.has_relay_id() && message.has_relay()))) {
    LOG(kWarning) << "Message should have either src id or relay information.";
    return false;
  }

  if (message.has_source_id() && !NodeId(message.source_id()).IsValid()) {
    LOG(kWarning) << "Invalid source id field.";
    return false;
  }

  if (message.has_relay_id() && !NodeId(message.relay_id()).IsValid()) {
    LOG(kWarning) << "Invalid relay id field.";
    return false;
  }

  if (message.has_relay() &&
      GetEndpointFromProtobuf(message.relay()).address().is_unspecified()) {
    LOG(kWarning) << "Invalid relay endpoint field.";
    return false;
  }

  if (static_cast<MessageType>(message.type()) == MessageType::kConnectRequest)
    if (message.direct() != static_cast<int32_t>(ConnectType::kSingle)) {
      LOG(kWarning) << "kConnectRequest type message must be kSingle connect type.";
      return false;
    }

  if (static_cast<MessageType>(message.type()) == MessageType::kConnectResponse)
    if (message.direct() != static_cast<int32_t>(ConnectType::kSingle)) {
      LOG(kWarning) << "kConnectResponse type message must be kSingle connect type.";
      return false;
    }

  if (static_cast<MessageType>(message.type()) == MessageType::kFindNodesResponse)
    if ((message.direct() != static_cast<int32_t>(ConnectType::kSingle))) {
      LOG(kWarning) << "kFindNodesResponse type message must be kSingle connect type.";
      return false;
    }
  return true;
}

void SetProtobufEndpoint(const boost::asio::ip::udp::endpoint& endpoint,
                         protobuf::Endpoint* pb_endpoint) {
  if (pb_endpoint) {
    pb_endpoint->set_ip(endpoint.address().to_string().c_str());
    pb_endpoint->set_port(endpoint.port());
  }
}

boost::asio::ip::udp::endpoint GetEndpointFromProtobuf(const protobuf::Endpoint& pb_endpoint) {
  return boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string(pb_endpoint.ip()),
                                        static_cast<uint16_t>(pb_endpoint.port()));
}

}  // namespace routing

}  // namespace maidsafe
