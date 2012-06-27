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

#include "maidsafe/common/utils.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/log.h"
#include "maidsafe/routing/routing_api.h"

namespace asio = boost::asio;
namespace ip = asio::ip;

namespace maidsafe {

namespace routing {

namespace test {

size_t RoutingNode::next_node_id_(0);
RoutingNode::RoutingNode(bool client_mode)
    : id_(0),
      node_info_(MakeNode()),
      routing_(),
      functors_(),
      mutex_() {
  functors_.close_node_replaced = nullptr;
  functors_.message_received = nullptr;
  functors_.network_status = nullptr;
  routing_.reset(new Routing(GetKeys(), client_mode));
  LOG(kVerbose) << "Node constructor";
  std::lock_guard<std::mutex> lock(mutex_);
  id_ = next_node_id_++;
}

RoutingNode::~RoutingNode() {}

asymm::Keys RoutingNode::GetKeys() const {
  asymm::Keys keys;
  keys.identity = node_info_.node_id.String();
  keys.public_key = node_info_.public_key;
  return keys;
}

int RoutingNode::GetStatus() {
  return routing_->GetStatus();
}

Endpoint RoutingNode::endpoint() const {
  return node_info_.endpoint;
}

NodeId RoutingNode::Id() const {
  return NodeId(GetKeys().identity);
}

SendStatus RoutingNode::Send(const NodeId &destination_id,
                             const NodeId &group_id,
                             const std::string &data,
                             const int32_t &type,
                             const ResponseFunctor response_functor,
                             const boost::posix_time::time_duration &timeout,
                             const ConnectType &connect_type) {
    return routing_->Send(destination_id, group_id, data, type, response_functor,
                          timeout, connect_type);
}

NodeInfo RoutingNode::node_info() const {
  return node_info_;
}

int RoutingNode::ZeroStateJoin(const NodeInfo &peer_node_info) {
  return routing_->ZeroStateJoin(functors_, endpoint(), peer_node_info);
}

int RoutingNode::Join(const Endpoint &peer_endpoint) {
  return routing_->Join(functors_, peer_endpoint);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
