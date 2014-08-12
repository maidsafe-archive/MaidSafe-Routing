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
#include "maidsafe/routing/network.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/utils2.h"

namespace maidsafe {

namespace routing {

class ClientRoutingTable;

template <typename NodeType>
class Service : public std::enable_shared_from_this<Service<NodeType>> {
 public:
  Service(Connections<NodeType>& connections, Network<NodeType>& network,
          PublicKeyHolder& public_key_holder);
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
  bool CheckPriority(const NodeId& this_node, const NodeId& peer_node);
  void ValidateAndSendConnectResponse(protobuf::Message message, const NodeInfo& peer_node,
                                      const rudp::EndpointPair& peer_endpoint_pair);
  void SendConnectResponse(protobuf::Message message, const NodeInfo& peer_node_in,
                           const rudp::EndpointPair& peer_endpoint_pair);
  void HandleConnectSuccess(NodeInfo& peer, bool client);
  Connections<NodeType>& connections_;
  Network<NodeType>& network_;
  PublicKeyHolder& public_key_holder_;
  RequestPublicKeyFunctor request_public_key_functor_;
};

template <typename NodeType>
Service<NodeType>::Service(Connections<NodeType>& connections, Network<NodeType>& network,
                           PublicKeyHolder& public_key_holder)
    : connections_(connections), network_(network), public_key_holder_(public_key_holder),
      request_public_key_functor_() {}

template <typename NodeType>
Service<NodeType>::~Service() {}

template <typename NodeType>
void Service<NodeType>::Ping(protobuf::Message& message) {
  if (message.destination_id() != connections_.kNodeId()->string()) {
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
  message.clear_route_history();
  message.clear_data();
  message.set_hops_to_live(Parameters::hops_to_live);
  rpcs::detail::SetMessageProperties(
      message, IsRequestMessage(false), MessageData(ping_response.SerializeAsString()),
      PeerNodeId(NodeId(message.source_id())), LocalNodeId(connections_.kNodeId()));
  assert(message.IsInitialized() && "unintialised message");
}

template <typename NodeType>
void Service<NodeType>::Connect(protobuf::Message& message) {
  if (NodeId(message.destination_id()) != connections_.kNodeId()) {
    // Message not for this node and we should not pass it on.
    LOG(kError) << "Message not for this node.";
    message.Clear();
    return;
  }
  protobuf::ConnectRequest connect_request;
  if (!connect_request.ParseFromString(message.data(0))) {
    LOG(kVerbose) << "Unable to parse connect request.";
    message.Clear();
    return;
  }

  if (connect_request.peer_id() != connections_.kNodeId()->string()) {
    LOG(kError) << "Message not for this node.";
    message.Clear();
    return;
  }

  NodeInfo peer_node;
  peer_node.id = NodeId(connect_request.contact().node_id());
  peer_node.connection_id = NodeId(connect_request.contact().connection_id());
  LOG(kVerbose) << "[" << connections_.kNodeId() << "] received Connect request from "
                << peer_node.id;
  rudp::EndpointPair peer_endpoint_pair;
  peer_endpoint_pair.external =
      GetEndpointFromProtobuf(connect_request.contact().public_endpoint());
  peer_endpoint_pair.local = GetEndpointFromProtobuf(connect_request.contact().private_endpoint());

  if (peer_endpoint_pair.external.address().is_unspecified() &&
      peer_endpoint_pair.local.address().is_unspecified()) {
    LOG(kWarning) << "Invalid endpoint pair provided in connect request.";
    message.Clear();
    return;
  }
  if (!message.client_node()) {
    ValidateAndSendConnectResponse(message, peer_node, peer_endpoint_pair);
  } else {
    SendConnectResponse(message, peer_node, peer_endpoint_pair);
  }
  message.Clear();
}

template <>
void Service<ClientNode>::Connect(protobuf::Message& message);

template <typename NodeType>
void Service<NodeType>::ValidateAndSendConnectResponse(
    protobuf::Message message, const NodeInfo& peer_node,
    const rudp::EndpointPair& peer_endpoint_pair) {
  std::weak_ptr<Service<NodeType>> service_weak_ptr(this->shared_from_this());
  if (request_public_key_functor_) {
    auto validate_node([=](boost::optional<asymm::PublicKey> public_key) {
      if (!public_key) {
        LOG(kError) << "Failed to retrieve public key for: " << peer_node.id;
        return;
      }
      if (std::shared_ptr<Service> service = service_weak_ptr.lock()) {
        service->public_key_holder_.Add(NodeIdPublicKeyPair(peer_node.id, *public_key));
        service->SendConnectResponse(message, peer_node, peer_endpoint_pair);
      }
    });
    request_public_key_functor_(peer_node.id, validate_node);
  }
}

template <typename NodeType>
void Service<NodeType>::SendConnectResponse(protobuf::Message message, const NodeInfo& peer_node_in,
    const rudp::EndpointPair& peer_endpoint_pair) {
  protobuf::ConnectResponse connect_response;
  rudp::EndpointPair this_endpoint_pair;
  NodeInfo peer_node(peer_node_in);
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
  message.set_request(false);
  message.set_hops_to_live(Parameters::hops_to_live);
  if (message.has_source_id())
    message.set_destination_id(message.source_id());
  else
    message.clear_destination_id();
  message.set_source_id(connections_.kNodeId()->string());

  // Check rudp & routing
  bool check_node_succeeded(false);
  if (message.client_node()) {  // Client node, check non-routing table
    LOG(kVerbose) << "Client connect request - will check non-routing table.";
    NodeId furthest_close_node_id =
        connections_.routing_table.GetNthClosestNode(connections_.kNodeId(),
                                                     2 * Parameters::closest_nodes_size).id;
    check_node_succeeded = connections_.client_routing_table.CheckNode(peer_node,
                                                                       furthest_close_node_id);
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
                    << peer_node.id << ", Connection id :" << peer_node.connection_id
                    << ". peer_endpoint_pair.external = " << peer_endpoint_pair.external
                    << ", peer_endpoint_pair.local = " << peer_endpoint_pair.local
                    << ". Rudp returned :" << ret_val;
        message.add_data(connect_response.SerializeAsString());
        return;
      } else {  // Resolving collision by giving priority to lesser node id.
        if (!CheckPriority(peer_node.id, connections_.kNodeId())) {
          LOG(kInfo) << "Already ongoing attempt with : " << peer_node.connection_id;
          connect_response.set_answer(protobuf::ConnectResponseType::kConnectAttemptAlreadyRunning);
          message.add_data(connect_response.SerializeAsString());
          return;
        }
      }
    }

    assert((!this_endpoint_pair.external.address().is_unspecified() ||
            !this_endpoint_pair.local.address().is_unspecified()) &&
           "Unspecified endpoint after GetAvailableEndpoint success.");

    int add_result(AddToRudp(network_, connections_.kNodeId(), connections_.kConnectionId(),
                             PeerNodeId(peer_node.id), PeerConnectionId(peer_node.connection_id),
                             PeerEndpointPair(peer_endpoint_pair), IsRequestor(false)));
    if (rudp::kSuccess == add_result) {
      connect_response.set_answer(protobuf::ConnectResponseType::kAccepted);

      connect_response.mutable_contact()->set_node_id(connections_.kNodeId()->string());
      connect_response.mutable_contact()->set_connection_id(
          connections_.kConnectionId()->string());
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
  if (connections_.routing_table.size() == 0)  // This node can only send to bootstrap_endpoint
    network_.SendToDirect(message, network_.bootstrap_connection_id(),
                          network_.bootstrap_connection_id());
  else
    network_.SendToClosestNode(message);
}

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
  if (0 == find_nodes.num_nodes_requested() || NodeId(message.destination_id()).IsZero()) {
    LOG(kWarning) << "Invalid find node request.";
    message.Clear();
    return;
  }

  LOG(kVerbose) << "[" << connections_.kNodeId() << "] parsed find node request for target id : "
                << HexSubstr(message.destination_id());
  protobuf::FindNodesResponse found_nodes;
  auto nodes(connections_.routing_table.GetClosestNodes(
      NodeId(message.destination_id()),
      static_cast<uint16_t>(find_nodes.num_nodes_requested() - 1)));
  found_nodes.add_nodes(connections_.kNodeId()->string());

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
  message.set_source_id(connections_.kNodeId()->string());
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

  HandleConnectSuccess(peer, message.client_node());
  message.Clear();  // message is sent directly to the peer
}

template <typename NodeType>
void Service<NodeType>::HandleConnectSuccess(NodeInfo& peer, bool client) {
  // Reply with ConnectSuccessAcknowledgement immediately
  LOG(kVerbose) << "ConnectSuccessFromResponder peer id : " << peer.id;
  if (peer.connection_id == network_.bootstrap_connection_id()) {
    LOG(kVerbose) << "Special case : kConnectSuccess from bootstrapping node: " << peer.id;
    return;
  }
  if (client) {
    if (!ValidateAndAddToRoutingTable(network_, connections_, PeerNodeId(peer.id),
                                      PeerConnectionId(peer.connection_id), asymm::PublicKey(),
                                      ClientNode())) {
      LOG(kVerbose) << "Failed to add to routing table";
      return;
    }
  } else {
    auto peer_public_key(public_key_holder_.Find(peer.id));
    if (!peer_public_key) {
      LOG(kVerbose) << "Missing peer public key for " << peer.id;
      return;
    }
    if (!ValidateAndAddToRoutingTable(network_, connections_, PeerNodeId(peer.id),
                                      PeerConnectionId(peer.connection_id),
                                      peer_public_key->GetValue(), VaultNode())) {
      LOG(kVerbose) << "Failed to add to routing table";
      return;
    } else {
      public_key_holder_.Remove(peer.id);
    }
  }

  auto count = (client ? Params<ClientNode>::max_routing_table_size
                       : Params<VaultNode>::max_routing_table_size);
  auto close_nodes_for_peer(connections_.routing_table.GetClosestNodes(peer.id, count));

  close_nodes_for_peer.erase(
      std::remove_if(std::begin(close_nodes_for_peer), std::end(close_nodes_for_peer),
                     [this, peer](const NodeInfo& info) {
                       return (info.id == peer.id) || (info.id == connections_.kNodeId());
                     }), std::end(close_nodes_for_peer));

  LOG(kVerbose) << "Service<NodeType>::HandleConnectSuccess " << close_nodes_for_peer.size();
  for (const auto& close_node_for_peer : close_nodes_for_peer)
    LOG(kVerbose) << close_node_for_peer.id;

  protobuf::Message connect_success_ack(rpcs::ConnectSuccessAcknowledgement(
      PeerNodeId(peer.id), connections_.kNodeId(), connections_.kConnectionId(),
      IsRequestor(false), close_nodes_for_peer, IsClient(NodeType::value)));
  network_.SendToDirect(connect_success_ack, peer.id, peer.connection_id);
}

template <typename NodeType>
void Service<NodeType>::GetGroup(protobuf::Message& message) {
  LOG(kVerbose) << "Service<NodeType>::GetGroup,  msg id:  " << message.id();
  protobuf::GetGroup get_group;
  assert(get_group.ParseFromString(message.data(0)));
  auto close_nodes(connections_.routing_table.GetClosestNodes(NodeId(get_group.node_id()),
                                                              Parameters::group_size, true));
  get_group.set_node_id(connections_.kNodeId()->string());
  for (const auto& node : close_nodes)
    get_group.add_group_nodes_id(node.id.string());
  message.clear_route_history();
  message.set_destination_id(message.source_id());
  message.set_source_id(connections_.kNodeId()->string());
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
