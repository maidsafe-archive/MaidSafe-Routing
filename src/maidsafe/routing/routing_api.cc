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
#include "boost/asio/deadline_timer.hpp"
#include "boost/date_time.hpp"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/service.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/timer.h"

namespace fs = boost::filesystem;
namespace bs2 = boost::signals2;

namespace maidsafe {

namespace routing {

namespace {
const unsigned int kNumChunksToCache(100);
const unsigned int kTimoutInSeconds(5);
}

Message::Message()
    : type(0),
      source_id(),
      destination_id(),
      data(),
      timeout(false),
      cacheable(false),
      direct(false),
      replication(0) {}

Message::Message(const protobuf::Message &protobuf_message)
    : type(protobuf_message.type()),
      source_id(protobuf_message.source_id()),
      destination_id(protobuf_message.destination_id()),
      data(protobuf_message.data()),
      timeout(protobuf_message.routing_failure()),
      cacheable(protobuf_message.cacheable()),
      direct(protobuf_message.direct()),
      replication(protobuf_message.replication()) {}

Routing::Routing(NodeType node_type,
                 const asymm::PrivateKey &private_key,
                 const std::string &node_id,
                 bool encryption_required)
    : asio_service_(),
      bootstrap_file_(),
      bootstrap_nodes_(),
      private_key_(private_key),
      node_local_endpoint_(),
      node_external_endpoint_(),
      transport_(new transport::ManagedConnection()),
      routing_table_(new RoutingTable(node_id)),
      rpc_ptr_(new Rpcs(routing_table_, transport_)),
      service_(new Service(rpc_ptr_, routing_table_)),
      timer_(new Timer(asio_service_)),
      message_received_signal_(),
      network_status_signal_(),
      validate_node_signal_(),
      cache_size_hint_(kNumChunksToCache),
      cache_chunks_(),
      waiting_node_validation_(),
      waiting_for_response_(),
      client_connections_(),
      joined_(false),
      signatures_required_(false),  // we may do this ourselves internally
      encryption_required_(encryption_required),
      node_type_(node_type) {
  Init();
}

// drop existing routing table and restart
void Routing::BootStrapFromThisEndpoint(const transport::Endpoint
&endpoint) {
  LOG(INFO) << " Entered bootstrap IP address : " << endpoint.ip.to_string();
  LOG(INFO) << " Entered bootstrap Port       : " << endpoint.port;
  for (unsigned int i = 0; i < routing_table_->Size(); ++i) {
    NodeInfo remove_node =
    routing_table_->GetClosestNode(routing_table_->kNodeId(), 0);
    transport_->RemoveConnection(remove_node.endpoint);
    routing_table_->DropNode(remove_node.endpoint);
  }
  network_status_signal_(routing_table_->Size());
  bootstrap_nodes_.clear();
  bootstrap_nodes_.push_back(endpoint);
  asio_service_.service().post(std::bind(&Routing::Join, this));
}

int Routing::Send(const Message &message,
                   const ResponseReceivedFunctor &response_functor) {
  if (message.destination_id.empty()) {
    DLOG(ERROR) << "No destination id, aborted send";
    return 1;
  }
  if (message.data.empty()) {
    DLOG(ERROR) << "No data, aborted send";
    return 2;
  }
  if (message.type < 100) {
    DLOG(ERROR) << "Attempt to use Reserved message type (<100), aborted send";
    return 3;
  }
  uint32_t message_unique_id =  timer_->AddTask(kTimoutInSeconds,
                                                response_functor);
  protobuf::Message proto_message;
  proto_message.set_id(message_unique_id);
  proto_message.set_source_id(routing_table_->kNodeId().String());
  proto_message.set_destination_id(message.destination_id);
  proto_message.set_cacheable(message.cacheable);
  proto_message.set_data(message.data);
  proto_message.set_direct(message.direct);
  proto_message.set_replication(message.replication);
  proto_message.set_type(message.type);
  proto_message.set_routing_failure(false);
  SendOn(proto_message);
  return 0;
}

void Routing::Init() {
  asio_service_.Start(5);
  // TODO(dirvine) fill in bootstrap file location and do ReadConfigFile
  transport_->Init(20);
  node_local_endpoint_ = transport_->GetOurEndpoint();
  LOG(INFO) << " Local IP address : " << node_local_endpoint_.ip.to_string();
  LOG(INFO) << " Local Port       : " << node_local_endpoint_.port;
  transport_->on_message_received()->connect(
      std::bind(&Routing::ReceiveMessage, this, args::_1));
  Join();
}

bool Routing::ReadBootstrapFile() {  // TODO(dirvine) FIXME now a dir
  protobuf::ConfigFile protobuf_config;
  protobuf::Bootstrap protobuf_bootstrap;
// TODO(Fraser#5#): 2012-03-14 - Use try catch / pass error_code for fs funcs.
//  if (!fs::exists(config_file_) || !fs::is_regular_file(config_file_)) {
  //    DLOG(ERROR) << "Cannot read config file " << config_file_;
//    return false;
//  }
//  try {
  //    fs::ifstream config_file_stream(config_file_);
//    if (!protobuf_config.ParseFromString(config_file_.string()))
//      return false;
//    if (!private_key_is_set_) {
  //      if (!protobuf_config.has_private_key()) {
    //        DLOG(ERROR) << "No private key in config or set ";
//        return false;
//      } else {
  //        asymm::DecodePrivateKey(protobuf_config.private_key(),
  //  &private_key_);
//      }
//    }
//    if (!node_is_set_) {
  //      if (protobuf_config.has_node_id()) {
    //         node_id_ = NodeId(protobuf_config.node_id());
//       } else {
  //        DLOG(ERROR) << "Cannot read NodeId ";
//        return false;
//       }
//    }
//    transport::Endpoint endpoint;
//    for (int i = 0; i != protobuf_bootstrap.endpoint_size(); ++i) {
  //      endpoint.ip.from_string(protobuf_bootstrap.endpoint(i).ip());
//      endpoint.port= protobuf_bootstrap.endpoint(i).port();
//      bootstrap_nodes_.push_back(endpoint);
//    }
//  }
//  catch(const std::exception &e) {
  //    DLOG(ERROR) << "Exception: " << e.what();
//    return false;
//  }
  return true;
}

bool Routing::WriteBootstrapFile() const {
  // TODO(dirvine) implement
  return false;
}

bs2::signal<void(int, std::string)> &Routing::RequestReceivedSignal() {
  return message_received_signal_;
}

bs2::signal<void(unsigned int)> &Routing::NetworkStatusSignal() {
  return network_status_signal_;
}

bs2::signal<void(std::string)> &Routing::ValidateNodeIdSignal() {
  return validate_node_signal_;
}

void Routing::Join() {
  if (bootstrap_nodes_.empty()) {
    DLOG(INFO) << "No bootstrap nodes";
    return;
  }

  for (auto it = bootstrap_nodes_.begin();
       it != bootstrap_nodes_.end(); ++it) {
    // TODO(dirvine) send bootstrap requests
  }
}

void Routing::AckReceived(const transport::TransportCondition &return_value,
                          const std::string &message) {
  if (return_value != transport::kSuccess)
    return;  // TODO(dirvine) FIXME we may need to take action here
             // depending on return code
  ReceiveMessage(message);
}

void Routing::SendOn(const protobuf::Message &message) {
  std::string message_data(message.SerializeAsString());
  transport::Endpoint send_to =
             routing_table_->
                 GetClosestNode(NodeId(message.destination_id()), 0).endpoint;
//   transport::ResponseFunctor response_functor =
//                 std::bind(&Routing::AckReceived, this, args::_1, args::_2);
// TODO(dirvine)  transport_->Send(send_to, message_data, response_functor);
}

void Routing::ReceiveMessage(const std::string &message) {
  protobuf::Message protobuf_message;
  protobuf::ConnectRequest connection_request;
  if (protobuf_message.ParseFromString(message))
    ProcessMessage(protobuf_message);
}

void Routing::ProcessMessage(protobuf::Message &message) {
  // TODO(dirvine) if message is from/for a client connected
  // to us replace the source address to me for requests
  // in responses the message will be direct with our address

  // handle cache data
  if (message.has_cacheable() && message.cacheable()) {
    if (message.has_response() && message.response()) {
      AddToCache(message);
     } else  {  // request
       if (GetFromCache(message))
         return;
     }
  }
  // is it for us ??
  if (!routing_table_->AmIClosestNode(NodeId(message.destination_id()))) {
    NodeId next_node =
     routing_table_->GetClosestNode(NodeId(message.destination_id()),
                                    0).node_id;
    SendOn(message);
    return;
  } else {  // I am closest
    if (message.type() == 0) {  // ping
      if (message.has_response() && message.response()) {
        // TODO(dirvine) FIXME its for me  !! DoPingResponse(message);
        return;  // Job done !!
      } else {
        service_->Ping(message);
      }
    }
    if (message.type() == 1) {  // find_nodes
      if (message.has_response() && message.response()) {
        // TODO(dirvine) FIXME its for me  !!  DoFindNodeResponse(message);
       return;
      } else {
         service_->FindNodes(message);
         return;
      }
    }
    if (message.type() == 2) {  // bootstrap
      if (message.has_response() && message.response()) {
         // TODO(dirvine) FIXME its for me  !  DoConnectResponse(message);
       return;
      } else {
         service_->Connect(message);
         return;
      }
    }

    // if this is set not direct and ID == ME do NOT respond.
    if (!message.direct() &&
      (message.destination_id() != routing_table_->kNodeId().String())) {
      try {
        message_received_signal_(static_cast<int>(message.type()),
                                 message.data());
      }
      catch(const std::exception &e) {
        DLOG(ERROR) << e.what();
      }
    } else {  // I am closest and it's direct
      if(message.response()) {
        timer_->ExecuteTaskNow(message);
        return;
      } else { // I am closest and it's a request
        try {
          message_received_signal_(static_cast<int>(message.type()),
                                 message.data());
        } catch(const std::exception &e) {
        DLOG(ERROR) << e.what();
        }
      }
    }
    // I am closest so will send to all my replicant nodes
    message.set_direct(true);
    message.set_source_id(routing_table_->kNodeId().String());
    auto close =
          routing_table_->GetClosestNodes(NodeId(message.destination_id()),
                                 static_cast<uint16_t>(message.replication()));
    for (auto it = close.begin(); it != close.end(); ++it) {
      message.set_destination_id((*it).String());
      SendOn(message);
    }
  }
}

void Routing::ProcessPingResponse(protobuf::Message& message) {
  // TODO , do we need this and where and how can I update the response
  protobuf::PingResponse ping_response;
  if (ping_response.ParseFromString(message.data()) &&
    ping_response.has_pong()) {
    //  do stuff here
    }
}

// the other node agreed to connect - he has accepted our connection
void Routing::ProcessConnectResponse(protobuf::Message& message) {
  protobuf::ConnectResponse connect_response;
  if (connect_response.ParseFromString(message.data()) &&
      connect_response.answer()) {
    NodeId node_to_add(connect_response.contact().node_id());
    for (auto it = waiting_node_validation_.begin();
                  it != waiting_node_validation_.end();
                  ++it) {
      if ((*it).node_id == node_to_add) {
        routing_table_->AddNode(*it);  // by now public key is also valid
        waiting_node_validation_.erase(it);
        break;
      }
    }
    // if this was a bootstrap connect then we will have our closest nodes
    // as well. Bootstrap node will drop us in 10 minutes;
    protobuf::FindNodesResponse find_nodes(connect_response.find_nodes());
    if (connect_response.has_find_nodes()) {
      protobuf::FindNodesResponse find_nodes(connect_response.find_nodes());
      for(int i = 0; i < find_nodes.nodes_size() ; ++i) {
        TryAddNode(NodeId(find_nodes.nodes(i)));
      }
    }
  }
}

void Routing::TryAddNode(NodeId node) {
  NodeInfo node_info;
  node_info.node_id = node;
  if (routing_table_->CheckNode(node_info)) {
    waiting_node_validation_.push_back(node_info);
    validate_node_signal_(node.String());
    boost::asio::deadline_timer timer(asio_service_.service(),
                                 boost::posix_time::seconds(kTimoutInSeconds));
    timer.async_wait(std::bind(&Routing::FindAndKillWaitingNodeValidation,
                               this, node));
  }
}

void Routing::ValidateThisNode(bool valid,
                               std::string node_id,
                               rsa::PublicKey& public_key) {
  NodeId node(node_id);
  for (auto it = waiting_node_validation_.begin();
       it != waiting_node_validation_.end();
       ++it) {
    if (!valid) {
      waiting_node_validation_.erase(it);
      return;
    }
    if ((*it).node_id == node) {
      (*it).public_key = public_key;
        // TODO(dirvine) create a connect request

      break;
    }
  }
}

void Routing::FindAndKillWaitingNodeValidation(NodeId node) {
  for (auto it = waiting_node_validation_.begin();
       it != waiting_node_validation_.end();
       ++it) {
    if ((*it).node_id == node) {
      waiting_node_validation_.erase(it);
      break;
    }
  }
}

bool Routing::GetFromCache(protobuf::Message &message) {
  bool result(false);
  for (auto it = cache_chunks_.begin(); it != cache_chunks_.end(); ++it) {
      if ((*it).first == message.source_id()) {
        result = true;
        message.set_destination_id(message.source_id());
        message.set_cacheable(true);
        message.set_data((*it).second);
        message.set_source_id(routing_table_->kNodeId().String());
        message.set_direct(true);
        message.set_response(false);
        SendOn(message);
      }
  }
  return result;
}

void Routing::AddToCache(const protobuf::Message &message) {
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


}  // namespace routing

}  // namespace maidsafe
