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

#include "maidsafe/routing/tests/routing_network.h"

#include <future>
#include <set>
#include <string>

#include "maidsafe/routing/routing_api_impl.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/log.h"
#include "maidsafe/routing/routing_api.h"

namespace asio = boost::asio;
namespace ip = asio::ip;

namespace maidsafe {

namespace routing {

namespace test {

size_t GenericNode::next_node_id_(0);

GenericNode::GenericNode(bool client_mode)
    : id_(0),
      node_info_plus_(MakeNodeInfoAndKeys()),
      routing_(),
      functors_(),
      mutex_(),
      client_mode_(false),
      joined_(false),
      expected_(0) {
  functors_.close_node_replaced = nullptr;
  functors_.message_received = nullptr;
  functors_.network_status = nullptr;
  routing_.reset(new Routing(GetKeys(node_info_plus_), client_mode));
  LOG(kVerbose) << "Node constructor";
  std::lock_guard<std::mutex> lock(mutex_);
  id_ = next_node_id_++;
}

GenericNode::GenericNode(bool client_mode, const NodeInfoAndPrivateKey &node_info)
    : id_(0),
      node_info_plus_(node_info),
      routing_(),
      functors_(),
      mutex_(),
      client_mode_(false),
      joined_(false),
      expected_(0) {
  functors_.close_node_replaced = nullptr;
  functors_.message_received = nullptr;
  functors_.network_status = nullptr;
  routing_.reset(new Routing(GetKeys(node_info_plus_), client_mode));
  LOG(kVerbose) << "Node constructor";
  std::lock_guard<std::mutex> lock(mutex_);
  id_ = next_node_id_++;
}


GenericNode::~GenericNode() {}

int GenericNode::GetStatus() const {
  return routing_->GetStatus();
}

Endpoint GenericNode::endpoint() const {
  return node_info_plus_.node_info.endpoint;
}

NodeId GenericNode::node_id() const {
  return node_info_plus_.node_info.node_id;
}

size_t GenericNode::id() const {
  return id_;
}

bool GenericNode::IsClient() const {
  return client_mode_;
}

void GenericNode::Send(const NodeId &destination_id,
                       const NodeId &group_id,
                       const std::string &data,
                       const int32_t &type,
                       const ResponseFunctor response_functor,
                       const boost::posix_time::time_duration &timeout,
                       const ConnectType &connect_type) {
    routing_->Send(destination_id, group_id, data, type, response_functor,
                   timeout, connect_type);
    return;
}

void GenericNode::RudpSend(const Endpoint &peer_endpoint, const protobuf::Message &message,
              rudp::MessageSentFunctor message_sent_functor) {
  routing_->impl_->network_.RudpSend(message, peer_endpoint, message_sent_functor);
}

bool GenericNode::RoutingTableHasNode(const NodeId &node_id) {
  return (std::find_if(routing_->impl_->routing_table_.routing_table_nodes_.begin(),
                       routing_->impl_->routing_table_.routing_table_nodes_.end(),
               [&node_id](const NodeInfo &node_info) { return node_id == node_info.node_id; })
               !=  routing_->impl_->routing_table_.routing_table_nodes_.end());
}

bool GenericNode::NonRoutingTableHasNode(const NodeId &node_id) {
  return (std::find_if(routing_->impl_->non_routing_table_.non_routing_table_nodes_.begin(),
                       routing_->impl_->non_routing_table_.non_routing_table_nodes_.end(),
               [&node_id](const NodeInfo &node_info) { return (node_id == node_info.node_id); } )
               !=  routing_->impl_->non_routing_table_.non_routing_table_nodes_.end());
}

testing::AssertionResult GenericNode::DropNode(const NodeId &node_id) {
  LOG(kVerbose) << " DropNode " << HexSubstr(routing_->impl_->routing_table_.kNodeId_.String())
                << " Removes " << HexSubstr(node_id.String());
  auto iter = std::find_if(routing_->impl_->routing_table_.routing_table_nodes_.begin(),
      routing_->impl_->routing_table_.routing_table_nodes_.end(),
      [&node_id](const NodeInfo &node_info) {
          return (node_id == node_info.node_id);
      });
  if (iter != routing_->impl_->routing_table_.routing_table_nodes_.end()) {
    LOG(kVerbose) << HexSubstr(routing_->impl_->routing_table_.kNodeId_.String())
               << " Removes " << HexSubstr(node_id.String());
    routing_->impl_->network_.Remove(iter->endpoint);
  } else {
    testing::AssertionFailure() << HexSubstr(routing_->impl_->routing_table_.keys_.identity)
                                << " does not have " << HexSubstr(node_id.String())
                                << " in routing table of ";
  }
  return testing::AssertionSuccess();
}


NodeInfo GenericNode::node_info() const {
  return node_info_plus_.node_info;
}

int GenericNode::ZeroStateJoin(const NodeInfo &peer_node_info) {
  return routing_->ZeroStateJoin(functors_, endpoint(), peer_node_info);
}

int GenericNode::Join(const Endpoint &peer_endpoint) {
  return routing_->Join(functors_, peer_endpoint);
}

void GenericNode::set_joined(const bool node_joined) {
  joined_ = node_joined;
}

bool GenericNode::joined() const {
  return joined_;
}

uint16_t GenericNode::expected() {
  return expected_;
}

void GenericNode::set_expected(const uint16_t &expected) {
  expected_ = expected;
}

void GenericNode::PrintRoutingTable() {
  LOG(kInfo) << " PrintRoutingTable of " << HexSubstr(node_info_plus_.node_info.node_id.String());
  for (auto node_info : routing_->impl_->routing_table_.routing_table_nodes_) {
    LOG(kInfo) << "NodeId: " << HexSubstr(node_info.node_id.String());
  }
}


}  // namespace test

}  // namespace routing

}  // namespace maidsafe
