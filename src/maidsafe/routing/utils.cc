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

#include <string>
#include <algorithm>
#include <vector>

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
  if (!this_endpoint.external.address().is_unspecified() &&
      !peer_endpoint.external.address().is_unspecified()) {
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
  }

  if (!this_endpoint.local.address().is_unspecified() &&
      !peer_endpoint.local.address().is_unspecified()) {
    protobuf::Message connect_success_local_endpoint(
        rpcs::ConnectSuccess(peer_id, this_node_id, this_endpoint.local, client));
    LOG(kVerbose) << "Calling RUDP::Add on this node's endpoint " << this_endpoint.local
                  << ", peer's endpoint " << peer_endpoint.local
                  << ", This node id : " << HexSubstr(this_node_id.String())
                  << ", Peer node id : " << HexSubstr(peer_id.String());
    int result = network_.Add(this_endpoint.local, peer_endpoint.local,
                              connect_success_local_endpoint.SerializeAsString());

    if (result != rudp::kSuccess)
      LOG(kWarning) << "rudp add failed on local endpoint" << result;
    else
      LOG(kVerbose) << "rudp.Add succeeded on local endpoint";
  }
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

void HandleSymmetricNodeAdd(RoutingTable& routing_table, const NodeId& peer_id,
                            const asymm::PublicKey& public_key) {
  if (routing_table.IsConnected(peer_id)) {
    LOG(kVerbose) << "[" << HexSubstr(routing_table.kKeys().identity) << "] "
                  << "already added node to routing table.  Node ID: "
                  << HexSubstr(peer_id.String())
                  << "Node is behind symmetric router but connected on local endpoint";
    return;
  }
  NodeInfo peer;
  peer.node_id = peer_id;
  peer.public_key = public_key;
  peer.endpoint = rudp::kNonRoutable;
  peer.nat_type = rudp::NatType::kSymmetric;

  if (routing_table.AddNode(peer)) {
    LOG(kVerbose) << "[" << HexSubstr(routing_table.kKeys().identity) << "] "
                  << "added node to routing table.  Node ID: " << HexSubstr(peer_id.String())
                  << "Node is behind symmetric router !";
  } else {
    LOG(kVerbose) << "Failed to add node to routing table.  Node id : "
                  << HexSubstr(peer_id.String());
  }
}

bool IsRoutingMessage(const protobuf::Message& message) {
  return message.routing_message();
}

bool IsNodeLevelMessage(const protobuf::Message& message) {
  return !IsRoutingMessage(message);
}

bool IsRequest(const protobuf::Message& message) {
  return (message.request());
}

bool IsResponse(const protobuf::Message& message) {
  return !IsRequest(message);
}

bool IsDirect(const protobuf::Message& message) {
  return message.direct();
}

bool ValidateMessage(const protobuf::Message &message) {
  if (!message.IsInitialized()) {
    LOG(kWarning) << "Uninitialised message dropped.";
    return false;
  }

  // Message has traversed more hops than expected
  if (message.hops_to_live() <= 0) {
    std::string route_history;
    for (auto route : message.route_history())
      route_history += HexSubstr(route) + ", ";
    LOG(kError) << "Message has traversed more hops than expected. "
                <<  Parameters::max_route_history << " last hops in route history are: "
                 << route_history
                 << " \nMessage source: " << HexSubstr(message.source_id())
                 << ", \nMessage destination: " << HexSubstr(message.destination_id())
                 << ", \nMessage type: " << message.type()
                 << ", \nMessage id: " << message.id();
    return false;
  }

  // Invalid destination id, unknown message
  if (!(NodeId(message.destination_id()).IsValid())) {
    LOG(kWarning) << "Stray message dropped, need destination ID for processing."
                  << " id: " << message.id();
    return false;
  }

  if (!NodeId(message.destination_id()).IsValid()) {
    LOG(kWarning) << "Message should have valid destination id.";
    return false;
  }

  if (!(message.has_source_id() || (message.has_relay_id() && message.has_relay()))) {
    LOG(kWarning) << "Message should have either src id or relay information.";
    assert(false && "Message should have either src id or relay information.");
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

  if (static_cast<MessageType>(message.type()) == MessageType::kConnect)
    if (!message.direct()) {
      LOG(kWarning) << "kConnectRequest type message must be direct.";
      return false;
    }

  if (static_cast<MessageType>(message.type()) == MessageType::kFindNodes &&
      (message.request() == false))
    if ((!message.direct())) {
      LOG(kWarning) << "kFindNodesResponse type message must be direct.";
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

std::string MessageTypeString(const protobuf::Message& message) {
  std::string message_type;
  switch (static_cast<MessageType>(message.type())) {
    case MessageType::kPing :
      message_type = "kPing";
      break;
    case MessageType::kConnect :
      message_type = "kConnect";
      break;
    case MessageType::kFindNodes :
      message_type = "kFindNodes";
      break;
    case MessageType::kProxyConnect :
      message_type = "kProxyConnect";
      break;
    case MessageType::kConnectSuccess :
      message_type = "kConnectSuccess";
      break;
    case MessageType::kNodeLevel :
      message_type = "kNodeLevel";
      break;
    default:
      message_type = "Unknown";
  }
  if (message.request())
    message_type = message_type + " Request";
  else
    message_type = message_type + " Response";
  return message_type;
}

std::vector<boost::asio::ip::udp::endpoint> OrderBootstrapList(
                    std::vector<boost::asio::ip::udp::endpoint> peer_endpoints) {
  if (peer_endpoints.empty())
    return peer_endpoints;
  auto copy_vector(peer_endpoints);
  for (auto &endpoint : copy_vector) {
    endpoint.port(5483);
  }
  auto it = std::unique(copy_vector.begin(), copy_vector.end());
  copy_vector.resize(it - copy_vector.begin());
  std::reverse(peer_endpoints.begin(), peer_endpoints.end());
  peer_endpoints.resize(peer_endpoints.size() + copy_vector.size());
  for (auto& i : copy_vector)
    peer_endpoints.push_back(i);
  std::reverse(peer_endpoints.begin(), peer_endpoints.end());
  return peer_endpoints;
}

protobuf::NatType NatTypeProtobuf(const rudp::NatType& nat_type) {
  switch (nat_type) {
    case rudp::NatType::kSymmetric :
      return protobuf::NatType::kSymmetric;
      break;
    case rudp::NatType::kOther :
      return protobuf::NatType::kOther;
      break;
    default :
      return protobuf::NatType::kUnknown;
      break;
  }
}

}  // namespace routing

}  // namespace maidsafe
