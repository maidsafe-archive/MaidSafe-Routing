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

#include "maidsafe/common/utils.h"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/rpcs.h"

namespace maidsafe {

namespace routing {

bool ClosestToMe(protobuf::Message &message, RoutingTable &routing_table) {
  return routing_table.AmIClosestNode(NodeId(message.destination_id()));
}

bool InClosestNodesToMe(protobuf::Message &message, RoutingTable &routing_table) {
  return routing_table.IsMyNodeInRange(NodeId(message.destination_id()),
                                       Parameters::closest_nodes_size);
}

void ValidateThisNode(rudp::ManagedConnections &rudp,
                      RoutingTable &routing_table,
                      const NodeId& node_id,
                      const asymm::PublicKey &public_key,
                      const rudp::EndpointPair &their_endpoint,
                      const rudp::EndpointPair &our_endpoint,
                      const bool &client) {
  NodeInfo node_info;
  node_info.node_id = NodeId(node_id);
  node_info.public_key = public_key;
  node_info.endpoint = their_endpoint.external;
  LOG(kVerbose) << "Calling rudp Add on endpoint = " << our_endpoint.external
                << ", their endpoint = " << their_endpoint.external;
  int result = rudp.Add(our_endpoint.external, their_endpoint.external, node_id.String());

  if (result != 0) {
      LOG(kWarning) << "rudp add failed " << result;
    return;
  }
  LOG(kVerbose) << "rudp.Add result = " << result;
  if (client) {
    // direct_non_routing_table_connections_.push_back(node_info);
    LOG(kError) << " got client and do not know how to add them yet ";
  } else {
    if (routing_table.AddNode(node_info)) {
      LOG(kVerbose) << "Added node to routing table. node id " << HexSubstr(node_id.String());
//      ProcessSend(rpcs::ProxyConnect(node_id, NodeId(routing_table.kKeys().identity),
//                                     their_endpoint),
//                  rudp,
//                  routing_table,
//                  Endpoint());
    } else {
      LOG(kVerbose) << "Not adding node to routing table  node id "
                    << HexSubstr(node_id.String())
                    << " just added rudp connection will be removed now";
      rudp.Remove(their_endpoint.external);
    }
  }
}

bool IsRoutingMessage(const protobuf::Message &message) {
  return ((message.type() < 100) && (message.type() > -100));
}

bool IsNodeLevelMessage(const protobuf::Message &message) {
  return !IsRoutingMessage(message);
}

bool IsRequest(const protobuf::Message &message) {
  return (message.type() > 0);
}

bool IsResponse(const protobuf::Message &message) {
  return !IsRequest(message);
}

void SetProtobufEndpoint(const Endpoint& endpoint, protobuf::Endpoint *pbendpoint) {
  if (pbendpoint) {
    pbendpoint->set_ip(endpoint.address().to_string().c_str());
    pbendpoint->set_port(endpoint.port());
  }
}

Endpoint GetEndpointFromProtobuf(const protobuf::Endpoint &pbendpoint) {
  Endpoint endpoint;
  endpoint.address(boost::asio::ip::address::from_string(pbendpoint.ip()));
  endpoint.port(static_cast<uint16_t>(pbendpoint.port()));
  return endpoint;
}

}  // namespace routing

}  // namespace maidsafe
