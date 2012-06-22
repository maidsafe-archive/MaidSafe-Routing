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

#include <chrono>
#include <future>
#include <thread>
#include <utility>

#include "boost/filesystem/exception.hpp"
#include "boost/filesystem/fstream.hpp"
#include "boost/thread/future.hpp"

#include "maidsafe/common/utils.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/bootstrap_file_handler.h"
#include "maidsafe/routing/message_handler.h"
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

Routing::Routing(const asymm::Keys &keys,
                 const bool client_mode)
    : impl_(new RoutingPrivate(keys, client_mode)) {
  CheckBootStrapFilePath();
  if (client_mode) {
    Parameters::max_routing_table_size = Parameters::closest_nodes_size;
  }
}

Routing::~Routing() {}

int Routing::GetStatus() {
  if (impl_->routing_table_.Size() == 0) {
    rudp::EndpointPair endpoint;
    if(impl_->rudp_.GetAvailableEndpoint(Endpoint(), endpoint) != rudp::kSuccess) {
      if (impl_->rudp_.GetAvailableEndpoint(Endpoint(), endpoint) == rudp::kNoneAvailable)
        return kNotJoined;
    }
  } else {
    return impl_->routing_table_.Size();
  }
  return kSuccess;
}

void Routing::CheckBootStrapFilePath() {
  LOG(kInfo) << "path " << GetUserAppDir();
  LOG(kInfo) << "sys path " << GetSystemAppDir();
  fs::path path;
  std::string file_name;
  boost::system::error_code error_code;
  if(impl_->client_mode_) {  // throw if no bootstrap available
    file_name = "bootstrap";
    path = GetUserAppDir() / file_name;
    if (fs::exists(path) && fs::is_regular_file(path)) {
      LOG(kInfo) << "Found bootstrap file at " << path;
    }
  } else {  // vaults
    file_name = "bootstrap." + EncodeToBase32(impl_->keys_.identity);
    path = GetSystemAppDir() / file_name;
    if (!fs::exists(path, error_code) || !fs::is_regular_file(path, error_code)) {
      LOG(kInfo) << "No bootstrap.id file associated with id found : " << path;
      file_name = "bootstrap";  //  need bootstrap file to copy from
    }
    if (file_name == "bootstrap") {  // find bootstrap file and copy contents bootstrap.id
      path = GetSystemAppDir() / file_name;
      std::string file_content;
      if (fs::exists(path, error_code) && fs::is_regular_file(path, error_code)) {
        LOG(kInfo) << "Will create bootstrap.id file from existing bootstrap file at " << path;
        ReadFile(path, &file_content);
      } else {
        LOG(kInfo) << "Not found bootstrap file at " << path;
      }
      file_name = "bootstrap." + EncodeToBase32(impl_->keys_.identity);
      path = GetSystemAppDir() / file_name;
      // create file and copy contents if available
      LOG(kInfo) << "Trying to create bootstrap.id file at " << path;
      WriteFile(path, file_content);
    }
  }
  impl_->bootstrap_file_path_ = path;
  fs::file_size(impl_->bootstrap_file_path_);  // throws
  impl_->bootstrap_nodes_ = ReadBootstrapFile(impl_->bootstrap_file_path_);
}

int Routing::Join(Functors functors, Endpoint peer_endpoint, Endpoint local_endpoint) {
  if (!local_endpoint.address().is_unspecified()) {  // DoZeroStateJoin
    return DoZeroStateJoin(functors, peer_endpoint, local_endpoint);
  } else if (!peer_endpoint.address().is_unspecified()) {  // BootStrapFromThisEndpoint
    return BootStrapFromThisEndpoint(functors, peer_endpoint);
  } else  {  // Default Join
    LOG(kInfo) << " Doing a default join";
    return DoJoin(functors);
  }
}

void Routing::ConnectFunctors(Functors functors) {
  impl_->routing_table_.set_close_node_replaced_functor(functors.close_node_replaced);
  impl_->message_handler_.set_message_received_functor(functors.message_received);
  impl_->message_handler_.set_node_validation_functor(functors.node_validation);
  impl_->functors_ = functors;
}

void Routing::DisconnectFunctors() {
  impl_->routing_table_.set_close_node_replaced_functor(nullptr);
  impl_->message_handler_.set_message_received_functor(nullptr);
  impl_->message_handler_.set_node_validation_functor(nullptr);
  impl_->functors_ = Functors();
}

// drop existing routing table and restart
// the endpoint is the endpoint to connect to.
int Routing::BootStrapFromThisEndpoint(Functors functors,
                                       const boost::asio::ip::udp::endpoint &endpoint) {
  LOG(kInfo) << " Entered bootstrap IP address : " << endpoint.address().to_string();
  LOG(kInfo) << " Entered bootstrap Port       : " << endpoint.port();
  if (impl_->routing_table_.Size() > 0) {
    DisconnectFunctors();  //TODO(Prakash): Do we need this ?
    for (unsigned int i = 0; i < impl_->routing_table_.Size(); ++i) {
      NodeInfo remove_node =
      impl_->routing_table_.GetClosestNode(NodeId(impl_->routing_table_.kKeys().identity), 0);
      impl_->rudp_.Remove(remove_node.endpoint);
      impl_->routing_table_.DropNode(remove_node.endpoint);
    }
    if(impl_->functors_.network_status)
      impl_->functors_.network_status(impl_->routing_table_.Size());
  }
  impl_->bootstrap_nodes_.clear();
  impl_->bootstrap_nodes_.push_back(endpoint);
  return DoJoin(functors);
}

int Routing::DoJoin(Functors functors) {
  if (impl_->bootstrap_nodes_.empty()) {
    LOG(kInfo) << "No bootstrap nodes Aborted Join !!";
    return kInvalidBootstrapContacts;
  }
  ConnectFunctors(functors);
  rudp::MessageReceivedFunctor message_recieved(std::bind(&Routing::ReceiveMessage, this,
                                                          args::_1));
  rudp::ConnectionLostFunctor connection_lost(std::bind(&Routing::ConnectionLost, this, args::_1));
  Endpoint bootstrap_endpoint(impl_->rudp_.Bootstrap(impl_->bootstrap_nodes_,
                                                     message_recieved,
                                                     connection_lost));

  if (bootstrap_endpoint.address().is_unspecified()) {
    LOG(kError) << "could not bootstrap.";
    return kNoOnlineBootstrapContacts;
  }
  LOG(kVerbose) << "Bootstrap successful, bootstrap node - " << bootstrap_endpoint;
  rudp::EndpointPair endpoint_pair;
  if (kSuccess != impl_->rudp_.GetAvailableEndpoint(bootstrap_endpoint, endpoint_pair)) {
    LOG(kError) << " Failed to get available endpoint for new connections";
    return kGeneralError;
  }
  LOG(kWarning) << " GetAvailableEndpoint for peer - " << bootstrap_endpoint << " my endpoint - " << endpoint_pair.external;
  impl_->message_handler_.set_bootstrap_endpoint(bootstrap_endpoint);
  std::string find_node_rpc(rpcs::FindNodes(NodeId(impl_->keys_.identity), NodeId(),
                                            endpoint_pair.external).SerializeAsString());
  boost::promise<bool> message_sent_promise;
  auto message_sent_future = message_sent_promise.get_future();
  uint8_t attempt_count(0);
  rudp::MessageSentFunctor message_sent_functor = [&](bool message_sent) {
      if (message_sent) {
        message_sent_promise.set_value(true);
      } else if (attempt_count < 3) {
        impl_->rudp_.Send(bootstrap_endpoint, find_node_rpc, message_sent_functor);
      } else {
        message_sent_promise.set_value(false);
      }
    };

  impl_->rudp_.Send(bootstrap_endpoint, find_node_rpc, message_sent_functor);

  if(!message_sent_future.timed_wait(boost::posix_time::seconds(10))) {
    LOG(kError) << "Unable to send find value rpc to bootstrap endpoint - " << bootstrap_endpoint;
    return false;
  }
  // now poll for routing table size to have at least one node available
  uint8_t poll_count(0);
  do {
    Sleep(boost::posix_time::milliseconds(100));
  } while ((impl_->routing_table_.Size() == 0) && (++poll_count < 100));
  if (impl_->routing_table_.Size() != 0) {
    LOG(kInfo) << "Successfully joined network, bootstrap node - " << bootstrap_endpoint;
    return kSuccess;
  } else {
    LOG(kInfo) << "Failed to join network, bootstrap node - " << bootstrap_endpoint;
    return kNotJoined;
  }
}

int Routing::DoZeroStateJoin(Functors functors, Endpoint peer_endpoint, Endpoint local_endpoint) {
  assert((!impl_->client_mode_) && "no client nodes allowed in zero state network");
  impl_->bootstrap_nodes_.clear();
  impl_->bootstrap_nodes_.push_back(peer_endpoint);
  if (impl_->bootstrap_nodes_.empty()) {
    LOG(kInfo) << "No bootstrap nodes Aborted Join !!";
    return kInvalidBootstrapContacts;
  }

  ConnectFunctors(functors);
  rudp::MessageReceivedFunctor message_recieved(std::bind(&Routing::ReceiveMessage, this,
                                                          args::_1));
  rudp::ConnectionLostFunctor connection_lost(std::bind(&Routing::ConnectionLost, this, args::_1));

  Endpoint bootstrap_endpoint(impl_->rudp_.Bootstrap(impl_->bootstrap_nodes_,
                                                     message_recieved,
                                                     connection_lost,
                                                     local_endpoint));

  if (bootstrap_endpoint.address().is_unspecified() && local_endpoint.address().is_unspecified()) {
    LOG(kError) << "could not bootstrap zero state node with " << bootstrap_endpoint;
    return kNoOnlineBootstrapContacts;
  }
  assert((bootstrap_endpoint == peer_endpoint) && "This should be only used in zero state network");
  LOG(kVerbose) << local_endpoint << " Bootstraped with remote endpoint " << bootstrap_endpoint;
  impl_->message_handler_.set_bootstrap_endpoint(bootstrap_endpoint);
  rudp::EndpointPair their_endpoint_pair;  //  zero state nodes must be directly connected endpoint
  rudp::EndpointPair our_endpoint_pair;
  their_endpoint_pair.external = their_endpoint_pair.local = bootstrap_endpoint;
  our_endpoint_pair.external = our_endpoint_pair.local = local_endpoint;

  if (impl_->functors_.node_validation)
    impl_->functors_.node_validation(NodeId(), their_endpoint_pair, our_endpoint_pair, false);

  // now poll for routing table size to have at least one node available
  uint8_t poll_count(0);
  do {
    Sleep(boost::posix_time::milliseconds(100));
  } while ((impl_->routing_table_.Size() == 0) && (++poll_count < 50));
  if (impl_->routing_table_.Size() != 0) {
    LOG(kInfo) << "Successfully joined zero state network, with " << bootstrap_endpoint;
    return kSuccess;
  } else {
    LOG(kInfo) << "Failed to join zero state network, with bootstrap_endpoint"
               << bootstrap_endpoint;
    return kNotJoined;
  }
}

SendStatus Routing::Send(const NodeId &destination_id,
                         const NodeId &/*group_id*/,
                         const std::string &data,
                         const int32_t &type,
                         const ResponseFunctor response_functor,
                         const int16_t &/*timeout_seconds*/,
                         const ConnectType &connect_type) {
  if (destination_id.String().empty()) {
    LOG(kError) << "No destination id, aborted send";
    return SendStatus::kInvalidDestinationId;
  }
  if (data.empty() && (type != 100)) {
    LOG(kError) << "No data, aborted send";
    return SendStatus::kEmptyData;
  }
  protobuf::Message proto_message;
  proto_message.set_id(0);
  // TODO(dirvine): see if ANONYMOUS and Endpoint required here
  proto_message.set_source_id(impl_->routing_table_.kKeys().identity);
  proto_message.set_destination_id(destination_id.String());
  proto_message.set_data(data);
  proto_message.set_direct(static_cast<int32_t>(connect_type));
  proto_message.set_type(type);
  SendOn(proto_message, impl_->rudp_, impl_->routing_table_);
  return SendStatus::kSuccess;
}

void Routing::ValidateThisNode(const NodeId& node_id,
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
  int result = impl_->rudp_.Add(our_endpoint.external, their_endpoint.external, node_id.String());

  if (result != kSuccess) {
      LOG(kWarning) << "rudp add failed " << result;
    return;
  }
  LOG(kVerbose) << "rudp_.Add result = " << result;
  if (client) {
    impl_->direct_non_routing_table_connections_.push_back(node_info);
  } else {
    if(impl_->routing_table_.AddNode(node_info)) {
      LOG(kVerbose) << "Added node to routing table. node id " << HexSubstr(node_id.String());
      if (impl_->bootstrap_nodes_.size() > 1000)
        impl_->bootstrap_nodes_.erase(impl_->bootstrap_nodes_.begin());
      impl_->bootstrap_nodes_.push_back(their_endpoint.external);
      WriteBootstrapFile(impl_->bootstrap_nodes_, impl_->bootstrap_file_path_);
    } else {
      LOG(kVerbose) << "Add node to routing table failed. node id "
                    << HexSubstr(node_id.String())
                    << " just added rudp connection will be removed now";
      impl_->rudp_.Remove(their_endpoint.external);
    }
  }
}

void Routing::ReceiveMessage(const std::string &message) {
  protobuf::Message protobuf_message;
  protobuf::ConnectRequest connection_request;
  if (protobuf_message.ParseFromString(message)) {
    LOG(kInfo) << " Message received, type: " << protobuf_message.type()
               << " from " << HexSubstr(protobuf_message.source_id())
               << " I am " << HexSubstr(impl_->keys_.identity);
    impl_->message_handler_.ProcessMessage(protobuf_message);
  } else {
    LOG(kVerbose) << " Message received, failed to parse";
  }
}

void Routing::ConnectionLost(const Endpoint &lost_endpoint) {
  NodeInfo node_info;
  if ((impl_->routing_table_.GetNodeInfo(lost_endpoint, &node_info) &&
      (impl_->routing_table_.IsMyNodeInRange(node_info.node_id,
                                             Parameters::closest_nodes_size)))) {
    // close node, get more
    SendOn(rpcs::FindNodes(NodeId(impl_->keys_.identity),
                           NodeId(impl_->keys_.identity)),
                           impl_->rudp_, impl_->routing_table_);
  }
  if (!impl_->routing_table_.DropNode(lost_endpoint))
    return;
  for (auto it = impl_->direct_non_routing_table_connections_.begin();
        it != impl_->direct_non_routing_table_connections_.end(); ++it) {
    if ((*it).endpoint ==  lost_endpoint) {
      impl_->direct_non_routing_table_connections_.erase(it);
      return;
    }
  }
  for (auto it = impl_->direct_non_routing_table_connections_.begin();
        it != impl_->direct_non_routing_table_connections_.end(); ++it) {
    if ((*it).endpoint ==  lost_endpoint) {
      impl_->direct_non_routing_table_connections_.erase(it);
      // close node, get more
      SendOn(rpcs::FindNodes(NodeId(impl_->keys_.identity),
                             NodeId(impl_->keys_.identity)),
                             impl_->rudp_, impl_->routing_table_);
      return;
    }
  }
}

}  // namespace routing

}  // namespace maidsafe
