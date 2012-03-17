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

#include "maidsafe/routing/routing_impl.h"

#include "boost/filesystem/fstream.hpp"
#include "boost/asio.hpp"
#include "maidsafe/transport/managed_connection.h"
//#include "maidsafe/transport/utils.h"
//#include "maidsafe/common/utils.h"

#ifdef __MSVC__
#  pragma warning(push)
#  pragma warning(disable: 4127 4244 4267)
#endif
#include "maidsafe/routing/routing.pb.h"
#ifdef __MSVC__
#  pragma warning(pop)
#endif
//#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/log.h"

namespace maidsafe {

namespace routing {

RoutingImpl::RoutingImpl(Routing::NodeType /*node_type*/,
                         const fs::path &config_file)
    : message_received_signal_(),
      network_status_signal_(),
      asio_service_(),
      config_file_(config_file),
      bootstrap_nodes_(),
      private_key_(),
      node_id_(),
      node_local_endpoint_(),
      node_external_endpoint_(),
      transport_(new transport::ManagedConnection()),
      routing_table_(Contact()), // TODO FIXME contact is empty here
      public_keys_(), 
      cache_size_hint_(Parameters::kNumChunksToCache),
      cache_chunks_(),
      private_key_is_set_(false),
      node_is_set_(false) {
      Init();
}

RoutingImpl::RoutingImpl(Routing::NodeType /*node_type*/,
                         const fs::path &config_file,
                         const asymm::PrivateKey &private_key,
                         const std::string &node_id)
    : message_received_signal_(),
      network_status_signal_(),
      asio_service_(),
      config_file_(config_file),
      bootstrap_nodes_(),
      private_key_(private_key),
      node_id_(node_id),
      node_local_endpoint_(),
      node_external_endpoint_(),
      transport_(new transport::ManagedConnection()),
      routing_table_(Contact()), // TODO FIXME contact is empty here
      public_keys_(), 
      cache_size_hint_(Parameters::kNumChunksToCache),
      cache_chunks_(),
      private_key_is_set_(false),
      node_is_set_(false) {
      Init();
}

void RoutingImpl::Init() {
  asio_service_.Start(5);
  transport_->Init(20);
  node_local_endpoint_ = transport_->GetOurEndpoint();
  LOG(INFO) << " Local IP address : " << node_local_endpoint_.ip.to_string();
  LOG(INFO) << " Local Port       : " << node_local_endpoint_.port;
  transport_->on_message_received()->connect(
      std::bind(&RoutingImpl::ReceiveMessage, this, args::_1));
  Join();
}

void RoutingImpl::AddManualBootStrapEndpoint(transport::Endpoint& endpoint) {
  bootstrap_nodes_.push_back(endpoint);
  LOG(INFO) << " Entered bootstrap IP address : " << endpoint.ip.to_string();
  LOG(INFO) << " Entered bootstrap Port       : " << endpoint.port;
  Join();
}

void RoutingImpl::Send(const Message &message,
                       ResponseReceivedFunctor response_functor) {
  protobuf::Message proto_message;
  proto_message.set_source_id(message.source_id);
  proto_message.set_destination_id(message.destination_id);
  proto_message.set_cacheable(message.cacheable);
  proto_message.set_data(message.data);
  proto_message.set_direct(message.direct);
  proto_message.set_replication(message.replication);
  proto_message.set_type(message.type);
  SendOn(proto_message, NodeId(message.destination_id));
  // TODO(Fraser#5#): 2012-03-14 - We'd better do something with the functor.
}

void RoutingImpl::Join() {
  if (bootstrap_nodes_.empty()) {
    DLOG(INFO) << "No bootstrap nodes";
    return;
  }

  for (auto it = bootstrap_nodes_.begin();
       it != bootstrap_nodes_.end(); ++it) {
   
  }
}

bool RoutingImpl::WriteConfigFile() const {
  // TODO implement 
return false;
}

bool RoutingImpl::ReadConfigFile() {
  protobuf::ConfigFile protobuf;
  // TODO(Fraser#5#): 2012-03-14 - Use try catch / pass error_code for fs funcs.
  if (!fs::exists(config_file_) || !fs::is_regular_file(config_file_)) {
    DLOG(ERROR) << "Cannot read config file " << config_file_;
    return false;
  }
  try {
    fs::ifstream config_file_stream(config_file_);
    if (!protobuf.ParseFromString(config_file_.string()))
      return false;
    if(!private_key_is_set_) {
      if(!protobuf.has_private_key()) {
        DLOG(ERROR) << "No private key in config or set ";
        return false;
      } else {
        asymm::DecodePrivateKey(protobuf.private_key(), &private_key_);
      }
    }
    if (!node_is_set_) {
      if(protobuf.has_node_id()) {
         node_id_ = NodeId(protobuf.node_id());
       } else {
        DLOG(ERROR) << "Cannot read NodeId ";
        return false;
       }
    }
    transport::Endpoint endpoint;
    for (int i = 0; i != protobuf.endpoint_size(); ++i) {
      endpoint.ip.from_string(protobuf.endpoint(i).ip());
      endpoint.port= protobuf.endpoint(i).port();
      bootstrap_nodes_.push_back(endpoint);
    }
  }
  catch(const std::exception &e) {
    DLOG(ERROR) << "Exception: " << e.what();
    return false;
  }
  return true;
}

void RoutingImpl::SendOn(const protobuf::Message &message,
                         const NodeId &target_node) {
  std::string message_data(message.SerializeAsString());
  NodeId send_to = routing_table_.GetClosestNode(target_node, 0).node_id;
//   transport_->Send(
}

void RoutingImpl::ReceiveMessage(const std::string &message) {
  protobuf::Message protobuf_message;
  if (protobuf_message.ParseFromString(message))
    ProcessMessage(protobuf_message);
}

void RoutingImpl::ProcessMessage(protobuf::Message &message) {
  // handle cache data
  if (message.has_cacheable() && message.cacheable()) {
    if (message.response()) {
      AddToCache(message);
     } else  {  // request
       if (GetFromCache(message))
         return;
     }
  }
  // is it for us ??
  if (!routing_table_.AmIClosestNode(NodeId(message.destination_id()))) {
    NodeId next_node =
              routing_table_.GetClosestNode(NodeId(message.destination_id()), 0).node_id;
    SendOn(message, next_node);
    return;
  } else { // I am closest
    if (message.type() == 0) { // ping
      if (message.has_response() && message.response()) {
        DoPingResponse(message);
        return; // Job done !!
      } else {
        DoPingRequest(message);
        return;
      }
    }
    if (message.type() == 1) {// find_nodes
      if (message.has_response() && message.response()) {
        DoFindNodeResponse(message);
        return; // Job done !!
      } else {
        DoFindNodeRequest(message);
        return;
      }
    }
    if (message.has_direct() && message.direct()) {
      if (message.destination_id() != node_id_.String()) {
      // TODO send back a failure I presume !!
      } else {
        try {
          Message msg(message);
          message_received_signal_(static_cast<int>(message.type()), msg);
        }
        catch(const std::exception &e) {
          DLOG(ERROR) << e.what();
        }
        return;
      }
    }
    // I am closest so will send to all my replicant nodes
    message.set_direct(true);
    message.set_source_id(node_id_.String());
    auto close =
          routing_table_.GetClosestNodes(NodeId(message.destination_id()),
                                         static_cast<uint16_t>(message.replication()));
    for (auto it = close.begin(); it != close.end(); ++it) {
      message.set_destination_id((*it).String());
      NodeId send_to = routing_table_.GetClosestNode((*it), 0).node_id;
      SendOn(message, send_to);
    }
    try {
      Message msg(message);
      message_received_signal_(static_cast<int>(message.type()), msg);
    }
    catch(const std::exception &e) {
      DLOG(ERROR) << e.what();
    }
    return;
  }
}

bool RoutingImpl::GetFromCache(protobuf::Message &message) {
  bool result(false);
  for(auto it = cache_chunks_.begin(); it != cache_chunks_.end(); ++it) {
      if ((*it).first == message.source_id()) {
        result = true;
        message.set_destination_id(message.source_id());
        message.set_cacheable(true);
        message.set_data((*it).second);
        message.set_source_id(node_id_.String());
        message.set_direct(true);
        message.set_response(false);
        NodeId next_node =
            routing_table_.GetClosestNode(NodeId(message.destination_id()), 0).node_id;
        SendOn(message, next_node);
      }
  }
  return result;
}

void RoutingImpl::AddToCache(const protobuf::Message &message) {
  std::pair<std::string, std::string> data;
  try {
    // check data is valid TODO FIXME - ask CAA
    if (crypto::Hash<crypto::SHA512>(message.data()) != message.source_id())
      return;
    data = std::make_pair(message.source_id(), message.data());
    cache_chunks_.push_back(data);
    while (cache_chunks_.size() > cache_size_hint_)
      cache_chunks_.erase(cache_chunks_.begin());
  }
  catch(const std::exception &/*e*/) {
    // oohps reduce cache size quickly
    cache_size_hint_ = cache_size_hint_ / 2;
    while (cache_chunks_.size() > cache_size_hint_)
      cache_chunks_.erase(cache_chunks_.begin()+1);
  }
}

void RoutingImpl::DoPingRequest(protobuf::Message &message) {
  protobuf::PingRequest ping_request;
  ping_request.set_ping(true);
  message.set_destination_id(message.source_id());
  message.set_source_id(node_id_.String());
  message.set_data(ping_request.SerializeAsString());
  message.set_direct(true);
  message.set_response(true);
  message.set_replication(1);
  message.set_type(0);
  NodeId send_to(message.destination_id());
  SendOn(message, send_to);
}

void RoutingImpl::DoPingResponse(const protobuf::Message &message) {
  protobuf::PingResponse ping_response;
  if (!ping_response.ParseFromString(message.data()))
    return;
  if (ping_response.pong())
    return; // TODO FIXME IMPLEMENT ME
}

void RoutingImpl::DoConnectRequest(protobuf::Message &message) {
    /// create a connect message to send direct.
  protobuf::ConnectRequest protobuf_connect_request;
  protobuf::Endpoint protobuf_endpoint;
  /// for now accept bootstrap requests without prejeduce
  
  /// for now accept client requests without prejeduce
}

void RoutingImpl::DoConnectResponse(const protobuf::Message &message) {
// send message back  wait on his connect
// add him to a pending endpoint queue
// and when transport asks us to accept him we will
  if (message.has_source_id())
    DLOG(INFO) << " have source ID";
}

void RoutingImpl::DoFindNodeRequest(protobuf::Message &message) {
  protobuf::FindNodesRequest find_nodes;
  protobuf::FindNodesResponse found_nodes;
  std::vector<NodeId>
          nodes(routing_table_.GetClosestNodes(NodeId(message.destination_id()),
                                              static_cast<uint16_t>(find_nodes.num_nodes_requested())));
  
  for (auto it = nodes.begin(); it != nodes.end(); ++it) 
    found_nodes.add_nodes((*it).String());
  message.set_destination_id(message.source_id());
  message.set_source_id(node_id_.String());
  message.set_data(found_nodes.SerializeAsString());
  message.set_direct(true);
  message.set_response(true);
  message.set_replication(1);
  message.set_type(1);
  NodeId send_to(message.destination_id());
  SendOn(message, send_to);
}

void RoutingImpl::DoFindNodeResponse(const protobuf::Message &message) {
  protobuf::FindNodesResponse find_nodes;
  if (! find_nodes.ParseFromString(message.data()))
    return;
  for (int i = 0; i < find_nodes.nodes().size(); ++i) {
    NodeInfo node;
    node.node_id = NodeId(find_nodes.nodes(i));
    routing_table_.AddNode(node, false);
  }
}




}  // namespace routing

}  // namespace maidsafe
