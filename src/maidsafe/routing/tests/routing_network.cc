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

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing_private.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_api.h"

namespace asio = boost::asio;
namespace ip = asio::ip;

namespace maidsafe {

namespace routing {

namespace test {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // unnamed namespace

size_t GenericNode::next_node_id_(1);

GenericNode::GenericNode(bool client_mode)
    : id_(0),
      node_info_plus_(MakeNodeInfoAndKeys()),
      routing_(),
      functors_(),
      mutex_(),
      client_mode_(client_mode),
      joined_(false),
      expected_(0),
      nat_type_(rudp::NatType::kUnknown),
      endpoint_() {
  endpoint_.address(maidsafe::GetLocalIp());
  endpoint_.port(maidsafe::test::GetRandomPort());
  functors_.close_node_replaced = nullptr;
  functors_.message_received = nullptr;
  functors_.network_status = nullptr;
  routing_.reset(new Routing(GetKeys(node_info_plus_), client_mode));
  LOG(kVerbose) << "Node constructor";
  std::lock_guard<std::mutex> lock(mutex_);
  id_ = next_node_id_++;
}

GenericNode::GenericNode(bool client_mode, const rudp::NatType& nat_type)
    : id_(0),
      node_info_plus_(MakeNodeInfoAndKeys()),
      routing_(),
      functors_(),
      mutex_(),
      client_mode_(client_mode),
      joined_(false),
      expected_(0),
      nat_type_(nat_type),
      endpoint_() {
  endpoint_.address(GetLocalIp());
  endpoint_.port(maidsafe::test::GetRandomPort());
  functors_.close_node_replaced = nullptr;
  functors_.message_received = nullptr;
  functors_.network_status = nullptr;
  routing_.reset(new Routing(GetKeys(node_info_plus_), client_mode));
  routing_->impl_->network_.nat_type_ = nat_type_;
  LOG(kVerbose) << "Node constructor";
  std::lock_guard<std::mutex> lock(mutex_);
  id_ = next_node_id_++;
}

GenericNode::GenericNode(bool client_mode, const NodeInfoAndPrivateKey& node_info)
    : id_(0),
      node_info_plus_(node_info),
      routing_(),
      functors_(),
      mutex_(),
      client_mode_(client_mode),
      joined_(false),
      expected_(0),
      nat_type_(rudp::NatType::kUnknown),
      endpoint_() {
  endpoint_.address(GetLocalIp());
  endpoint_.port(maidsafe::test::GetRandomPort());
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
  return /*routing_->GetStatus()*/0;
}

Endpoint GenericNode::endpoint() const {
  return endpoint_;
}

NodeId GenericNode::connection_id() const {
  return node_info_plus_.node_info.connection_id;
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

void GenericNode::set_client_mode(const bool& client_mode) {
  client_mode_ = client_mode;
}

std::vector<NodeInfo> GenericNode::RoutingTable() const {
  return routing_->impl_->routing_table_.nodes_;
}

std::vector<NodeId> GenericNode::RandomNodeVector() {
  return routing_->impl_->random_node_vector_;
}

NodeId GenericNode::GetRandomExistingNode() {
  return routing_->GetRandomExistingNode();
}

void GenericNode::AddExistingRandomNode(const NodeId& node_id) {
  routing_->AddExistingRandomNode(node_id, routing_->impl_);
}

void GenericNode::Send(const NodeId& destination_id,
                       const NodeId& group_claim,
                       const std::string& data,
                       const ResponseFunctor response_functor,
                       const boost::posix_time::time_duration& timeout,
                       bool direct,
                       bool cache) {
    routing_->Send(destination_id, group_claim, data, response_functor,
                   timeout, direct, cache);
    return;
}

void GenericNode::RudpSend(const NodeId& peer_node_id, const protobuf::Message& message,
              rudp::MessageSentFunctor message_sent_functor) {
  routing_->impl_->network_.RudpSend(message, peer_node_id, message_sent_functor);
}

void GenericNode::SendToClosestNode(const protobuf::Message& message) {
  routing_->impl_->network_.SendToClosestNode(message);
}

bool GenericNode::RoutingTableHasNode(const NodeId node_id) {
  return (std::find_if(routing_->impl_->routing_table_.nodes_.begin(),
                       routing_->impl_->routing_table_.nodes_.end(),
               [node_id](const NodeInfo& node_info) { return node_id == node_info.node_id; })
               !=  routing_->impl_->routing_table_.nodes_.end());
}

bool GenericNode::NonRoutingTableHasNode(const NodeId& node_id) {
  return (std::find_if(routing_->impl_->non_routing_table_.nodes_.begin(),
                       routing_->impl_->non_routing_table_.nodes_.end(),
               [&node_id](const NodeInfo& node_info) { return (node_id == node_info.node_id); } )
               !=  routing_->impl_->non_routing_table_.nodes_.end());
}

testing::AssertionResult GenericNode::DropNode(const NodeId& node_id) {
  LOG(kInfo) << " DropNode " << HexSubstr(routing_->impl_->routing_table_.kNodeId_.String())
                << " Removes " << HexSubstr(node_id.String());
  auto iter = std::find_if(routing_->impl_->routing_table_.nodes_.begin(),
      routing_->impl_->routing_table_.nodes_.end(),
      [&node_id](const NodeInfo& node_info) {
          return (node_id == node_info.node_id);
      });
  if (iter != routing_->impl_->routing_table_.nodes_.end()) {
    LOG(kVerbose) << HexSubstr(routing_->impl_->routing_table_.kNodeId_.String())
               << " Removes " << HexSubstr(node_id.String());
    routing_->impl_->network_.Remove(iter->connection_id);
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

int GenericNode::ZeroStateJoin(const Endpoint& peer_endpoint,
                               const NodeInfo& peer_node_info) {
  return routing_->ZeroStateJoin(functors_, endpoint(), peer_endpoint,
                                 peer_node_info);
}

void GenericNode::Join(const std::vector<Endpoint>& peer_endpoints) {
  routing_->Join(functors_, peer_endpoints);
}

void GenericNode::set_joined(const bool node_joined) {
  joined_ = node_joined;
}

bool GenericNode::joined() const {
  return joined_;
}

int GenericNode::expected() {
  return expected_;
}

void GenericNode::set_expected(const int& expected) {
  expected_ = expected;
}

void GenericNode::PrintRoutingTable() {
  LOG(kVerbose) << " PrintRoutingTable of " << HexSubstr(node_info_plus_.node_info.node_id.String())
             << (IsClient() ? " Client" : "Vault");
  for (auto node_info : routing_->impl_->routing_table_.nodes_) {
    LOG(kVerbose) << "NodeId: " << HexSubstr(node_info.node_id.String());
  }
  LOG(kInfo) << "Non-RoutingTable of " << HexSubstr(node_info_plus_.node_info.node_id.String());
  for (auto node_info : routing_->impl_->non_routing_table_.nodes_) {
    LOG(kVerbose) << "NodeId: " << HexSubstr(node_info.node_id.String());
  }
}


}  // namespace test

}  // namespace routing

}  // namespace maidsafe
