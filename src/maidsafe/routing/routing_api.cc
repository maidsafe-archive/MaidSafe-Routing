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
                 Functors functors,
                 const bool client_mode)
    : impl_(new RoutingPrivate(keys, functors, client_mode)) {
  CheckBootStrapFilePath();
  if (client_mode) {
    Parameters::max_routing_table_size = Parameters::closest_nodes_size;
  }
  Join();
}

Routing::~Routing() {}

int Routing::GetStatus() {
  if (impl_->routing_table_.Size() == 0) {
    rudp::EndpointPair endpoint;
    if(impl_->rudp_.GetAvailableEndpoint(endpoint) != rudp::kSuccess) {
      if (impl_->rudp_.GetAvailableEndpoint(endpoint) == rudp::kNoneAvailable)
        return kNotJoined;
    }
  } else {
    return impl_->routing_table_.Size();
  }
  return kSuccess;
}

void Routing::CheckBootStrapFilePath() {
  std::string file_name;
  if(impl_->client_mode_) {
    file_name = "bootstrap";  // TODO (FIXME)
    impl_->bootstrap_file_path_ = GetUserAppDir() / file_name;
  } else {
    file_name = "bootstrap." + EncodeToBase32(impl_->keys_.identity);
    impl_->bootstrap_file_path_ = GetSystemAppDir() / file_name;
  }
  // test path
  // not catching exceptions !!
  fs::ifstream file_in(impl_->bootstrap_file_path_, std::ios::binary);
  fs::ofstream file_out(impl_->bootstrap_file_path_, std::ios::binary);
  if (file_in.good()) {
    if (fs::exists(impl_->bootstrap_file_path_)) {
      fs::file_size(impl_->bootstrap_file_path_);  // throws
    } else if (file_out.good()) {
      file_out.put('c');
    fs::file_size(impl_->bootstrap_file_path_);  // throws
    fs::remove(impl_->bootstrap_file_path_);
    } else {
      fs::file_size(impl_->bootstrap_file_path_);  // throws
    }
  } else {
    fs::file_size(impl_->bootstrap_file_path_);  // throws
  }
}

// drop existing routing table and restart
// the endpoint is the endpoint to connect to.
bool Routing::BootStrapFromThisEndpoint(const boost::asio::ip::udp::endpoint &endpoint,
                                        boost::asio::ip::udp::endpoint local_endpoint) {
  LOG(kInfo) << " Entered bootstrap IP address : " << endpoint.address().to_string();
  LOG(kInfo) << " Entered bootstrap Port       : " << endpoint.port();
  if (endpoint.address().is_unspecified()) {
    LOG(kError) << "Attempt to boot from unspecified endpoint ! aborted";
    return false;
  }
  for (unsigned int i = 0; i < impl_->routing_table_.Size(); ++i) {
    NodeInfo remove_node =
    impl_->routing_table_.GetClosestNode(NodeId(impl_->routing_table_.kKeys().identity), 0);
    impl_->rudp_.Remove(remove_node.endpoint);
    impl_->routing_table_.DropNode(remove_node.endpoint);
  }
  if(impl_->functors_.network_status)
    impl_->functors_.network_status(impl_->routing_table_.Size());
  impl_->bootstrap_nodes_.clear();
  impl_->bootstrap_nodes_.push_back(endpoint);
  return Join(local_endpoint);
}

bool Routing::Join(Endpoint local_endpoint) {
  if (impl_->bootstrap_nodes_.empty()) {
    LOG(kInfo) << "No bootstrap nodes Aborted Join !!";
    return false;
  }
  rudp::MessageReceivedFunctor message_recieved(std::bind(&Routing::ReceiveMessage, this,
                                                          args::_1));
  rudp::ConnectionLostFunctor connection_lost(std::bind(&Routing::ConnectionLost, this, args::_1));

  Endpoint bootstrap_endpoint(impl_->rudp_.Bootstrap(impl_->bootstrap_nodes_,
                                                     message_recieved,
                                                     connection_lost,
                                                     local_endpoint));

  if (bootstrap_endpoint.address().is_unspecified() && local_endpoint.address().is_unspecified()) {
    LOG(kError) << "could not get bootstrap address and not zero state";
    return false;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  auto boot = std::async(std::launch::async, [&] {
      rudp::MessageSentFunctor message_sent_functor;  // TODO (FIXME)
      return impl_->rudp_.Send(bootstrap_endpoint, rpcs::FindNodes(
                                 NodeId(impl_->keys_.identity), local_endpoint).SerializeAsString(),
                               message_sent_functor); }); // NOLINT Prakash
  return (boot.get() == 0);
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

void Routing::ValidateThisNode(const std::string &node_id,
                               const asymm::PublicKey &public_key,
                               const Endpoint &their_endpoint,
                               const Endpoint &our_endpoint,
                               const bool &client) {
  NodeInfo node_info;
  // TODO(dirvine) Add Managed Connection  here !!!
  node_info.node_id = NodeId(node_id);
  node_info.public_key = public_key;
  node_info.endpoint = their_endpoint;
  impl_->rudp_.Add(their_endpoint, our_endpoint, node_id);
  if (client) {
    impl_->direct_non_routing_table_connections_.push_back(node_info);
  } else {
    impl_->routing_table_.AddNode(node_info);
    if (impl_->bootstrap_nodes_.size() > 1000) {
    impl_->bootstrap_nodes_.erase(impl_->bootstrap_nodes_.begin());
    }
    impl_->bootstrap_nodes_.push_back(their_endpoint);
    std::error_code error;
    WriteBootstrapFile(impl_->bootstrap_nodes_, impl_->bootstrap_file_path_);
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
  }
}

void Routing::ConnectionLost(const Endpoint &lost_endpoint) {
  NodeInfo node_info;
  if ((impl_->routing_table_.GetNodeInfo(lost_endpoint, &node_info) &&
      (impl_->routing_table_.IsMyNodeInRange(node_info.node_id,
                                             Parameters::closest_nodes_size)))) {
    // close node, get more
    SendOn(rpcs::FindNodes(NodeId(impl_->keys_.identity)), impl_->rudp_, impl_->routing_table_);
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
      SendOn(rpcs::FindNodes(NodeId(impl_->keys_.identity)), impl_->rudp_, impl_->routing_table_);
      return;
    }
  }
}

}  // namespace routing

}  // namespace maidsafe
