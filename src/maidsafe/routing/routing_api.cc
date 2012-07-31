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

#include "maidsafe/routing/routing_api.h"

#include <cstdint>
#include <functional>

#include "boost/thread/future.hpp"

#include "maidsafe/common/log.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/bootstrap_file_handler.h"
#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_private.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/timer.h"
#include "maidsafe/routing/utils.h"


namespace args = std::placeholders;
namespace fs = boost::filesystem;

namespace maidsafe {

namespace routing {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // unnamed namespace

Routing::Routing(const asymm::Keys& keys, const bool& client_mode)
    : impl_(new RoutingPrivate(keys, client_mode)) {
  if (!CheckBootstrapFilePath())
    LOG(kInfo) << "No bootstrap nodes, require BootStrapFromThisEndpoint()";

  if (!client_mode)
    assert(!keys.identity.empty() && "Server Nodes cannot be created without valid keys");

  if (keys.identity.empty()) {
    impl_->anonymous_node_ = true;
    asymm::GenerateKeyPair(&impl_->keys_);
    impl_->keys_.identity = NodeId(RandomString(64)).String();
    impl_->routing_table_.set_keys(impl_->keys_);
    LOG(kInfo) << "Anonymous node ID: " << HexSubstr(impl_->keys_.identity);
  }
}

Routing::~Routing() {
  LOG(kVerbose) << "~Routing() - Disconnecting functors.  Routing table Size: "
                << impl_->routing_table_.Size();
  DisconnectFunctors();
}

// TODO(Prakash) For client nodes, copy bootstrap file from sys dir if it's not available @ user dir
bool Routing::CheckBootstrapFilePath() const {
  LOG(kVerbose) << "Application Path " << GetUserAppDir();
  LOG(kVerbose) << "System Path " << GetSystemAppDir();
  fs::path path;
  std::string file_name;
  if (impl_->client_mode_) {
    file_name = "bootstrap";
    path = GetUserAppDir() / file_name;
  } else {
    file_name = "bootstrap." + EncodeToBase32(impl_->keys_.identity);
    path = GetSystemAppDir() / file_name;
  }
  fs::path local_file = fs::current_path() / file_name;
  boost::system::error_code exists_error_code, is_regular_file_error_code;
  if (!fs::exists(local_file, exists_error_code) ||
      !fs::is_regular_file(local_file, is_regular_file_error_code) ||
      exists_error_code || is_regular_file_error_code) {
    if (exists_error_code) {
      LOG(kError) << "Failed to check for bootstrap file at " << local_file << ".  "
                  << exists_error_code.message();
    }
    if (is_regular_file_error_code) {
      LOG(kError) << "Failed to check for bootstrap file at " << local_file << ".  "
                  << is_regular_file_error_code.message();
    }
  } else {
    LOG(kVerbose) << "Found bootstrap file at " << local_file;
    impl_->bootstrap_file_path_ = local_file;
    impl_->bootstrap_nodes_ = ReadBootstrapFile(impl_->bootstrap_file_path_);
    return true;
  }

  if (!fs::exists(path, exists_error_code) ||
      !fs::is_regular_file(path, is_regular_file_error_code) ||
      exists_error_code || is_regular_file_error_code) {
    if (exists_error_code) {
      LOG(kError) << "Failed to check for bootstrap file at " << path << ".  "
                  << exists_error_code.message();
    }
    if (is_regular_file_error_code) {
      LOG(kError) << "Failed to check for bootstrap file at " << path << ".  "
                  << is_regular_file_error_code.message();
    }
    return false;
  } else {
    LOG(kVerbose) << "Found bootstrap file at " << path;
    impl_->bootstrap_file_path_ = path;
    impl_->bootstrap_nodes_ = ReadBootstrapFile(impl_->bootstrap_file_path_);
    return true;
  }
}

void Routing::Join(Functors functors, Endpoint peer_endpoint) {
  if (!peer_endpoint.address().is_unspecified()) {
    return BootstrapFromThisEndpoint(functors, peer_endpoint);
  } else {
    LOG(kInfo) << "Doing a default join";
    if (CheckBootstrapFilePath()) {
      return DoJoin(functors);
    } else {
      LOG(kError) << "Invalid Bootstrap Contacts";
      if (functors.network_status)
        functors.network_status(kInvalidBootstrapContacts);
      return;
    }
  }
}

void Routing::ConnectFunctors(const Functors& functors) {
  impl_->routing_table_.set_network_status_functor(functors.network_status);
  impl_->routing_table_.set_close_node_replaced_functor(functors.close_node_replaced);
  impl_->message_handler_->set_message_received_functor(functors.message_received);
  impl_->message_handler_->set_request_public_key_functor(functors.request_public_key);
  impl_->functors_ = functors;
}

void Routing::DisconnectFunctors() {  // TODO(Prakash) : fix race condition when functors in use
  impl_->routing_table_.set_network_status_functor(nullptr);
  impl_->routing_table_.set_close_node_replaced_functor(nullptr);
  impl_->message_handler_->set_message_received_functor(nullptr);
  impl_->message_handler_->set_request_public_key_functor(nullptr);
  impl_->functors_ = Functors();
}

void Routing::BootstrapFromThisEndpoint(const Functors& functors, const Endpoint& endpoint) {
  LOG(kInfo) << "Doing a BootstrapFromThisEndpoint Join.  Entered bootstrap endpoint: " << endpoint
             << ", this node's ID: " << HexSubstr(impl_->keys_.identity)
             << (impl_->client_mode_ ? " Client" : "");
  if (impl_->routing_table_.Size() > 0) {
    DisconnectFunctors();  // TODO(Prakash): Do we need this ?
    for (uint16_t i = 0; i < impl_->routing_table_.Size(); ++i) {
      NodeInfo remove_node =
          impl_->routing_table_.GetClosestNode(NodeId(impl_->routing_table_.kKeys().identity));
      impl_->network_.Remove(remove_node.endpoint);
      impl_->routing_table_.DropNode(remove_node.endpoint);
    }
    if (impl_->functors_.network_status)
      impl_->functors_.network_status(impl_->routing_table_.Size());
  }
  impl_->bootstrap_nodes_.clear();
  impl_->bootstrap_nodes_.push_back(endpoint);
  DoJoin(functors);
}

void Routing::DoJoin(const Functors& functors) {
  int return_value(DoBootstrap(functors));
  if (kSuccess != return_value) {
    if (functors.network_status)
      functors.network_status(return_value);
    return;
  }

  if (impl_->anonymous_node_) {  // No need to do find value for anonymous node
    if (functors.network_status)
      functors.network_status(return_value);
    return;
  }

  if (functors.network_status)
    functors.network_status(DoFindNode());
}

int Routing::DoBootstrap(const Functors& functors) {
  if (impl_->bootstrap_nodes_.empty()) {
    LOG(kError) << "No bootstrap nodes.  Aborted join.";
    return kInvalidBootstrapContacts;
  }
  ConnectFunctors(functors);
  return impl_->network_.Bootstrap(
      impl_->bootstrap_nodes_,
      [this](const std::string& message) { ReceiveMessage(message); },
      [this](const Endpoint& lost_endpoint) { ConnectionLost(lost_endpoint); });  // NOLINT (Fraser)
}

int Routing::DoFindNode() {
  protobuf::Message find_node_rpc(
      rpcs::FindNodes(NodeId(impl_->keys_.identity),
                      NodeId(impl_->keys_.identity),
                      true,
                      impl_->network_.this_node_relay_endpoint()));

  boost::promise<bool> message_sent_promise;
  auto message_sent_future = message_sent_promise.get_future();
  rudp::MessageSentFunctor message_sent_functor(
      [&message_sent_promise](int message_sent) {
        message_sent_promise.set_value(rudp::kSuccess == message_sent);
      });

  impl_->network_.SendToDirectEndpoint(find_node_rpc, impl_->network_.bootstrap_endpoint(),
                                       message_sent_functor);

  if (!message_sent_future.timed_wait(boost::posix_time::seconds(10))) {
    LOG(kError) << "Unable to send FindNode RPC to bootstrap endpoint "
                << impl_->network_.bootstrap_endpoint();
    return kFailedtoSendFindNode;
  } else {
    LOG(kInfo) << "Successfully sent FindNode RPC to bootstrap endpoint "
               << impl_->network_.bootstrap_endpoint();
    return kSuccess;
  }
}

int Routing::ZeroStateJoin(Functors functors, const Endpoint& local_endpoint,
                           const NodeInfo& peer_node) {
  assert((!impl_->client_mode_) && "no client nodes allowed in zero state network");
  assert((!impl_->anonymous_node_) && "not allwed on anonymous node");
  impl_->bootstrap_nodes_.clear();
  impl_->bootstrap_nodes_.push_back(peer_node.endpoint);
  if (impl_->bootstrap_nodes_.empty()) {
    LOG(kError) << "No bootstrap nodes.  Aborted join.";
    return kInvalidBootstrapContacts;
  }

  ConnectFunctors(functors);
  int result(impl_->network_.Bootstrap(
      impl_->bootstrap_nodes_,
      [this](const std::string& message) { ReceiveMessage(message); },
      [this](const Endpoint& lost_endpoint) { ConnectionLost(lost_endpoint); },
      local_endpoint));

  if (result != kSuccess) {
    LOG(kError) << "Could not bootstrap zero state node with " << peer_node.endpoint;
    return result;
  }

  assert((impl_->network_.bootstrap_endpoint() == peer_node.endpoint) &&
         "This should be only used in zero state network");
  LOG(kVerbose) << local_endpoint << " Bootstrapped with remote endpoint " << peer_node.endpoint;

  rudp::EndpointPair peer_endpoint_pair;  // zero state nodes must be directly connected endpoint
  rudp::EndpointPair this_endpoint_pair;
  peer_endpoint_pair.external = peer_endpoint_pair.local = peer_node.endpoint;
  this_endpoint_pair.external = this_endpoint_pair.local = local_endpoint;

  ValidatePeer(impl_->network_,
               impl_->routing_table_,
               impl_->non_routing_table_,
               NodeId(peer_node.node_id),
               peer_node.public_key,
               peer_endpoint_pair,
               this_endpoint_pair,
               false);

  // Now poll for routing table size to have at other node available
  uint8_t poll_count(0);
  do {
    Sleep(boost::posix_time::milliseconds(100));
  } while ((impl_->routing_table_.Size() == 0) && (++poll_count < 50));
  if (impl_->routing_table_.Size() != 0) {
    LOG(kInfo) << "Node Successfully joined zero state network, with "
               << impl_->network_.bootstrap_endpoint()
               << ", Routing table size - " << impl_->routing_table_.Size()
               << ", Node id : " << HexSubstr(impl_->keys_.identity);
    return kSuccess;
  } else {
    LOG(kError) << "Failed to join zero state network, with bootstrap_endpoint "
                << impl_->network_.bootstrap_endpoint();
    return kNotJoined;
  }
}

int Routing::GetStatus() const {
  if (impl_->routing_table_.Size() == 0) {
    rudp::EndpointPair endpoint;
    int status = impl_->network_.GetAvailableEndpoint(Endpoint(), endpoint);
    if (rudp::kSuccess != status) {
      if (status == rudp::kNotBootstrapped)
        return kNotJoined;
    }
  } else {
    return impl_->routing_table_.Size();
  }
  return kSuccess;
}

void Routing::Send(const NodeId& destination_id,
                   const NodeId& /*group_id*/,
                   const std::string& data,
                   const int32_t& type,
                   ResponseFunctor response_functor,
                   const boost::posix_time::time_duration& timeout,
                   const ConnectType& connect_type) {
  if (destination_id.String().empty()) {
    LOG(kError) << "No destination ID, aborted send";
    if (response_functor)
      response_functor(kInvalidDestinationId, std::vector<std::string>());
    return;
  }

  if (data.size() > Parameters::max_data_size) {
    LOG(kError) << "Data size not allowed";
    if (response_functor)
      response_functor(kDataSizeNotAllowed, std::vector<std::string>());
    return;
  }

  if (data.empty() && (type != 100)) {
    LOG(kError) << "No data, aborted send";
    if (response_functor)
      response_functor(kEmptyData, std::vector<std::string>());
    return;
  }

  if (100 >= type) {
    LOG(kError) << "Type below 101 not allowed, aborted send";
    if (response_functor)
      response_functor(kTypeNotAllowed, std::vector<std::string>());
    return;
  }

  protobuf::Message proto_message;
  if (response_functor)
    proto_message.set_id(impl_->timer_.AddTask(timeout, response_functor));
  proto_message.set_destination_id(destination_id.String());
  proto_message.add_data(data);
  proto_message.set_direct(static_cast<int32_t>(connect_type));
  proto_message.set_type(type);
  proto_message.set_client_node(impl_->client_mode_);

  // Anonymous node
  if (impl_->anonymous_node_) {
    proto_message.set_relay_id(impl_->routing_table_.kKeys().identity);
    SetProtobufEndpoint(impl_->network_.this_node_relay_endpoint(), proto_message.mutable_relay());
    Endpoint bootstrap_endpoint = impl_->network_.bootstrap_endpoint();
    rudp::MessageSentFunctor message_sent(
        [&](int result) {
          if (rudp::kSuccess != result) {
            impl_->timer_.CancelTask(proto_message.id());
            LOG(kError) << "Anonymous Session Ended, Send not allowed anymore";
          } else {
            LOG(kInfo) << "Message Sent from Anonymous node";
          }
        });

    impl_->network_.SendToDirectEndpoint(proto_message, bootstrap_endpoint,  message_sent);
    return;
  }

  // Non Anonymous, normal node
  proto_message.set_source_id(impl_->routing_table_.kKeys().identity);
  if (impl_->routing_table_.kKeys().identity != destination_id.String()) {
    impl_->network_.SendToClosestNode(proto_message);
  } else {
    LOG(kInfo) << "Sending request to self";
    ReceiveMessage(proto_message.SerializeAsString());
  }
}

void Routing::ReceiveMessage(const std::string& message) {
  if (impl_->tearing_down_) {
    LOG(kVerbose) << "Ignoring message received since this node is shutting down";
    return;
  }

  protobuf::Message protobuf_message;
  protobuf::ConnectRequest connection_request;
  if (protobuf_message.ParseFromString(message)) {
    bool relay_message(!protobuf_message.has_source_id());
    LOG(kInfo) << "This node [" << HexSubstr(impl_->keys_.identity) << "] received message type: "
               << protobuf_message.type() << " from "
               << (relay_message ? HexSubstr(protobuf_message.relay_id()) + " -- RELAY REQUEST" :
                                   HexSubstr(protobuf_message.source_id()));
    impl_->message_handler_->HandleMessage(protobuf_message);
  } else {
    LOG(kWarning) << "Message received, failed to parse";
  }
}

void Routing::ConnectionLost(const Endpoint& lost_endpoint) {
  LOG(kWarning) << "Routing::ConnectionLost---------------------------------------------------";
  NodeInfo dropped_node;
  if (!impl_->tearing_down_ &&
      (impl_->routing_table_.GetNodeInfo(lost_endpoint, dropped_node) &&
       impl_->routing_table_.IsThisNodeInRange(dropped_node.node_id,
                                               Parameters::closest_nodes_size))) {
    // Close node lost, get more nodes
    LOG(kWarning) << "Lost close node, getting more.";
    // TODO(Prakash): uncomment once find node flooding is resolved.
    // impl_->network_.SendToClosestNode(rpcs::FindNodes(NodeId(impl_->keys_.identity),
    //                                 NodeId(impl_->keys_.identity)));
  }

  // Checking routing table
  dropped_node = impl_->routing_table_.DropNode(lost_endpoint);
  if (dropped_node.node_id != NodeId()) {
    LOG(kWarning) << "Lost connection with routing node "
                  << HexSubstr(dropped_node.node_id.String()) << ", endpoint " << lost_endpoint;
    LOG(kWarning) << "Routing::ConnectionLost-----------------------------------------Exiting";
    return;
  }

  // Checking non-routing table
  dropped_node = impl_->non_routing_table_.DropNode(lost_endpoint);
  if (dropped_node.node_id != NodeId()) {
    LOG(kWarning) << "Lost connection with non-routing node "
                  << HexSubstr(dropped_node.node_id.String()) << ", endpoint " << lost_endpoint;
  } else {
    LOG(kWarning) << "Lost connection with unknown/internal endpoint " << lost_endpoint;
  }
  LOG(kWarning) << "Routing::ConnectionLost--------------------------------------------Exiting";
}

bool Routing::ConfirmGroupMembers(const NodeId& node1, const NodeId& node2) {
  return impl_->routing_table_.ConfirmGroupMembers(node1, node2);
}

}  // namespace routing

}  // namespace maidsafe
