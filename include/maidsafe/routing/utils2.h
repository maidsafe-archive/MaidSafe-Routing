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

#ifndef MAIDSAFE_ROUTING_UTILS2_H_
#define MAIDSAFE_ROUTING_UTILS2_H_

#include <string>
#include <vector>

#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/client_routing_table.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/return_codes.h"

namespace maidsafe {

namespace routing {

namespace fs = boost::filesystem;

template <typename NodeType>
class NetworkUtils;

template <typename NodeType>
struct ConnectionsInfo {
  ConnectionsInfo(const NodeId& node_id, const asymm::Keys& keys) : routing_table(node_id, keys) {}
  RoutingTable<NodeType> routing_table;
};

template <>
struct ConnectionsInfo<VaultNode> {
  ConnectionsInfo(const NodeId& node_id, const asymm::Keys& keys)
      : routing_table(node_id, keys), client_routing_table(node_id) {}
  RoutingTable<VaultNode> routing_table;
  ClientRoutingTable client_routing_table;
};

template <typename NodeType>
struct Connections : ConnectionsInfo<NodeType> {
  Connections(const NodeId& node_id, const asymm::Keys& keys)
      : ConnectionsInfo<NodeType>(node_id, keys) {}
  NodeId kNodeId() { return ConnectionsInfo<NodeType>::routing_table.kNodeId(); }
  NodeId kConnectionId() { return ConnectionsInfo<NodeType>::routing_table.kConnectionId(); }
};

template <typename NodeType>
int AddToRudp(NetworkUtils<NodeType>& network, const NodeId& this_node_id,
              const NodeId& this_connection_id, const NodeId& peer_id,
              const NodeId& peer_connection_id, rudp::EndpointPair peer_endpoint_pair,
              bool requestor) {
  LOG(kVerbose) << "AddToRudp. peer_id : " << DebugId(peer_id)
                << " , connection id : " << DebugId(peer_connection_id);
  protobuf::Message connect_success(
      rpcs::ConnectSuccess(peer_id, this_node_id, this_connection_id, requestor, NodeType::value));
  int result(
      network.Add(peer_connection_id, peer_endpoint_pair, connect_success.SerializeAsString()));
  if (result != rudp::kSuccess) {
    LOG(kError) << "rudp add failed for peer node [" << peer_id
                << "]. Connection id : " << peer_connection_id << ". result : " << result;
  } else {
    LOG(kVerbose) << "rudp.Add succeeded for peer node [" << peer_id
                  << "]. Connection id : " << peer_connection_id;
  }
  return result;
}

template <typename NodeType>
bool ValidateAndAddToRoutingTable(NetworkUtils<NodeType>& network,
                                  Connections<NodeType>& connections, const NodeId& peer_id,
                                  const NodeId& connection_id, const asymm::PublicKey& public_key,
                                  VaultNode) {
  if (network.MarkConnectionAsValid(connection_id) != kSuccess) {
    LOG(kError) << "[" << connections.kNodeId() << "] "
                << ". Rudp failed to validate connection with  Peer id : " << peer_id
                << " , Connection id : " << connection_id;
    return false;
  }

  NodeInfo peer;
  peer.id = peer_id;
  peer.public_key = public_key;
  peer.connection_id = connection_id;
  bool routing_accepted_node(false);
  if (connections.routing_table.AddNode(peer))
    routing_accepted_node = true;

  if (routing_accepted_node) {
    LOG(kVerbose) << "[" << connections.kNodeId() << "] "
                  << "added "
                  << "node to "
                  << "routing table.  Node ID: " << HexSubstr(peer_id.string());
    return true;
  }

  LOG(kInfo) << "[" << connections.kNodeId() << "] "
             << "failed to add "
             << "node to "
             << "routing table. Node ID: " << HexSubstr(peer_id.string())
             << ". Added rudp connection will be removed.";
  network.Remove(connection_id);
  return false;
}

template <typename NodeType>
bool ValidateAndAddToRoutingTable(NetworkUtils<NodeType>& network,
                                  Connections<NodeType>& connections, const NodeId& peer_id,
                                  const NodeId& connection_id, const asymm::PublicKey& public_key,
                                  ClientNode) {
  if (network.MarkConnectionAsValid(connection_id) != kSuccess) {
    LOG(kError) << "[" << connections.kNodeId() << "] "
                << ". Rudp failed to validate connection with  Peer id : " << peer_id
                << " , Connection id : " << connection_id;
    return false;
  }

  NodeInfo peer;
  peer.id = peer_id;
  peer.public_key = public_key;
  peer.connection_id = connection_id;
  bool routing_accepted_node(false);
  NodeId furthest_close_node_id =
      connections.routing_table.GetNthClosestNode(connections.kNodeId(),
                                                  2 * Parameters::closest_nodes_size).id;

  if (connections.client_routing_table.AddNode(peer, furthest_close_node_id))
    routing_accepted_node = true;

  if (routing_accepted_node) {
    LOG(kVerbose) << "[" << connections.kNodeId() << "] added client-node to non-routing table."
                  << "Node ID: " << HexSubstr(peer_id.string());
    return true;
  }

  LOG(kInfo) << "[" << connections.kNodeId() << "] failed to add client-node to non-routing table."
             << "Node ID: " << HexSubstr(peer_id.string())
             << ". Added rudp connection will be removed.";
  network.Remove(connection_id);
  return false;
}

template <>
bool ValidateAndAddToRoutingTable(NetworkUtils<ClientNode>& network,
                                  Connections<ClientNode>& connections, const NodeId& peer_id,
                                  const NodeId& connection_id, const asymm::PublicKey& public_key,
                                  ClientNode);

template <typename NodeType>
void InformClientOfNewCloseNode(NetworkUtils<NodeType>& network, const NodeInfo& client,
                                const NodeInfo& new_close_node, const NodeId& this_node_id) {
  protobuf::Message inform_client_of_new_close_node(
      rpcs::InformClientOfNewCloseNode(new_close_node.id, this_node_id, client.id));
  network.SendToDirect(inform_client_of_new_close_node, client.id, client.connection_id);
}

// FIXME
template <typename NodeType>
void HandleSymmetricNodeAdd(RoutingTable<NodeType>& /*routing_table*/, const NodeId& /*peer_id*/,
                            const asymm::PublicKey& /*public_key*/) {
  //  if (routing_table.Contains(peer_id)) {
  //    LOG(kVerbose) << "[" << HexSubstr(routing_table.kKeys().identity) << "] "
  //                  << "already added node to routing table.  Node ID: "
  //                  << HexSubstr(peer_id.string())
  //                  << "Node is behind symmetric router but connected on local endpoint";
  //    return;
  //  }
  //  NodeInfo peer;
  //  peer.id = peer_id;
  //  peer.public_key = public_key;
  ////  peer.endpoint = rudp::kNonRoutable;
  //  peer.nat_type = rudp::NatType::kSymmetric;

  //  if (routing_table.AddNode(peer)) {
  //    LOG(kVerbose) << "[" << HexSubstr(routing_table.kKeys().identity) << "] "
  //                  << "added node to routing table.  Node ID: " << HexSubstr(peer_id.string())
  //                  << "Node is behind symmetric router !";
  //  } else {
  //    LOG(kVerbose) << "Failed to add node to routing table.  Node id : "
  //                  << HexSubstr(peer_id.string());
  //  }
}

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_UTILS2_H_
