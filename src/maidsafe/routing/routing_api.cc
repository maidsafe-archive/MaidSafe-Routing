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

#include <utility>
#include "boost/filesystem/fstream.hpp"
#include "boost/filesystem/exception.hpp"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/timer.h"
#include "maidsafe/routing/version.h"
#include "maidsafe/routing/bootstrap_file_handler.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/utils.h"
#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/routing_api_impl.h"

namespace fs = boost::filesystem;
namespace bs2 = boost::signals2;

namespace maidsafe {

namespace routing {

int8_t GetMajorVersion() {
  return MAIDSAFE_ROUTING_VERSION;
}

int8_t GetMinorVersion() {
  return MAIDSAFE_ROUTING_VERSION; /*_MINOR; TODO(dirvine)*/
}

int8_t GetPatchVersion() {
  return MAIDSAFE_ROUTING_VERSION; /*_PATCH; TODO(dirvine) */
}
  
Message::Message()
    : type(0),
      destination_id(),
      data(),
      timeout(Parameters::timout_in_seconds),
      direct(false),
      replication(1) {}

Message::Message(const protobuf::Message &protobuf_message)
    : type(protobuf_message.type()),
      destination_id(protobuf_message.destination_id()),
      data(protobuf_message.data()),
      timeout(Parameters::timout_in_seconds),
      direct(protobuf_message.direct()),
      replication(protobuf_message.replication()) {}


Routing::Routing(const asymm::Keys &keys,
                 const boost::filesystem::path &boostrap_file_path,
                 bool client_mode)
    : impl_(new RoutingPrivate(keys, boostrap_file_path, client_mode)) {
  // test path
  std::string dummy_content;
  // not catching exceptions !!
  fs::ifstream file_in(boostrap_file_path, std::ios::in | std::ios::binary);
  fs::ofstream file_out(boostrap_file_path, std::ios::out | std::ios::binary);
  if(file_in.good()) {
    if (fs::exists(boostrap_file_path)) {
      fs::file_size(boostrap_file_path);  // throws
    } else if (file_out.good()) {
      file_out.put('c');
    fs::file_size(boostrap_file_path);  // throws
    fs::remove(boostrap_file_path);
    } else {
      fs::file_size(boostrap_file_path);  // throws
    }
  } else {
    fs::file_size(boostrap_file_path);  // throws
  }
  Init();
}

Routing::~Routing() { }


// drop existing routing table and restart
void Routing::BootStrapFromThisEndpoint(const boost::asio::ip::udp::endpoint
&endpoint) {
  LOG(INFO) << " Entered bootstrap IP address : " << endpoint.address().to_string();
  LOG(INFO) << " Entered bootstrap Port       : " << endpoint.port();
  for (unsigned int i = 0; i < impl_->routing_table_.Size(); ++i) {
    NodeInfo remove_node =
    impl_->routing_table_.GetClosestNode(NodeId(impl_->routing_table_.kKeys().identity), 0);
    impl_->rudp_.Remove(remove_node.endpoint);
    impl_->routing_table_.DropNode(remove_node.endpoint);
  }
  impl_->network_status_signal_(impl_->routing_table_.Size());
  impl_->bootstrap_nodes_.clear();
  impl_->bootstrap_nodes_.push_back(endpoint);
  impl_->asio_service_.service().post(std::bind(&Routing::Join, this));
}

int Routing::Send(const Message &message,
                   const MessageReceivedFunctor response_functor) {
  if (message.destination_id.empty()) {
    DLOG(ERROR) << "No destination id, aborted send";
    return kInvalidDestinatinId;
  }
  if (message.data.empty() && (message.type != 100)) {
    DLOG(ERROR) << "No data, aborted send";
    return kEmptyData;
  }
  if (message.type < 100) {
    DLOG(ERROR) << "Attempt to use Reserved message type (<100), aborted send";
    return kInvalidType;
  }
  if (impl_->routing_table_.kKeys().identity == "ANONYMOUS") {
    // TODO(dirvine) FIXME need to get current used endpoint.
    // set this in message.relat.ip and port
  }
  uint32_t message_unique_id =  impl_->timer_.AddTask(message.timeout,
                                                response_functor);
  protobuf::Message proto_message;
  proto_message.set_id(message_unique_id);
  proto_message.set_source_id(impl_->routing_table_.kKeys().identity);
  proto_message.set_destination_id(message.destination_id);
  proto_message.set_data(message.data);
  proto_message.set_direct(message.direct);
  proto_message.set_replication(message.replication);
  proto_message.set_type(message.type);
  proto_message.set_routing_failure(false);
  SendOn(proto_message, impl_->rudp_, impl_->routing_table_);
  return 0;
}


void Routing::ValidateThisNode(const std::string &node_id,
                              const asymm::PublicKey &public_key,
                              const boost::asio::ip::udp::endpoint &their_endpoint,
                              const boost::asio::ip::udp::endpoint &our_endpoint,
                              bool client) {
  NodeInfo node_info;
  // TODO(dirvine) Add Managed Connection  here !!!
  node_info.node_id = NodeId(node_id);
  node_info.public_key = public_key;
  node_info.endpoint = their_endpoint;
  impl_->rudp_.Add(their_endpoint, our_endpoint, node_id);
  if (client) {
    impl_->client_connections_.push_back(node_info);
  } else {
    impl_->routing_table_.AddNode(node_info);
    if (impl_->bootstrap_nodes_.size() > 1000) {
    impl_->bootstrap_nodes_.erase(impl_->bootstrap_nodes_.begin());
    }
    impl_->bootstrap_nodes_.push_back(their_endpoint);
    WriteBootstrapFile(impl_->bootstrap_nodes_, impl_->bootstrap_file_path_);
  }
}

void Routing::Init() {
  impl_->asio_service_.Start(5);
// TODO(dirvine) handle return code
  impl_->rudp_.GetAvailableEndpoint(& impl_->node_local_endpoint_);
  // TODO(dirvine) connect rudp signals !!
  LOG(INFO) << " Local IP address : " << impl_->node_local_endpoint_.address().to_string();
  LOG(INFO) << " Local Port       : " << impl_->node_local_endpoint_.port();
  Join();
}
//TODO add anonymous join method FIXME

void Routing::Join() {
  if (impl_->bootstrap_nodes_.empty()) {
    DLOG(INFO) << "No bootstrap nodes";
    return;
  }

  for (auto it = impl_->bootstrap_nodes_.begin();
       it != impl_->bootstrap_nodes_.end(); ++it) {
    // TODO(dirvine) send bootstrap requests
  }

//TODO send this message direct to whom we bootstrap onto   rpcs::FindNodes(NodeId(impl_.keys_.identity));
}

bs2::signal<void(int, std::string)> &Routing::MessageReceivedSignal() {
  return impl_->message_received_signal_;
}

bs2::signal<void(int16_t)> &Routing::NetworkStatusSignal() {
  return impl_->network_status_signal_;
}

bs2::signal<void(std::string, std::string)>
                            &Routing::CloseNodeReplacedOldNewSignal() {
  return impl_->routing_table_.CloseNodeReplacedOldNewSignal();
}

  boost::signals2::signal<void(const std::string&,
                           const boost::asio::ip::udp::endpoint&,
                           const bool,
                           const boost::asio::ip::udp::endpoint&,
                           NodeValidatedFunctor &)>
                           &Routing::NodeValidationSignal() {
  return impl_->node_validation_signal_;
                           }

void Routing::ReceiveMessage(const std::string &message) {
  protobuf::Message protobuf_message;
  protobuf::ConnectRequest connection_request;
  if (protobuf_message.ParseFromString(message))
    impl_->message_handler_.ProcessMessage(protobuf_message);
}

void Routing::ConnectionLost(boost::asio::ip::udp::endpoint& lost_endpoint) {
  NodeInfo node_info;
  if ((impl_->routing_table_.GetNodeInfo(lost_endpoint, &node_info) &&
     (impl_->routing_table_.IsMyNodeInRange(node_info.node_id,
                                            Parameters::closest_nodes_size)))) {
    SendOn(rpcs::FindNodes(NodeId(impl_->keys_.identity)),
           impl_->rudp_,
           impl_->routing_table_); // close node, get more
  }
  if (!impl_->routing_table_.DropNode(lost_endpoint))
    return;
  for (auto it = impl_->client_connections_.begin();
        it != impl_->client_connections_.end(); ++it) {
      if((*it).endpoint ==  lost_endpoint) {
        impl_->client_connections_.erase(it);
        return;
      }
  }
  for (auto it = impl_->client_routing_table_.begin();
        it != impl_->client_routing_table_.end(); ++it) {
      if((*it).endpoint ==  lost_endpoint) {
        impl_->client_routing_table_.erase(it);
      SendOn(rpcs::FindNodes(NodeId(impl_->keys_.identity)),
      impl_->rudp_,
      impl_->routing_table_);  // close node, get more
      return;
      }
  }
}


}  // namespace routing

}  // namespace maidsafe
