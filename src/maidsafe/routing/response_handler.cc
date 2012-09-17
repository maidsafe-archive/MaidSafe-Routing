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

#include "maidsafe/routing/response_handler.h"

#include<memory>
#include<vector>
#include<string>
#include <algorithm>

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/utils.h"


namespace maidsafe {

namespace routing {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // unnamed namespace

ResponseHandler::ResponseHandler(RoutingTable& routing_table,
                                 NonRoutingTable& non_routing_table,
                                 NetworkUtils& network)
    : routing_table_(routing_table),
      non_routing_table_(non_routing_table),
      network_(network),
      request_public_key_functor_() {}

void ResponseHandler::Ping(protobuf::Message& message) {
  // Always direct, never pass on

  // TODO(dirvine): do we need this and where and how can I update the response
  protobuf::PingResponse ping_response;
  if (ping_response.ParseFromString(message.data(0))) {
    // do stuff here
  }
}

void ResponseHandler::Connect(protobuf::Message& message) {
  protobuf::ConnectResponse connect_response;
  protobuf::ConnectRequest connect_request;
  if (!connect_response.ParseFromString(message.data(0))) {
    LOG(kError) << "Could not parse connect response";
    return;
  }

  if (!connect_response.answer()) {
    LOG(kVerbose) << "Peer rejected this node's connection request." << " id: " << message.id();
    return;
  }

  if (!connect_request.ParseFromString(connect_response.original_request())) {
    LOG(kError) << "Could not parse original connect request" << " id: " << message.id();
    return;
  }

  if (!NodeId(connect_response.contact().node_id()).IsValid() ||
      NodeId(connect_response.contact().node_id()).Empty()) {
    LOG(kError) << "Invalid contact details";
    return;
  }

  NodeInfo node_to_add;
  node_to_add.node_id = NodeId(connect_response.contact().node_id());

  if (routing_table_.CheckNode(node_to_add) ||
      (node_to_add.node_id == network_.bootstrap_connection_id())) {
    rudp::EndpointPair peer_endpoint_pair;
    peer_endpoint_pair.external =
        GetEndpointFromProtobuf(connect_response.contact().public_endpoint());
    peer_endpoint_pair.local =
        GetEndpointFromProtobuf(connect_response.contact().private_endpoint());

    if (peer_endpoint_pair.external.address().is_unspecified() &&
        peer_endpoint_pair.local.address().is_unspecified()) {
      LOG(kError) << "Invalid peer endpoint details";
      return;
    }
    // Updating bootstrap only for test local network
    if (!peer_endpoint_pair.external.address().is_unspecified())
      network_.AddToBootstrapFile(peer_endpoint_pair.external);
    else
      network_.AddToBootstrapFile(peer_endpoint_pair.local);

    NodeId peer_node_id(connect_response.contact().node_id());
    NodeId this_node_seen_connection_id;
    if (connect_response.has_connection_id())
      this_node_seen_connection_id = NodeId((connect_response.connection_id()));

    LOG(kVerbose) << "This node [" << DebugId(routing_table_.kNodeId())
                  << "] received connect response from "
                  << DebugId(peer_node_id)
                  << " id: " << message.id();

    // Handling the case when this and peer node are behind symmetric router
//    rudp::NatType nat_type = NatTypeFromProtobuf(connect_response.contact().nat_type());

//    if ((nat_type == rudp::NatType::kSymmetric) &&
//        (network_.nat_type() == rudp::NatType::kSymmetric)) {
//      auto validate_node = [&] (const asymm::PublicKey& key)->void {
//          LOG(kInfo) << "validation callback called with public key for" << DebugId(peer_node_id)
//                     << " -- pseudo connection";
//          HandleSymmetricNodeAdd(routing_table_, peer_node_id, key);
//        };

//      TaskResponseFunctor add_symmetric_node = [&](std::vector<std::string>) {
//          if (request_public_key_functor_) {
//            request_public_key_functor_(peer_node_id, validate_node);
//          }
//        };
//      network_.timer().AddTask(boost::posix_time::seconds(5), add_symmetric_node, 1);
//    }

    std::weak_ptr<ResponseHandler> response_handler_weak_ptr = shared_from_this();
    if (request_public_key_functor_) {
      auto validate_node([=] (const asymm::PublicKey& key) {
                             LOG(kInfo) << "Validation callback called with public key for "
                                        << DebugId(peer_node_id);
                             if (std::shared_ptr<ResponseHandler> response_handler =
                                 response_handler_weak_ptr.lock()) {
                               ValidateAndAddToRudp(
                                   response_handler->network_,
                                   response_handler->routing_table_.kNodeId(),
                                   this_node_seen_connection_id,
                                   peer_node_id,
                                   peer_node_id,
                                   peer_endpoint_pair,
                                   key,
                                   response_handler->routing_table_.client_mode());
                             }
                           });
      request_public_key_functor_(peer_node_id, validate_node);
    }
  } else {
    LOG(kInfo) << "Already added node, will check other closest_nodes";
  }

  std::vector<std::string> closest_nodes(connect_request.closest_id().begin(),
                                         connect_request.closest_id().end());
  closest_nodes.push_back(message.source_id());
  for (auto node_id : connect_response.closer_id())
    LOG(kVerbose) << "Connecting to closer id: " << HexSubstr(node_id);

  ConnectTo(std::vector<std::string>(connect_response.closer_id().begin(),
                                     connect_response.closer_id().end()),
            closest_nodes);
}

void ResponseHandler::FindNodes(const protobuf::Message& message) {
  protobuf::FindNodesResponse find_nodes;
  if (!find_nodes.ParseFromString(message.data(0))) {
    LOG(kError) << "Could not parse find node response";
    return;
  }
//  if (asymm::CheckSignature(find_nodes.original_request(),
//                            find_nodes.original_signature(),
//                            routing_table.kKeys().public_key) != kSuccess) {
//    LOG(kError) << " find node request was not signed by us";
//    return;  // we never requested this
//  }

  LOG(kVerbose) << "This node [" << DebugId(routing_table_.kNodeId())
                << "] received FindNodes response from " << HexSubstr(message.source_id())
                << " id: " << message.id();
  std::string find_node_result = "FindNodes from " + HexSubstr(message.source_id()) +
          " returned :\n";
  for (int i = 0; i < find_nodes.nodes_size(); ++i) {
    find_node_result += "[" + HexSubstr(find_nodes.nodes(i)) + "]\t";
  }
  LOG(kVerbose) << find_node_result;

  ConnectTo(std::vector<std::string>(find_nodes.nodes().begin(), find_nodes.nodes().end()),
            std::vector<std::string>(find_nodes.nodes().begin(), find_nodes.nodes().end()));
}

void ResponseHandler::ConnectTo(const std::vector<std::string>& nodes,
                                const std::vector<std::string>& closest_nodes) {
  std::vector<std::string> closest_node_ids;
  auto routing_table_closest_nodes(routing_table_.GetClosestNodes(
                                   routing_table_.kNodeId(),
                                   Parameters::max_routing_table_size));

  for (auto node_id : routing_table_closest_nodes)
    closest_node_ids.push_back(node_id.String());

  closest_node_ids.insert(closest_node_ids.end(),
                          closest_nodes.begin(),
                          closest_nodes.end());

  closest_node_ids.insert(closest_node_ids.end(),
                          nodes.begin(),
                          nodes.end());

  std::sort(closest_node_ids.begin(), closest_node_ids.end(),
            [=](const std::string& lhs, const std::string& rhs)->bool {
              return NodeId::CloserToTarget(NodeId(lhs), NodeId(rhs),
                                            NodeId(routing_table_.kKeys().identity));
            });
  auto iter(std::unique(closest_node_ids.begin(), closest_node_ids.end()));
  auto resize = std::min(static_cast<uint16_t>(std::distance(iter, closest_node_ids.begin())),
                          Parameters::max_routing_table_size);
  if (closest_node_ids.size() > resize)
    closest_node_ids.resize(resize);
  if (network_.bootstrap_connection_id().Empty() && (routing_table_.Size() == 0)) {
      LOG(kWarning) << "Need to re bootstrap !";
    return;
  }

  bool send_to_bootstrap_connection((routing_table_.Size() < Parameters::closest_nodes_size) &&
                                    !network_.bootstrap_connection_id().Empty());
  for (uint16_t i = 0; i < nodes.size(); ++i) {
    NodeInfo node_to_add;
    node_to_add.node_id = NodeId(nodes.at(i));
    if (node_to_add.node_id == NodeId(routing_table_.kKeys().identity))
      continue;  // TODO(Prakash): FIXME handle collision and return kIdCollision on join()

    if (routing_table_.CheckNode(node_to_add)) {
      LOG(kVerbose) << "CheckNode succeeded for node " << DebugId(node_to_add.node_id);
      rudp::EndpointPair this_endpoint_pair, peer_endpoint_pair;
      rudp::NatType this_nat_type(rudp::NatType::kUnknown);
      int ret_val = network_.GetAvailableEndpoint(NodeId(nodes.at(i)),
                                                  peer_endpoint_pair,
                                                  this_endpoint_pair,
                                                  this_nat_type);
      if (kSuccess != ret_val) {
        LOG(kError) << "Failed to get available endpoint for new connections : " << ret_val;
        return;
      }
      assert((!this_endpoint_pair.external.address().is_unspecified() ||
              !this_endpoint_pair.local.address().is_unspecified()) &&
             "Unspecified endpoint after GetAvailableEndpoint success.");
      NodeId relay_connection_id;
      bool relay_message(false);
      if (send_to_bootstrap_connection) {
        // Not in any peer's routing table, need a path back through relay IP.
        relay_connection_id = network_.this_node_relay_connection_id();
        relay_message = true;
      }

      protobuf::Message connect_rpc(
          rpcs::Connect(NodeId(nodes.at(i)),
          this_endpoint_pair,
          routing_table_.kNodeId(),
          closest_node_ids,
          routing_table_.client_mode(),
          this_nat_type,
          relay_message,
          relay_connection_id));
      LOG(kVerbose) << "Sending Connect RPC to " << HexSubstr(nodes.at(i))
                    << " message id : " << connect_rpc.id();
      if (send_to_bootstrap_connection)
        network_.SendToDirect(connect_rpc, network_.bootstrap_connection_id());
      else
        network_.SendToClosestNode(connect_rpc);
    }
  }
}

void ResponseHandler::ConnectSuccess(protobuf::Message& message) {
  protobuf::ConnectSuccess connect_success;

  if (!connect_success.ParseFromString(message.data(0))) {
    LOG(kWarning) << "Unable to parse connect success.";
    message.Clear();
    return;
  }

  NodeId peer_node_id(connect_success.node_id());
  NodeId peer_connection_id(connect_success.connection_id());
  if (peer_node_id.Empty() || !peer_node_id.IsValid()) {
    LOG(kWarning) << "Invalid node id provided";
    return;
  }

  std::weak_ptr<ResponseHandler> response_handler_weak_ptr = shared_from_this();
  if (request_public_key_functor_) {
    auto validate_node([=] (const asymm::PublicKey& key) {
                           LOG(kInfo) << "Validation callback called with public key for "
                                      << DebugId(peer_node_id);
                           if (std::shared_ptr<ResponseHandler> response_handler =
                               response_handler_weak_ptr.lock()) {
                             ValidateAndAddToRoutingTable(
                                 response_handler->network_,
                                 response_handler->routing_table_,
                                 response_handler->non_routing_table_,
                                 peer_node_id,
                                 peer_connection_id,
                                 key,
                                 message.client_node());
                           }
                         });
      request_public_key_functor_(peer_node_id, validate_node);
  }
}

void ResponseHandler::set_request_public_key_functor(RequestPublicKeyFunctor request_public_key) {
  request_public_key_functor_ = request_public_key;
}

RequestPublicKeyFunctor ResponseHandler::request_public_key_functor() const {
  return request_public_key_functor_;
}

}  // namespace routing

}  // namespace maidsafe
