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

#include "maidsafe/routing/service.h"

#include <string>
#include <algorithm>
#include <vector>

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/client_routing_table.h"
#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/network.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // unnamed namespace

Service::Service(RoutingTable& routing_table, ClientRoutingTable& client_routing_table,
                 Network& network, PublicKeyHolder& public_key_holder)
    : mutex_(), routing_table_(routing_table), client_routing_table_(client_routing_table),
      network_(network), request_public_key_functor_(), public_key_holder_(public_key_holder) {}

Service::~Service() {}

void Service::Ping(protobuf::Message& message) {
  if (message.destination_id() != routing_table_.kNodeId().string()) {
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
  message.set_ack_id(RandomInt32());
  message.clear_route_history();
  message.clear_data();
  message.add_data(ping_response.SerializeAsString());
  message.set_destination_id(message.source_id());
  message.set_source_id(routing_table_.kNodeId().string());
  message.set_hops_to_live(Parameters::hops_to_live);
  assert(message.IsInitialized() && "unintialised message");
}

void Service::Connect(protobuf::Message& message) {
  if (message.destination_id() != routing_table_.kNodeId().string()) {
    // Message not for this node and we should not pass it on.
    LOG(kError) << "Message not for this node.";
    message.Clear();
    return;
  }
  protobuf::ConnectRequest connect_request;
  if (!connect_request.ParseFromString(message.data(0))) {
    message.Clear();
    return;
  }

  if (connect_request.peer_id() != routing_table_.kNodeId().string()) {
    LOG(kError) << "Message not for this node.";
    message.Clear();
    return;
  }

  NodeInfo peer_node;
  peer_node.id = NodeId(connect_request.contact().node_id());
  peer_node.connection_id = NodeId(connect_request.contact().connection_id());
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

void Service::ValidateAndSendConnectResponse(protobuf::Message message, const NodeInfo& peer_node,
    const rudp::EndpointPair& peer_endpoint_pair) {
  std::weak_ptr<Service> service_weak_ptr = shared_from_this();
  if (request_public_key_functor_) {
    auto validate_node([=](boost::optional<asymm::PublicKey> public_key) {
      if (!public_key) {
        LOG(kError) << "Failed to retrieve public key for: " << peer_node.id;
        return;
      }
      if (std::shared_ptr<Service> service = service_weak_ptr.lock()) {
        service->public_key_holder_.Add(peer_node.id, *public_key);
        service->SendConnectResponse(message, peer_node, peer_endpoint_pair);
      }
    });
    request_public_key_functor_(peer_node.id, validate_node);
  }
}

void Service::SendConnectResponse(protobuf::Message message, const NodeInfo& peer_node_in,
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
  message.set_client_node(routing_table_.client_mode());
  message.set_request(false);
  message.set_ack_id(RandomInt32());
  message.set_hops_to_live(Parameters::hops_to_live);
  if (message.has_source_id())
    message.set_destination_id(message.source_id());
  else
    message.clear_destination_id();
  message.set_source_id(routing_table_.kNodeId().string());

  // Check rudp & routing
  bool check_node_succeeded(false);
  if (message.client_node()) {  // Client node, check non-routing table
    NodeId furthest_close_node_id =
        routing_table_.GetNthClosestNode(routing_table_.kNodeId(),
                                         2 * Parameters::closest_nodes_size).id;
    check_node_succeeded = client_routing_table_.CheckNode(peer_node, furthest_close_node_id);
  } else {
    check_node_succeeded = routing_table_.CheckNode(peer_node);
  }

  if (check_node_succeeded) {
    rudp::NatType this_nat_type(rudp::NatType::kUnknown);
    int ret_val = network_.GetAvailableEndpoint(peer_node.connection_id, peer_endpoint_pair,
                                                this_endpoint_pair, this_nat_type);
    if (ret_val != rudp::kSuccess && ret_val != rudp::kBootstrapConnectionAlreadyExists) {
      if (rudp::kUnvalidatedConnectionAlreadyExists != ret_val &&
          rudp::kConnectAttemptAlreadyRunning != ret_val) {
        LOG(kError) << "[" << routing_table_.kNodeId() << "] Service: "
                    << "Failed to get available endpoint for new connection to node id : "
                    << peer_node.id << ", Connection id :" << peer_node.connection_id
                    << ". peer_endpoint_pair.external = " << peer_endpoint_pair.external
                    << ", peer_endpoint_pair.local = " << peer_endpoint_pair.local
                    << ". Rudp returned :" << ret_val;
        message.add_data(connect_response.SerializeAsString());
        return;
      } else {  // Resolving collision by giving priority to lesser node id.
        if (!CheckPriority(peer_node.id, routing_table_.kNodeId())) {
          connect_response.set_answer(protobuf::ConnectResponseType::kConnectAttemptAlreadyRunning);
          message.add_data(connect_response.SerializeAsString());
          return;
        }
      }
    }

    assert((!this_endpoint_pair.external.address().is_unspecified() ||
            !this_endpoint_pair.local.address().is_unspecified()) &&
           "Unspecified endpoint after GetAvailableEndpoint success.");

    int add_result(AddToRudp(network_, routing_table_.kNodeId(), routing_table_.kConnectionId(),
                             peer_node.id, peer_node.connection_id, peer_endpoint_pair, false,
                             routing_table_.client_mode()));
    if (rudp::kSuccess == add_result) {
      connect_response.set_answer(protobuf::ConnectResponseType::kAccepted);

      connect_response.mutable_contact()->set_node_id(routing_table_.kNodeId().string());
      connect_response.mutable_contact()->set_connection_id(
          routing_table_.kConnectionId().string());
      connect_response.mutable_contact()->set_nat_type(NatTypeProtobuf(this_nat_type));

      SetProtobufEndpoint(this_endpoint_pair.local,
                          connect_response.mutable_contact()->mutable_private_endpoint());
      SetProtobufEndpoint(this_endpoint_pair.external,
                          connect_response.mutable_contact()->mutable_public_endpoint());
    }
  }

  message.add_data(connect_response.SerializeAsString());
  assert(message.IsInitialized() && "unintialised message");
  if (routing_table_.size() == 0)  // This node can only send to bootstrap_endpoint
    network_.SendToDirect(message, network_.bootstrap_connection_id(),
                          network_.bootstrap_connection_id());
  else
    network_.SendToClosestNode(message);
}

bool Service::CheckPriority(const NodeId& this_node, const NodeId& peer_node) {
  assert(this_node != peer_node);
  return (this_node > peer_node);
}

void Service::FindNodes(protobuf::Message& message) {
  protobuf::FindNodesRequest find_nodes;
  if (!find_nodes.ParseFromString(message.data(0))) {
    LOG(kWarning) << "Unable to parse find node request.";
    message.Clear();
    return;
  }
  if (0 == find_nodes.num_nodes_requested() || !NodeId(message.destination_id()).IsValid()) {
    LOG(kWarning) << "Invalid find node request.";
    message.Clear();
    return;
  }

  protobuf::FindNodesResponse found_nodes;
  auto nodes(routing_table_.GetClosestNodes(
                 NodeId(message.destination_id()),
                 static_cast<unsigned int>(find_nodes.num_nodes_requested() - 1)));
  found_nodes.add_nodes(routing_table_.kNodeId().string());

  for (const auto& node : nodes) {
    if (node.id != NodeId(message.source_id()))
      found_nodes.add_nodes(node.id.string());
  }

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
  }
  message.set_source_id(routing_table_.kNodeId().string());
  message.clear_route_history();
  message.clear_data();
  message.add_data(found_nodes.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_ack_id(RandomInt32());
  message.set_client_node(routing_table_.client_mode());
  message.set_request(false);
  message.set_hops_to_live(Parameters::hops_to_live);
  assert(message.IsInitialized() && "unintialised message");
}

void Service::ConnectSuccess(protobuf::Message& message) {
  protobuf::ConnectSuccess connect_success;

  if (!connect_success.ParseFromString(message.data(0))) {
    LOG(kWarning) << "Unable to parse connect success.";
    message.Clear();
    return;
  }

  NodeInfo peer;
  peer.id = NodeId(connect_success.node_id());
  peer.connection_id = NodeId(connect_success.connection_id());

  if (!peer.id.IsValid() || !peer.connection_id.IsValid()) {
    LOG(kWarning) << "Invalid node_id / connection_id provided";
    return;
  }

  HandleConnectSuccess(peer, message.client_node());
  message.Clear();  // message is sent directly to the peer
}

void Service::HandleConnectSuccess(NodeInfo& peer, bool client) {
  // Reply with ConnectSuccessAcknowledgement immediately
  if (peer.connection_id == network_.bootstrap_connection_id()) {
    return;
  }
  if (client) {
    if (!ValidateAndAddToRoutingTable(network_, routing_table_, client_routing_table_, peer.id,
                                      peer.connection_id, asymm::PublicKey(), true)) {
      return;
    }
  } else {
    auto peer_public_key(public_key_holder_.Find(peer.id));
    if (!peer_public_key) {
      return;
    }
    if (!ValidateAndAddToRoutingTable(network_, routing_table_, client_routing_table_, peer.id,
                                      peer.connection_id, *peer_public_key, false)) {
      return;
    } else {
      public_key_holder_.Remove(peer.id);
    }
  }

  auto count =
       (client ? Parameters::max_routing_table_size_for_client
               : Parameters::max_routing_table_size);
  auto close_nodes_for_peer(routing_table_.GetClosestNodes(peer.id, count));

  close_nodes_for_peer.erase(
      std::remove_if(std::begin(close_nodes_for_peer), std::end(close_nodes_for_peer),
                     [this, peer](const NodeInfo& info) {
                       return (info.id == peer.id) || (info.id == routing_table_.kNodeId());
                     }), std::end(close_nodes_for_peer));

  protobuf::Message connect_success_ack(rpcs::ConnectSuccessAcknowledgement(
      peer.id, routing_table_.kNodeId(), routing_table_.kConnectionId(),
      false,  // this node is responder
      close_nodes_for_peer, routing_table_.client_mode()));
  network_.SendToDirect(connect_success_ack, peer.id, peer.connection_id);
}

void Service::GetGroup(protobuf::Message& message) {
  protobuf::GetGroup get_group;
  assert(get_group.ParseFromString(message.data(0)));
  auto close_nodes(routing_table_.GetClosestNodes(NodeId(get_group.node_id()),
                                                  Parameters::group_size, true));
  get_group.set_node_id(routing_table_.kNodeId().string());
  for (const auto& node : close_nodes)
    get_group.add_group_nodes_id(node.id.string());
  message.clear_route_history();
  message.set_destination_id(message.source_id());
  message.set_source_id(routing_table_.kNodeId().string());
  message.clear_route_history();
  message.clear_data();
  message.add_data(get_group.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_client_node(routing_table_.client_mode());
  message.set_request(false);
  message.set_ack_id(RandomInt32());
  message.set_hops_to_live(Parameters::hops_to_live);
  assert(message.IsInitialized() && "unintialised message");
}

void Service::set_request_public_key_functor(RequestPublicKeyFunctor request_public_key) {
  request_public_key_functor_ = request_public_key;
}

RequestPublicKeyFunctor Service::request_public_key_functor() const {
  return request_public_key_functor_;
}

}  // namespace routing

}  // namespace maidsafe
