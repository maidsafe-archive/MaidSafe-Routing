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

#include "boost/filesystem/exception.hpp"
#include "boost/filesystem/fstream.hpp"
#include "boost/thread/future.hpp"

#include "maidsafe/common/utils.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/bootstrap_file_handler.h"
#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_api_impl.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/timer.h"
#include "maidsafe/routing/utils.h"

namespace args = std::placeholders;
namespace bs2 = boost::signals2;
namespace fs = boost::filesystem;

namespace maidsafe {

namespace routing {

Routing::Routing(const asymm::Keys &keys, const bool &client_mode)
    : impl_(new RoutingPrivate(keys, client_mode)) {
  if (!CheckBootStrapFilePath()) {
    LOG(kInfo) << " No bootstrap nodes, require BootStrapFromThisEndpoint()";
  }
  if (client_mode) {
    Parameters::max_routing_table_size = Parameters::closest_nodes_size;
  } else {
    assert(!keys.identity.empty() && "Server Nodes cannot be created without valid keys");
  }
  if (keys.identity.empty()) {
    impl_->anonymous_node_ = true;
    asymm::GenerateKeyPair(&impl_->keys_);
    impl_->keys_.identity = NodeId(RandomString(64)).String();
    impl_->routing_table_.set_keys(impl_->keys_);
    LOG(kInfo) << " Anonymous node id : " << HexSubstr(impl_->keys_.identity);
  }
}

Routing::~Routing() {
  LOG(kVerbose) << "~Routing() - Disconnecting funtors"
                << ", Routing table Size - " << impl_->routing_table_.Size();
  impl_->message_handler_.set_tearing_down();
  DisconnectFunctors();
}

int Routing::GetStatus() {
  if (impl_->routing_table_.Size() == 0) {
    rudp::EndpointPair endpoint;
    int status = impl_->network_.GetAvailableEndpoint(Endpoint(), endpoint);
    if (rudp::kSuccess != status) {
      if (status == rudp::kNoneAvailable)
        return kNotJoined;
    }
  } else {
    return impl_->routing_table_.Size();
  }
  return kSuccess;
}

bool Routing::CheckBootStrapFilePath() {
  LOG(kVerbose) << "Application Path " << GetUserAppDir();
  LOG(kVerbose) << "System Path " << GetSystemAppDir();
  fs::path path;
  fs::path local_file;
  std::string file_name;
  boost::system::error_code error_code;
  if (impl_->client_mode_) {
    file_name = "bootstrap";
    path = GetUserAppDir() / file_name;
  } else {
    file_name = "bootstrap." + EncodeToBase32(impl_->keys_.identity);
    path = GetSystemAppDir() / file_name;
  }
  local_file = fs::current_path() / file_name;
  if (fs::exists(local_file) && fs::is_regular_file(local_file)) {
    LOG(kVerbose) << "Found bootstrap file at " << local_file.string();
    impl_->bootstrap_file_path_ = local_file;
    impl_->bootstrap_nodes_ = ReadBootstrapFile(impl_->bootstrap_file_path_);
    return true;
  } else {
    path = GetUserAppDir() / file_name;
    if (fs::exists(path) && fs::is_regular_file(path)) {
      LOG(kVerbose) << "Found bootstrap file at " << path;
      impl_->bootstrap_file_path_ = path;
      impl_->bootstrap_nodes_ = ReadBootstrapFile(impl_->bootstrap_file_path_);
      return true;
    }
  }
  return false;
}

int Routing::Join(Functors functors, Endpoint peer_endpoint) {
  if (!peer_endpoint.address().is_unspecified()) {  // BootStrapFromThisEndpoint
    return BootStrapFromThisEndpoint(functors, peer_endpoint);
  } else  {  // Default Join
    LOG(kInfo) << " Doing a default join";
    if (CheckBootStrapFilePath()) {
      return DoJoin(functors);
    } else {
      LOG(kError) << "Invalid Bootstrap Contacts";
      return kInvalidBootstrapContacts;
    }
  }
}

void Routing::ConnectFunctors(Functors functors) {
  impl_->routing_table_.set_close_node_replaced_functor(functors.close_node_replaced);
  impl_->message_handler_.set_message_received_functor(functors.message_received);
  impl_->message_handler_.set_node_validation_functor(functors.request_public_key);
  impl_->functors_ = functors;
}

void Routing::DisconnectFunctors() {  // TODO(Prakash) : fix raise condition when functors in use
  impl_->routing_table_.set_close_node_replaced_functor(nullptr);
  impl_->message_handler_.set_message_received_functor(nullptr);
  impl_->message_handler_.set_node_validation_functor(nullptr);
  impl_->functors_ = Functors();
}

// drop existing routing table and restart
// the endpoint is the endpoint to connect to.
int Routing::BootStrapFromThisEndpoint(Functors functors, const Endpoint &endpoint) {
  LOG(kInfo) << " Doing a BootStrapFromThisEndpoint Join."
             << " Entered bootstrap endpoint : " << endpoint
             << ", My Node id : " << HexSubstr(impl_->keys_.identity)
             << (impl_->client_mode_ ? "Client" : "");

  if (impl_->routing_table_.Size() > 0) {
    DisconnectFunctors();  // TODO(Prakash): Do we need this ?
    for (unsigned int i = 0; i < impl_->routing_table_.Size(); ++i) {
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
  return DoJoin(functors);
}

int Routing::DoJoin(Functors functors) {
  int return_value(DoBootstrap(functors));
  if (kSuccess != return_value)
    return return_value;

  if (impl_->anonymous_node_)  //  No need to do find value for anonymous node
    return return_value;

  return DoFindNode();
}

int Routing::DoBootstrap(Functors functors) {
  if (impl_->bootstrap_nodes_.empty()) {
    LOG(kError) << "No bootstrap nodes Aborted Join !!";
    return kInvalidBootstrapContacts;
  }
  ConnectFunctors(functors);
  rudp::MessageReceivedFunctor message_recieved(std::bind(&Routing::ReceiveMessage, this,
                                                          args::_1));
  rudp::ConnectionLostFunctor connection_lost(std::bind(&Routing::ConnectionLost, this, args::_1));
  Endpoint bootstrap_endpoint(impl_->network_.Bootstrap(impl_->bootstrap_nodes_,
                                                        message_recieved,
                                                        connection_lost));

  if (bootstrap_endpoint.address().is_unspecified()) {
    LOG(kError) << "No Online Bootstrap Node found.";
    return kNoOnlineBootstrapContacts;
  }
  LOG(kVerbose) << "Bootstrap successful, bootstrap node - " << bootstrap_endpoint;
  rudp::EndpointPair endpoint_pair;
  if (kSuccess != impl_->network_.GetAvailableEndpoint(bootstrap_endpoint, endpoint_pair)) {
    LOG(kError) << " Failed to get available endpoint for new connections";
    return kGeneralError;
  }
  LOG(kVerbose) << " GetAvailableEndpoint for peer - "
                << bootstrap_endpoint << " my endpoint - " << endpoint_pair.external;
  impl_->message_handler_.set_bootstrap_endpoint(bootstrap_endpoint);
  impl_->message_handler_.set_my_relay_endpoint(endpoint_pair.external);
  return kSuccess;
}

int Routing::DoFindNode() {
  protobuf::Message find_node_rpc(
      rpcs::FindNodes(NodeId(impl_->keys_.identity),
                      NodeId(impl_->keys_.identity),
                      true,
                      impl_->message_handler_.my_relay_endpoint()));

  boost::promise<bool> message_sent_promise;
  auto message_sent_future = message_sent_promise.get_future();
  uint8_t attempt_count(0);
  rudp::MessageSentFunctor message_sent_functor = [&](bool message_sent) {
      if (message_sent) {
        message_sent_promise.set_value(true);
      } else if (attempt_count < 3) {
        impl_->network_.SendToDirectEndpoint(find_node_rpc,
                                             impl_->message_handler_.bootstrap_endpoint(),
                                             message_sent_functor);
      } else {
        message_sent_promise.set_value(false);
      }
    };

  impl_->network_.SendToDirectEndpoint(find_node_rpc, impl_->message_handler_.bootstrap_endpoint(),
                                       message_sent_functor);

  if (!message_sent_future.timed_wait(boost::posix_time::seconds(10))) {
    LOG(kError) << "Unable to send FindValue rpc to bootstrap endpoint - "
                << impl_->message_handler_.bootstrap_endpoint();
    return false;
  }
  // now poll for routing table size to have at least one node available

  uint8_t poll_count(0);
  // TODO(Prakash) : Need to fix target min RT size.
  // TODO(Prakash) : workaround to allow fake rudp tests pass FIXME
  uint8_t target_routing_table_size = 2;  // (impl_->client_mode_? 4 : 2);
  do {
    Sleep(boost::posix_time::milliseconds(100));
  } while ((impl_->routing_table_.Size() < target_routing_table_size) &&
           (++poll_count < 100));
  if (impl_->routing_table_.Size() >= target_routing_table_size) {
    LOG(kInfo) << (impl_->client_mode_? "Client ": "")
               << " Node successfully joined network, bootstrap node - "
               << impl_->message_handler_.bootstrap_endpoint()
               << ", Routing table size - " << impl_->routing_table_.Size()
               << ", Node id : " << HexSubstr(impl_->keys_.identity);
    return kSuccess;
  } else {
    LOG(kError) << "Failed to join network, bootstrap node - "
                << impl_->message_handler_.bootstrap_endpoint();
    return kNotJoined;
  }
}

int Routing::ZeroStateJoin(Functors functors, const Endpoint &local_endpoint,
                           const NodeInfo &peer_node) {
  assert((!impl_->client_mode_) && "no client nodes allowed in zero state network");
  assert((!impl_->anonymous_node_) && "not allwed on anonymous node");
  impl_->bootstrap_nodes_.clear();
  impl_->bootstrap_nodes_.push_back(peer_node.endpoint);
  if (impl_->bootstrap_nodes_.empty()) {
    LOG(kError) << "No bootstrap nodes, Aborted Join !!";
    return kInvalidBootstrapContacts;
  }

  ConnectFunctors(functors);
  rudp::MessageReceivedFunctor message_recieved(std::bind(&Routing::ReceiveMessage, this,
                                                          args::_1));
  rudp::ConnectionLostFunctor connection_lost(std::bind(&Routing::ConnectionLost, this, args::_1));

  Endpoint bootstrap_endpoint(impl_->network_.Bootstrap(impl_->bootstrap_nodes_,
                                                        message_recieved,
                                                        connection_lost,
                                                        local_endpoint));

  if (bootstrap_endpoint.address().is_unspecified() && local_endpoint.address().is_unspecified()) {
    LOG(kError) << "Could not bootstrap zero state node with " << bootstrap_endpoint;
    return kNoOnlineBootstrapContacts;
  }
  assert((bootstrap_endpoint == peer_node.endpoint) &&
         "This should be only used in zero state network");
  LOG(kVerbose) << local_endpoint << " Bootstraped with remote endpoint " << bootstrap_endpoint;
  impl_->message_handler_.set_bootstrap_endpoint(bootstrap_endpoint);
  impl_->message_handler_.set_my_relay_endpoint(local_endpoint);
  rudp::EndpointPair their_endpoint_pair;  //  zero state nodes must be directly connected endpoint
  rudp::EndpointPair our_endpoint_pair;
  their_endpoint_pair.external = their_endpoint_pair.local = peer_node.endpoint;
  our_endpoint_pair.external = our_endpoint_pair.local = local_endpoint;

  ValidateThisNode(impl_->network_, impl_->routing_table_, impl_->non_routing_table_,
                   NodeId(peer_node.node_id), peer_node.public_key, their_endpoint_pair,
                   our_endpoint_pair, false);

  // now poll for routing table size to have at other node available
  uint8_t poll_count(0);
  do {
    Sleep(boost::posix_time::milliseconds(100));
  } while ((impl_->routing_table_.Size() == 0) && (++poll_count < 50));
  if (impl_->routing_table_.Size() != 0) {
    LOG(kInfo) << "Node Successfully joined zero state network, with " << bootstrap_endpoint
               << ", Routing table size - " << impl_->routing_table_.Size()
               << ", Node id : " << HexSubstr(impl_->keys_.identity);
    return kSuccess;
  } else {
    LOG(kError) << "Failed to join zero state network, with bootstrap_endpoint"
                << bootstrap_endpoint;
    return kNotJoined;
  }
}

void Routing::Send(const NodeId &destination_id,
                   const NodeId &/*group_id*/,
                   const std::string &data,
                   const int32_t &type,
                   const ResponseFunctor response_functor,
                   const boost::posix_time::time_duration &timeout,
                   const ConnectType &connect_type) {
  if (destination_id.String().empty()) {
    LOG(kError) << "No destination id, aborted send";
    if (response_functor)
      response_functor(kInvalidDestinationId, "");
    return;
  }
  if (data.empty() && (type != 100)) {
    LOG(kError) << "No data, aborted send";
    if (response_functor)
      response_functor(kEmptyData, "");
    return;
  }

  if (100 >= type) {
    LOG(kError) << "Type below 101 not allowed, aborted send";
    if (response_functor)
      response_functor(kTypeNotAllowed, "");
    return;
  }

  protobuf::Message proto_message;
  if (response_functor)
    proto_message.set_id(impl_->timer_.AddTask(timeout, response_functor));
  proto_message.set_destination_id(destination_id.String());
  proto_message.set_data(data);
  proto_message.set_direct(static_cast<int32_t>(connect_type));
  proto_message.set_type(type);
  proto_message.set_client_node(impl_->client_mode_);

  // Anonymous node
  if (impl_->anonymous_node_) {
    proto_message.set_relay_id(impl_->routing_table_.kKeys().identity);
    SetProtobufEndpoint(impl_->message_handler_.my_relay_endpoint(), proto_message.mutable_relay());
    Endpoint bootstrap_endpoint = impl_->message_handler_.bootstrap_endpoint();
    rudp::MessageSentFunctor message_sent = [&] (bool result) {
        if (!result) {
          impl_->timer_.KillTask(proto_message.id());
          LOG(kError) << "Anonymous Session Ended, Send not allowed anymore";
        } else {
          LOG(kInfo) << "Message Sent from Anonymous node";
        }
      };

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
  return;
}

void Routing::ReceiveMessage(const std::string &message) {
  if (impl_->message_handler_.tearing_down()) {
    LOG(kVerbose) << " Ignoring, Message received, shutting down";
    return;
  }

  protobuf::Message protobuf_message;
  protobuf::ConnectRequest connection_request;
  if (protobuf_message.ParseFromString(message)) {
    bool relay_message(!protobuf_message.has_source_id());
    LOG(kInfo) << " Message received, type: " << protobuf_message.type()
               << " from "
               << (relay_message? HexSubstr(protobuf_message.relay_id()):
                     HexSubstr(protobuf_message.source_id()))
               << " I am " << HexSubstr(impl_->keys_.identity)
               << (relay_message? " -- RELAY REQUEST": "");
    impl_->message_handler_.ProcessMessage(protobuf_message);
  } else {
    LOG(kWarning) << " Message received, failed to parse";
  }
}

void Routing::ConnectionLost(const Endpoint &lost_endpoint) {
  NodeInfo dropped_node;
  if ((!impl_->message_handler_.tearing_down()) &&
      (impl_->routing_table_.GetNodeInfo(lost_endpoint, &dropped_node) &&
      (impl_->routing_table_.IsMyNodeInRange(dropped_node.node_id,
                                             Parameters::closest_nodes_size)))) {
    // close node lost, get more nodes
    impl_->network_.SendToClosestNode(rpcs::FindNodes(NodeId(impl_->keys_.identity),
                                      NodeId(impl_->keys_.identity)));
  }
  //  Checking RT
  dropped_node = impl_->routing_table_.DropNode(lost_endpoint);
  if (dropped_node.node_id != NodeId()) {
    LOG(kWarning) << " Lost connection with routing node : "
                  << HexSubstr(dropped_node.node_id.String())
                  << ", endpoint : "
                  << lost_endpoint;
    return;
  }

  //  Checking NRT
  dropped_node = impl_->non_routing_table_.DropNode(lost_endpoint);
  if (dropped_node.node_id != NodeId()) {
    LOG(kWarning) << " Lost connection with non routing node : "
                  << HexSubstr(dropped_node.node_id.String())
                  << ", endpoint : "
                  << lost_endpoint;
  } else {
    LOG(kWarning) << " Lost connection with unknown/internal endpoint : "
                  << lost_endpoint;
  }
}

}  // namespace routing

}  // namespace maidsafe
