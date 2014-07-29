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

#ifndef MAIDSAFE_ROUTING_SERVICE_H_
#define MAIDSAFE_ROUTING_SERVICE_H_

#include <memory>

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/utils2.h"

namespace maidsafe {

namespace routing {

class ClientRoutingTable;

template <typename NodeType>
class Service {
 public:
  Service(Connections<NodeType>& connections, NetworkUtils<NodeType>& network);
  virtual ~Service();
  // Handle all incoming requests and send back reply
  virtual void Ping(protobuf::Message& message);
  virtual void Connect(protobuf::Message& message);
  virtual void FindNodes(protobuf::Message& message);
  virtual void ConnectSuccess(protobuf::Message& message);
  virtual void GetGroup(protobuf::Message& message);
  void set_request_public_key_functor(RequestPublicKeyFunctor request_public_key);
  RequestPublicKeyFunctor request_public_key_functor() const;

 private:
  void ConnectSuccessFromRequester(NodeInfo& peer);
  void ConnectSuccessFromResponder(NodeInfo& peer, bool client);
  bool CheckPriority(const NodeId& this_node, const NodeId& peer_node);

  Connections<NodeType>& connections_;
  NetworkUtils<NodeType>& network_;
  RequestPublicKeyFunctor request_public_key_functor_;
};

template <typename NodeType>
Service<NodeType>::Service(Connections<NodeType>& connections, NetworkUtils<NodeType>& network)
    : connections_(connections), network_(network), request_public_key_functor_() {}

template <typename NodeType>
Service<NodeType>::~Service() {}

template <typename NodeType>
void Service<NodeType>::Ping(protobuf::Message& message) {
  if (message.destination_id() != connections_.kNodeId().string()) {
    // Message not for this node and we should not pass it on.
    LOG(kError) << "Message not for this node.";
    message.Clear();
    return;
  }
  protobuf::PingResponse ping_response;
  protobuf::PingRequest ping_request;

  if (!ping_request.ParseFromString(message.data(0))) {
    LOG(kError) << "No Data.";
    return;
  }
  ping_response.set_pong(true);
  ping_response.set_original_request(message.data(0));
  ping_response.set_original_signature(message.signature());
#ifdef TESTING
  ping_response.set_timestamp(GetTimeStamp());
#endif
  message.set_request(false);
  message.clear_route_history();
  message.clear_data();
  message.add_data(ping_response.SerializeAsString());
  message.set_destination_id(message.source_id());
  message.set_source_id(connections_.kNodeId().string());
  message.set_hops_to_live(Parameters::hops_to_live);
  assert(message.IsInitialized() && "unintialised message");
}

template <typename NodeType>
void Service<NodeType>::Connect(protobuf::Message& message) {
  if (message.destination_id() != connections_.kNodeId().string()) {
    // Message not for this node and we should not pass it on.
    LOG(kError) << "Message not for this node.";
    message.Clear();
    return;
  }
  protobuf::ConnectRequest connect_request;
  protobuf::ConnectResponse connect_response;
  if (!connect_request.ParseFromString(message.data(0))) {
    LOG(kVerbose) << "Unable to parse connect request.";
    message.Clear();
    return;
  }

  if (connect_request.peer_id() != connections_.kNodeId().string()) {
    LOG(kError) << "Message not for this node.";
    message.Clear();
    return;
  }

  NodeInfo peer_node;
  peer_node.id = NodeId(connect_request.contact().node_id());
  peer_node.connection_id = NodeId(connect_request.contact().connection_id());
  LOG(kVerbose) << "[" << connections_.kNodeId() << "]"
                << " received Connect request from " << peer_node.id;
  rudp::EndpointPair this_endpoint_pair, peer_endpoint_pair;
  peer_endpoint_pair.external =
      GetEndpointFromProtobuf(connect_request.contact().public_endpoint());
  peer_endpoint_pair.local = GetEndpointFromProtobuf(connect_request.contact().private_endpoint());

  if (peer_endpoint_pair.external.address().is_unspecified() &&
      peer_endpoint_pair.local.address().is_unspecified()) {
    LOG(kWarning) << "Invalid endpoint pair provided in connect request.";
    message.Clear();
    return;
  }

  // Prepare response
  connect_response.set_answer(protobuf::ConnectResponseType::kRejected);
#ifdef TESTING
  connect_response.set_timestamp(GetTimeStamp());
#endif
  connect_response.set_original_request(message.data(0));
  connect_response.set_original_signature(message.signature());

  message.clear_route_history();
  message.clear_data();
  message.set_direct(true);
  message.set_replication(1);
  message.set_client_node(NodeType::value);
  message.set_request(false);
  message.set_hops_to_live(Parameters::hops_to_live);
  if (message.has_source_id())
    message.set_destination_id(message.source_id());
  else
    message.clear_destination_id();
  message.set_source_id(connections_.kNodeId().string());

  // Check rudp & routing
  bool check_node_succeeded(false);
  if (message.client_node()) {  // Client node, check non-routing table
    LOG(kVerbose) << "Client connect request - will check non-routing table.";
    NodeId furthest_close_node_id =
        connections_.routing_table.GetNthClosestNode(connections_.kNodeId(),
                                                     2 * Parameters::closest_nodes_size).id;
    check_node_succeeded =
        connections_.client_routing_table.CheckNode(peer_node, furthest_close_node_id);
  } else {
    LOG(kVerbose) << "Server connect request - will check routing table.";
    check_node_succeeded = connections_.routing_table.CheckNode(peer_node);
  }

  if (check_node_succeeded) {
    LOG(kVerbose) << "CheckNode(node) for " << (message.client_node() ? "client" : "server")
                  << " node succeeded.";
    rudp::NatType this_nat_type(rudp::NatType::kUnknown);
    int ret_val = network_.GetAvailableEndpoint(peer_node.connection_id, peer_endpoint_pair,
                                                this_endpoint_pair, this_nat_type);
    if (ret_val != rudp::kSuccess && ret_val != rudp::kBootstrapConnectionAlreadyExists) {
      if (rudp::kUnvalidatedConnectionAlreadyExists != ret_val &&
          rudp::kConnectAttemptAlreadyRunning != ret_val) {
        LOG(kError) << "[" << connections_.kNodeId() << "] Service: "
                    << "Failed to get available endpoint for new connection to node id : "
                    << peer_node.id << ", Connection id :" << DebugId(peer_node.connection_id)
                    << ". peer_endpoint_pair.external = " << peer_endpoint_pair.external
                    << ", peer_endpoint_pair.local = " << peer_endpoint_pair.local
                    << ". Rudp returned :" << ret_val;
        message.add_data(connect_response.SerializeAsString());
        return;
      } else {  // Resolving collision by giving priority to lesser node id.
        if (!CheckPriority(peer_node.id, connections_.kNodeId())) {
          LOG(kInfo) << "Already ongoing attempt with : " << DebugId(peer_node.connection_id);
          connect_response.set_answer(protobuf::ConnectResponseType::kConnectAttemptAlreadyRunning);
          message.add_data(connect_response.SerializeAsString());
          return;
        }
      }
    }

    assert((!this_endpoint_pair.external.address().is_unspecified() ||
            !this_endpoint_pair.local.address().is_unspecified()) &&
           "Unspecified endpoint after GetAvailableEndpoint success.");

    int add_result(AddToRudp(network_, connections_.kNodeId(),
                             connections_.kConnectionId(), peer_node.id,
                             peer_node.connection_id, peer_endpoint_pair, false));
    if (rudp::kSuccess == add_result) {
      connect_response.set_answer(protobuf::ConnectResponseType::kAccepted);

      connect_response.mutable_contact()->set_node_id(connections_.kNodeId().string());
      connect_response.mutable_contact()->set_connection_id(
          connections_.kConnectionId().string());
      connect_response.mutable_contact()->set_nat_type(NatTypeProtobuf(this_nat_type));

      SetProtobufEndpoint(this_endpoint_pair.local,
                          connect_response.mutable_contact()->mutable_private_endpoint());
      SetProtobufEndpoint(this_endpoint_pair.external,
                          connect_response.mutable_contact()->mutable_public_endpoint());
    }
  } else {
    LOG(kVerbose) << "CheckNode(node) for " << (message.client_node() ? "client" : "server")
                  << " node failed.";
  }

  message.add_data(connect_response.SerializeAsString());
  assert(message.IsInitialized() && "unintialised message");
}

template <>
void Service<ClientNode>::Connect(protobuf::Message& message);

template <typename NodeType>
bool Service<NodeType>::CheckPriority(const NodeId& this_node, const NodeId& peer_node) {
  assert(this_node != peer_node);
  return (this_node > peer_node);
}

template <typename NodeType>
void Service<NodeType>::FindNodes(protobuf::Message& message) {
  protobuf::FindNodesRequest find_nodes;
  if (!find_nodes.ParseFromString(message.data(0))) {
    LOG(kWarning) << "Unable to parse find node request.";
    message.Clear();
    return;
  }
  if (0 == find_nodes.num_nodes_requested() || NodeId(find_nodes.target_node()).IsZero()) {
    LOG(kWarning) << "Invalid find node request.";
    message.Clear();
    return;
  }

  LOG(kVerbose) << "[" << connections_.kNodeId() << "] parsed find node request for target id : "
                << HexSubstr(find_nodes.target_node());
  protobuf::FindNodesResponse found_nodes;
  auto nodes(connections_.routing_table.GetClosestNodes(
      NodeId(find_nodes.target_node()),
      static_cast<uint16_t>(find_nodes.num_nodes_requested() - 1)));
  found_nodes.add_nodes(connections_.kNodeId().string());

  for (const auto& node : nodes)
    found_nodes.add_nodes(node.id.string());

  LOG(kVerbose) << "Responding Find node with " << found_nodes.nodes_size() << " contacts.";

  found_nodes.set_original_request(message.data(0));
  found_nodes.set_original_signature(message.signature());
#ifdef TESTING
  found_nodes.set_timestamp(GetTimeStamp());
#endif
  assert(found_nodes.IsInitialized() && "unintialised found_nodes response");
  if (message.has_source_id()) {
    message.set_destination_id(message.source_id());
  } else {
    message.clear_destination_id();
    LOG(kVerbose) << "Relay message, so not setting destination ID.";
  }
  message.set_source_id(connections_.kNodeId().string());
  message.clear_route_history();
  message.clear_data();
  message.add_data(found_nodes.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_client_node(NodeType::value);
  message.set_request(false);
  message.set_hops_to_live(Parameters::hops_to_live);
  assert(message.IsInitialized() && "unintialised message");
}

template <typename NodeType>
void Service<NodeType>::ConnectSuccess(protobuf::Message& message) {
  protobuf::ConnectSuccess connect_success;

  if (!connect_success.ParseFromString(message.data(0))) {
    LOG(kWarning) << "Unable to parse connect success.";
    message.Clear();
    return;
  }

  NodeInfo peer;
  peer.id = NodeId(connect_success.node_id());
  peer.connection_id = NodeId(connect_success.connection_id());

  if (peer.id.IsZero() || peer.connection_id.IsZero()) {
    LOG(kWarning) << "Invalid node_id / connection_id provided";
    return;
  }

  if (!connect_success.requestor()) {
    ConnectSuccessFromResponder(peer, message.client_node());
  }
  message.Clear();  // message is sent directly to the peer
}

template <typename NodeType>
void Service<NodeType>::ConnectSuccessFromRequester(NodeInfo& /*peer*/) {}

template <typename NodeType>
void Service<NodeType>::ConnectSuccessFromResponder(NodeInfo& peer, bool client) {
  // Reply with ConnectSuccessAcknowledgement immediately
  LOG(kVerbose) << "ConnectSuccessFromResponder peer id : " << DebugId(peer.id);
  if (peer.connection_id == network_.bootstrap_connection_id()) {
    LOG(kVerbose) << "Special case : kConnectSuccess from bootstrapping node: " << DebugId(peer.id);
    return;
  }
  auto count =
      (client ? Parameters::max_routing_table_size_for_client : Parameters::max_routing_table_size);
  auto close_nodes_for_peer(connections_.routing_table.GetClosestNodes(peer.id, count));

  auto itr(std::find_if(std::begin(close_nodes_for_peer), std::end(close_nodes_for_peer),
                        [=](const NodeInfo & info)->bool { return (peer.id == info.id); }));
  if (itr != std::end(close_nodes_for_peer))
    close_nodes_for_peer.erase(itr);

  protobuf::Message connect_success_ack(rpcs::ConnectSuccessAcknowledgement(
      peer.id, connections_.kNodeId(), connections_.kConnectionId(),
      true,  // this node is requestor
      close_nodes_for_peer, NodeType::value));
  network_.SendToDirect(connect_success_ack, peer.id, peer.connection_id);
}

template <typename NodeType>
void Service<NodeType>::GetGroup(protobuf::Message& message) {
  LOG(kVerbose) << "Service<NodeType>::GetGroup,  msg id:  " << message.id();
  protobuf::GetGroup get_group;
  assert(get_group.ParseFromString(message.data(0)));
  auto close_nodes(connections_.routing_table.GetClosestNodes(NodeId(get_group.node_id()),
                                                              Parameters::group_size, true));
  get_group.set_node_id(connections_.kNodeId().string());
  for (const auto& node : close_nodes)
    get_group.add_group_nodes_id(node.id.string());
  message.clear_route_history();
  message.set_destination_id(message.source_id());
  message.set_source_id(connections_.kNodeId().string());
  message.clear_route_history();
  message.clear_data();
  message.add_data(get_group.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_client_node(NodeType::value);
  message.set_request(false);
  message.set_hops_to_live(Parameters::hops_to_live);
  assert(message.IsInitialized() && "unintialised message");
}

template <typename NodeType>
void Service<NodeType>::set_request_public_key_functor(RequestPublicKeyFunctor request_public_key) {
  request_public_key_functor_ = request_public_key;
}

template <typename NodeType>
RequestPublicKeyFunctor Service<NodeType>::request_public_key_functor() const {
  return request_public_key_functor_;
}

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_SERVICE_H_
