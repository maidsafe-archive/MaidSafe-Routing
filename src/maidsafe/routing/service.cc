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

#include "maidsafe/routing/service.h"

#include <string>
#include <algorithm>
#include <vector>

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/utils.h"


namespace maidsafe {

namespace routing {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // unnamed namespace

Service::Service(RoutingTable& routing_table,
                 NonRoutingTable& non_routing_table,
                 NetworkUtils& network)
  : routing_table_(routing_table),
    non_routing_table_(non_routing_table),
    network_(network),
    request_public_key_functor_() {}

Service::~Service() {}

void Service::Ping(protobuf::Message& message) {
  if (message.destination_id() != routing_table_.kKeys().identity) {
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
  ping_response.set_timestamp(GetTimeStamp());
  message.set_request(false);
  message.clear_route_history();
  message.clear_data();
  message.add_data(ping_response.SerializeAsString());
  message.set_destination_id(message.source_id());
  message.set_source_id(routing_table_.kKeys().identity);
  message.set_hops_to_live(Parameters::hops_to_live);
  assert(message.IsInitialized() && "unintialised message");
}

void Service::Connect(protobuf::Message& message) {
  asymm::Keys keys = routing_table_.kKeys();
  if (message.destination_id() != routing_table_.kKeys().identity) {
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
  NodeInfo peer_node;
  peer_node.node_id = NodeId(connect_request.contact().node_id());
  peer_node.connection_id = NodeId(connect_request.contact().connection_id());
  LOG(kVerbose) << "[" << DebugId(routing_table_.kNodeId()) << "]"
                << " received Connect request from "
                << DebugId(peer_node.node_id);
  connect_response.set_answer(false);
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

  bool check_node_succeeded(false);
  if (message.client_node()) {  // Client node, check non-routing table
    LOG(kVerbose) << "Client connect request - will check non-routing table.";
    NodeId furthest_close_node_id =
        routing_table_.GetNthClosestNode(routing_table_.kNodeId(),
                                         Parameters::closest_nodes_size).node_id;
    check_node_succeeded = non_routing_table_.CheckNode(peer_node, furthest_close_node_id);
  } else {
    LOG(kVerbose) << "Server connect request - will check routing table.";
    check_node_succeeded = routing_table_.CheckNode(peer_node);
  }

  if (check_node_succeeded) {
    LOG(kVerbose) << "CheckNode(node) for " << (message.client_node() ? "client" : "server")
                  << " node succeeded.";
//    rudp::NatType peer_nat_type = NatTypeFromProtobuf(connect_request.contact().nat_type());
    rudp::NatType this_nat_type(rudp::NatType::kUnknown);

    int ret_val = network_.GetAvailableEndpoint(peer_node.connection_id, peer_endpoint_pair,
                                               this_endpoint_pair, this_nat_type);
    if (ret_val != rudp::kSuccess) {
      LOG(kError) << "[" << DebugId(routing_table_.kNodeId()) << "] Service: "
                  << "Failed to get available endpoint for new connection to node id : "
                  << DebugId(peer_node.node_id)
                  << ", Connection id :"
                  << DebugId(peer_node.connection_id)
                  << ". peer_endpoint_pair.external = "
                  << peer_endpoint_pair.external
                  << ", peer_endpoint_pair.local = "
                  << peer_endpoint_pair.local
                  << ". Rudp returned :"
                  << ret_val;
      message.Clear();
      return;
    }

    assert((!this_endpoint_pair.external.address().is_unspecified() ||
            !this_endpoint_pair.local.address().is_unspecified()) &&
           "Unspecified endpoint after GetAvailableEndpoint success.");


//  // Handling the case when this node and peer node are behind symmetric router
//    if ((peer_nat_type == rudp::NatType::kSymmetric) &&
//        (this_nat_type == rudp::NatType::kSymmetric)) {
//      auto validate_node = [=, &routing_table] (const asymm::PublicKey& key)->void {
//          LOG(kInfo) << "Validation callback called with public key for "
//                     << DebugId(peer_node.node_id)
//                     << " -- pseudo connection";
//          HandleSymmetricNodeAdd(routing_table, peer_node.node_id, key);
//        };

//      TaskResponseFunctor add_symmetric_node =
//        [=, &routing_table, &request_public_key_functor](std::vector<std::string>) {
//          if (request_public_key_functor)
//            request_public_key_functor(peer_node.node_id, validate_node);
//        };
//      network.timer().AddTask(boost::posix_time::seconds(5), add_symmetric_node, 1);
//    }
    if (request_public_key_functor_) {
      auto validate_node =
          [=, &routing_table_, &non_routing_table_, &network_] (const asymm::PublicKey& key)->void {
            LOG(kInfo) << "validation callback called with public key for "
                       << DebugId(peer_node.node_id);
            ValidateAndAddToRudp(network_,
                                 routing_table_.kNodeId(),
                                 routing_table_.kConnectionId(),
                                 peer_node.node_id,
                                 peer_node.connection_id,
                                 peer_endpoint_pair,
                                 key,
                                 routing_table_.client_mode());
          };
      request_public_key_functor_(peer_node.node_id, validate_node);
      connect_response.set_answer(true);

      connect_response.mutable_contact()->set_node_id(routing_table_.kNodeId().String());
      connect_response.mutable_contact()->set_connection_id(
          routing_table_.kConnectionId().String());
      connect_response.mutable_contact()->set_nat_type(NatTypeProtobuf(this_nat_type));

      SetProtobufEndpoint(this_endpoint_pair.local,
                          connect_response.mutable_contact()->mutable_private_endpoint());
      SetProtobufEndpoint(this_endpoint_pair.external,
                          connect_response.mutable_contact()->mutable_public_endpoint());
    } else {
      LOG(kError) << "request_public_key_functor not available for getting key";
    }
  }

  connect_response.set_timestamp(GetTimeStamp());
  connect_response.set_original_request(message.data(0));
  connect_response.set_original_signature(message.signature());
  NodeId source((message.has_relay_connection_id() ? message.relay_id() : message.source_id()));
  if (connect_request.closest_id_size() > 0) {
    for (auto node_id : routing_table_.GetClosestNodes(source,
        ((message.client_node()) ? Parameters::closest_nodes_size - 1 :
                                   Parameters::max_routing_table_size - 1))) {
      if (std::find(connect_request.closest_id().begin(), connect_request.closest_id().end(),
                    node_id.String()) == connect_request.closest_id().end() &&
          (NodeId::CloserToTarget(node_id,
              NodeId(connect_request.closest_id(connect_request.closest_id_size() - 1)), source) ||
              (connect_request.closest_id_size() + connect_response.closer_id_size() <=
            ((message.client_node()) ? Parameters::closest_nodes_size :
                                       Parameters::max_routing_table_size))) &&
          source != node_id)
        connect_response.add_closer_id(node_id.String());
      LOG(kVerbose) << "Returning closer id size" << connect_response.closer_id_size()
                    << ", RT size : " << routing_table_.Size();
    }
  }
  message.clear_route_history();
  message.clear_data();
  message.add_data(connect_response.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_client_node(routing_table_.client_mode());
  message.set_request(false);
  message.set_hops_to_live(Parameters::hops_to_live);
  if (message.has_source_id())
    message.set_destination_id(message.source_id());
  else
    message.clear_destination_id();
  message.set_source_id(routing_table_.kKeys().identity);
  assert(message.IsInitialized() && "unintialised message");
}

void Service::FindNodes(protobuf::Message& message) {
  protobuf::FindNodesRequest find_nodes;
  if (!find_nodes.ParseFromString(message.data(0))) {
    LOG(kWarning) << "Unable to parse find node request.";
    message.Clear();
    return;
  }
  if ((0 == find_nodes.num_nodes_requested() ||
      NodeId(find_nodes.target_node()).Empty() ||
      !NodeId(find_nodes.target_node()).IsValid())) {
    LOG(kWarning) << "Invalid find node request.";
    message.Clear();
    return;
  }

  LOG(kVerbose) << "[" << DebugId(routing_table_.kNodeId()) << "]"
                << " parsed find node request for target id : "
                << HexSubstr(find_nodes.target_node());
  protobuf::FindNodesResponse found_nodes;
  std::vector<NodeId> nodes(routing_table_.GetClosestNodes(NodeId(find_nodes.target_node()),
                              static_cast<uint16_t>(find_nodes.num_nodes_requested() - 1)));
  found_nodes.add_nodes(routing_table_.kKeys().identity);

  for (auto node : nodes)
    found_nodes.add_nodes(node.String());

  LOG(kVerbose) << "Responding Find node with " << found_nodes.nodes_size()  << " contacts.";

  found_nodes.set_original_request(message.data(0));
  found_nodes.set_original_signature(message.signature());
  found_nodes.set_timestamp(GetTimeStamp());
  assert(found_nodes.IsInitialized() && "unintialised found_nodes response");
  if (message.has_source_id()) {
    message.set_destination_id(message.source_id());
  } else {
    message.clear_destination_id();
    LOG(kVerbose) << "Relay message, so not setting destination ID.";
  }
  message.set_source_id(routing_table_.kKeys().identity);
  message.clear_route_history();
  message.clear_data();
  message.add_data(found_nodes.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_client_node(routing_table_.client_mode());
  message.set_request(false);
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
