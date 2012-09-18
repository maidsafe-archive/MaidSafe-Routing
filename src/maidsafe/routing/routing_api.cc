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
#include <future>
#include "boost/asio/deadline_timer.hpp"
#include "boost/thread/future.hpp"

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/bootstrap_file_handler.h"
#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/network_utils.h"
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

  assert((client_mode || !keys.identity.empty()) &&
         "Server Nodes cannot be created without valid keys");
  if (keys.identity.empty()) {
    impl_->anonymous_node_ = true;
    LOG(kInfo) << "Anonymous node ID: " << DebugId(impl_->kNodeId_);
  }
}

Routing::~Routing() {
  LOG(kVerbose) << "~Routing() - Disconnecting functors.  Routing table Size: "
                << impl_->routing_table_.Size();
  DisconnectFunctors();
}

bool Routing::CheckBootstrapFilePath() const {
  // Global bootstrap file
  std::vector<Endpoint> global_bootstrap_nodes;
  boost::system::error_code exists_error_code, is_regular_file_error_code;

  fs::path local_file = fs::current_path() / "bootstrap";
  if (!fs::exists(local_file, exists_error_code) ||
      !fs::is_regular_file(local_file, is_regular_file_error_code) ||
      exists_error_code || is_regular_file_error_code) {
    if (exists_error_code) {
      LOG(kWarning) << "Failed to find bootstrap file at " << local_file << ".  "
                    << exists_error_code.message();
    }
    if (is_regular_file_error_code) {
      LOG(kError) << "bootstrap file is not a regular file " << local_file << ".  "
                  << is_regular_file_error_code.message();
    }
    return false;
  } else {
    LOG(kVerbose) << "Found bootstrap file at " << local_file;
    impl_->bootstrap_file_path_ = local_file;
    impl_->bootstrap_nodes_ = ReadBootstrapFile(impl_->bootstrap_file_path_);
    impl_->network_.set_bootstrap_file_path(impl_->bootstrap_file_path_);

    // Appending global_bootstrap_file's contents
    for (auto i: global_bootstrap_nodes)
      impl_->bootstrap_nodes_.push_back(i);
    return true;
  }
}

void Routing::Join(Functors functors,
                   std::vector<boost::asio::ip::udp::endpoint> peer_endpoints) {
#ifdef LOCAL_TEST
  LOG(kVerbose) << "RoutingPrivate::bootstraps_.size(): " << RoutingPrivate::bootstraps_.size();
  for (auto endpoint : RoutingPrivate::bootstraps_)
    peer_endpoints.push_back(endpoint);
#endif
  ConnectFunctors(functors);
  if (!peer_endpoints.empty()) {
    return BootstrapFromTheseEndpoints(functors, peer_endpoints);
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
  impl_->routing_table_.set_remove_node_functor([this](const NodeInfo& node,
                                                       const bool& internal_rudp_only) {
      RemoveNode(node, internal_rudp_only);
  });

  impl_->routing_table_.set_network_status_functor(functors.network_status);
  impl_->routing_table_.set_close_node_replaced_functor(functors.close_node_replaced);
  impl_->message_handler_->set_message_received_functor(functors.message_received);
  impl_->message_handler_->set_request_public_key_functor(functors.request_public_key);
  impl_->functors_ = functors;
}

void Routing::DisconnectFunctors() {  // TODO(Prakash) : fix race condition when functors in use
  impl_->routing_table_.set_remove_node_functor(nullptr);
  impl_->routing_table_.set_network_status_functor(nullptr);
  impl_->routing_table_.set_close_node_replaced_functor(nullptr);
  impl_->message_handler_->set_message_received_functor(nullptr);
  impl_->message_handler_->set_request_public_key_functor(nullptr);
  impl_->functors_ = Functors();
}

void Routing::BootstrapFromTheseEndpoints(const Functors& functors,
                                          const std::vector<Endpoint>& endpoints) {
  LOG(kInfo) << "Doing a BootstrapFromThisEndpoint Join.  Entered first bootstrap endpoint: "
             << endpoints[0]
             << ", this node's ID: " << DebugId(impl_->kNodeId_)
             << (impl_->client_mode_ ? " Client" : "");
  if (impl_->routing_table_.Size() > 0) {
    for (uint16_t i = 0; i < impl_->routing_table_.Size(); ++i) {
      NodeInfo remove_node =
          impl_->routing_table_.GetClosestNode(impl_->kNodeId_);
      impl_->network_.Remove(remove_node.connection_id);
      impl_->routing_table_.DropNode(remove_node.node_id, true);
    }
    if (impl_->functors_.network_status)
      impl_->functors_.network_status(static_cast<int>(impl_->routing_table_.Size()));
  }
  impl_->bootstrap_nodes_.clear();
#ifdef LOCAL_TEST
  uint16_t start_index(0);
  start_index = (endpoints.size() > 2) ? 2 : 0;
  if (start_index == 0) {
    impl_->bootstrap_nodes_.insert(impl_->bootstrap_nodes_.begin(),
                                   endpoints.begin(),
                                   endpoints.end());
  } else {
    uint16_t random(static_cast<uint16_t>(RandomUint32() % (endpoints.size() - 2)));
    for (size_t index(0); index < (endpoints.size() - 2); ++index) {
      impl_->bootstrap_nodes_.push_back(
          endpoints[(random + index) % (endpoints.size() - 2) + 2]);
    }
  }
  for (auto endpoint : impl_->bootstrap_nodes_)
    LOG(kVerbose) << "Static ep: " << endpoint;
#else
  impl_->bootstrap_nodes_.insert(impl_->bootstrap_nodes_.begin(),
                                 endpoints.begin(),
                                 endpoints.end());
#endif
  DoJoin(functors);
}

void Routing::DoJoin(const Functors& functors) {
  int return_value(DoBootstrap());
  if (kSuccess != return_value) {
    if (functors.network_status)
      functors.network_status(return_value);
    return;
  }

  if (impl_->anonymous_node_) {  // No need to do find node for anonymous node
    if (functors.network_status)
      functors.network_status(return_value);
    return;
  }

  assert(!impl_->anonymous_node_&& "Not allowed for anonymous nodes");
  FindClosestNode(boost::system::error_code(), impl_, 0);

  if (functors.network_status)
    functors.network_status(return_value);
}

int Routing::DoBootstrap() {
  if (impl_->bootstrap_nodes_.empty()) {
    LOG(kError) << "No bootstrap nodes.  Aborted join.";
    return kInvalidBootstrapContacts;
  }
  std::weak_ptr<RoutingPrivate> impl_weak_ptr(impl_);
  return impl_->network_.Bootstrap(
      impl_->bootstrap_nodes_,
      impl_->client_mode_,
      [=](const std::string& message) { ReceiveMessage(message, impl_weak_ptr); },
      [=](const NodeId& lost_connection_id) { ConnectionLost(lost_connection_id, impl_weak_ptr);});  // NOLINT
}

void Routing::FindClosestNode(const boost::system::error_code& error_code,
                              std::weak_ptr<RoutingPrivate> impl,
                              int attempts) {
  if (error_code != boost::asio::error::operation_aborted) {
    std::shared_ptr<RoutingPrivate> pimpl = impl.lock();
    if (!pimpl) {
      LOG(kVerbose) << "Ignoring another FindClosestNode, since this node is shutting down";
      return;
    }
    assert(!pimpl->anonymous_node_ && "Not allowed for anonymous nodes");
    if (attempts == 0) {
      assert(!pimpl->network_.bootstrap_connection_id().Empty()
             && "Only after bootstraping succeeds");
      assert(!pimpl->network_.this_node_relay_connection_id().Empty() &&
             "This should be set after bootstraping succeeds");
    } else {
      if (pimpl->routing_table_.Size() > 0) {
        // Exit the loop & start recovery loop
        LOG(kInfo) << "Added a node in routing table."
                   << " Terminating setup loop & Scheduling recovery loop.";
        pimpl->recovery_timer_.expires_from_now(
            boost::posix_time::seconds(Parameters::recovery_timeout_in_seconds));
        std::weak_ptr<RoutingPrivate> impl_weak_ptr(pimpl);
        pimpl->recovery_timer_.async_wait([=](const boost::system::error_code& error_code) {
                                                ReSendFindNodeRequest(error_code, impl_weak_ptr);
                                              });
        return;
      }

      if (attempts >= 10) {
        LOG(kError) << "This node's [" << HexSubstr(pimpl->keys_.identity)
                    << "] failed to get closest node."
                    << " Reconnecting ....";
        // TODO(Prakash) : Remove the bootstrap node from the list
        std::weak_ptr<RoutingPrivate> impl_weak_ptr(pimpl);
        std::async(std::launch::async, [=] {
                     std::shared_ptr<RoutingPrivate> pimpl = impl_weak_ptr.lock();
                     if (!pimpl) {
                       LOG(kVerbose) << "Not re bootstrapping since this node is shutting down";
                       return;
                     }
                     Sleep(boost::posix_time::seconds(3));
                     DoJoin(impl_->functors_);
                  });
      }
    }

    protobuf::Message find_node_rpc(
        rpcs::FindNodes(pimpl->kNodeId_,
                        pimpl->kNodeId_,
                        1,
                        true,
                        pimpl->network_.this_node_relay_connection_id()));

    rudp::MessageSentFunctor message_sent_functor(
        [=](int message_sent) {
          if (message_sent == kSuccess)
            LOG(kInfo) << "Successfully sent FindNodes RPC to bootstrap connection id : "
                       << DebugId(pimpl->network_.bootstrap_connection_id());
          else
            LOG(kError) << "Failed to send FindNodes RPC to bootstrap connection id : "
                        << DebugId(pimpl->network_.bootstrap_connection_id());
        });

    ++attempts;
    pimpl->network_.SendToDirect(find_node_rpc, pimpl->network_.bootstrap_connection_id(),
                                 message_sent_functor);
    pimpl->setup_timer_.expires_from_now(boost::posix_time::seconds(
                                         Parameters::setup_timeout_in_seconds));
    std::weak_ptr<RoutingPrivate> impl_weak_ptr(pimpl);
    pimpl->setup_timer_.async_wait([=](boost::system::error_code error_code_local) {
                                       FindClosestNode(error_code_local, impl_weak_ptr, attempts);
                                     });
  }
}

int Routing::ZeroStateJoin(Functors functors,
                           const Endpoint& local_endpoint,
                           const Endpoint& peer_endpoint,
                           const NodeInfo& peer_node) {
  std::weak_ptr<RoutingPrivate> impl_weak_ptr(impl_);
  assert(impl_weak_ptr.lock());
  assert((!impl_->client_mode_) && "no client nodes allowed in zero state network");
  assert((!impl_->anonymous_node_) && "not allowed on anonymous node");
  impl_->bootstrap_nodes_.clear();
  impl_->bootstrap_nodes_.push_back(peer_endpoint);
  if (impl_->bootstrap_nodes_.empty()) {
    LOG(kError) << "No bootstrap nodes.  Aborted join.";
    return kInvalidBootstrapContacts;
  }

  ConnectFunctors(functors);
  int result(impl_->network_.Bootstrap(
      impl_->bootstrap_nodes_,
      impl_->client_mode_,
      [=](const std::string& message) { ReceiveMessage(message, impl_weak_ptr); },
      [=](const NodeId& lost_connection_id) { ConnectionLost(lost_connection_id, impl_weak_ptr); },
      local_endpoint));

  if (result != kSuccess) {
    LOG(kError) << "Could not bootstrap zero state node from local endpoint : "
                << local_endpoint << " with peer endpoint : " << peer_endpoint;
    return result;
  }

  LOG(kInfo) << "This Node [" << DebugId(impl_->kNodeId_) << "]"
             << "bootstrap connection id : "
             << DebugId(impl_->network_.bootstrap_connection_id());

  assert(!peer_node.node_id.Empty() && "empty nodeid passed");
  assert((impl_->network_.bootstrap_connection_id() == peer_node.node_id) &&
         "Should bootstrap only with known peer for zero state network");
  LOG(kVerbose) << local_endpoint << " Bootstrapped with remote endpoint " << peer_endpoint;
  rudp::NatType nat_type(rudp::NatType::kUnknown);
  rudp::EndpointPair peer_endpoint_pair;  // zero state nodes must be directly connected endpoint
  rudp::EndpointPair this_endpoint_pair;
  peer_endpoint_pair.external = peer_endpoint_pair.local = peer_endpoint;
  this_endpoint_pair.external = this_endpoint_pair.local = local_endpoint;

  result = impl_->network_.GetAvailableEndpoint(peer_node.node_id, peer_endpoint_pair,
                                                this_endpoint_pair, nat_type);
  if (result != kSuccess) {
    LOG(kError) << "Failed to get available endpoint to add zero state node : " << peer_endpoint;
    return result;
  }

  result = impl_->network_.Add(peer_node.node_id, peer_endpoint_pair, "invalid");
  if (result != kSuccess) {
    LOG(kError) << "Failed to add zero state node : " << peer_endpoint;
    return result;
  }

  ValidateAndAddToRoutingTable(impl_->network_,
                               impl_->routing_table_,
                               impl_->non_routing_table_,
                               peer_node.node_id,
                               peer_node.node_id,
                               peer_node.public_key,
                               false);
  // Now poll for routing table size to have other zero state peer.
  uint8_t poll_count(0);
  do {
    Sleep(boost::posix_time::milliseconds(100));
  } while ((impl_->routing_table_.Size() == 0) && (++poll_count < 50));
  if (impl_->routing_table_.Size() != 0) {
    LOG(kInfo) << "Node Successfully joined zero state network, with "
               << DebugId(impl_->network_.bootstrap_connection_id())
               << ", Routing table size - " << impl_->routing_table_.Size()
               << ", Node id : " << DebugId(impl_->kNodeId_);

    impl_->recovery_timer_.expires_from_now(
        boost::posix_time::seconds(Parameters::recovery_timeout_in_seconds));
    impl_->recovery_timer_.async_wait([=](const boost::system::error_code& error_code) {
                                          ReSendFindNodeRequest(error_code, impl_weak_ptr);
                                        });
#ifdef LOCAL_TEST
    std::lock_guard<std::mutex> lock(RoutingPrivate::mutex_);
    RoutingPrivate::bootstraps_.push_back(local_endpoint);
    RoutingPrivate::bootstrap_nodes_id_.insert(NodeId(impl_->keys_.identity));
#endif
    return kSuccess;
  } else {
    LOG(kError) << "Failed to join zero state network, with bootstrap_endpoint "
                << peer_endpoint;
    return kNotJoined;
  }
}

void Routing::Send(const NodeId& destination_id,
                   const NodeId& group_claim,
                   const std::string& data,
                   ResponseFunctor response_functor,
                   const boost::posix_time::time_duration& timeout,
                   bool direct,
                   bool cache) {
  if (destination_id.Empty() || !destination_id.IsValid()) {
    LOG(kError) << "Invalid destination ID, aborted send";
    if (response_functor)
      response_functor(std::vector<std::string>());
    return;
  }

  if (data.empty() || (data.size() > Parameters::max_data_size)) {
    LOG(kError) << "Data size not allowed : " << data.size();
    if (response_functor)
      response_functor(std::vector<std::string>());
    return;
  }

  protobuf::Message proto_message;
  proto_message.set_destination_id(destination_id.String());
  proto_message.set_routing_message(false);
  proto_message.add_data(data);
  proto_message.set_type(static_cast<int32_t>(MessageType::kNodeLevel));
  proto_message.set_cacheable(cache);
  proto_message.set_direct(direct);
  proto_message.set_client_node(impl_->client_mode_);
  proto_message.set_request(true);
  proto_message.set_hops_to_live(Parameters::hops_to_live);
  uint16_t replication(1);
  if (group_claim.IsValid() && !group_claim.Empty())
    proto_message.set_group_claim(group_claim.String());

  if (!direct) {
    replication = Parameters::node_group_size;
    if (response_functor)
      proto_message.set_id(impl_->timer_.AddTask(timeout, response_functor,
                                                 Parameters::node_group_size));
  } else {
    if (response_functor)
      proto_message.set_id(impl_->timer_.AddTask(timeout, response_functor, 1));
  }

  proto_message.set_replication(replication);
  // Anonymous node /Partial join state
  if (impl_->anonymous_node_ || (impl_->routing_table_.Size() == 0)) {
    proto_message.set_relay_id(impl_->kNodeId_.String());
    proto_message.set_relay_connection_id(impl_->network_.this_node_relay_connection_id().String());
    NodeId bootstrap_connection_id(impl_->network_.bootstrap_connection_id());
    assert(proto_message.has_relay_connection_id() && "did not set this_node_relay_connection_id");
    rudp::MessageSentFunctor message_sent(
        [=](int result) {
          if (rudp::kSuccess != result) {
            impl_->timer_.CancelTask(proto_message.id());
            if (impl_->anonymous_node_) {
              LOG(kError) << "Anonymous Session Ended, Send not allowed anymore";
              impl_->functors_.network_status(kAnonymousSessionEnded);
            } else {
              LOG(kError) << "Partial join Session Ended, Send not allowed anymore";
              if (impl_->functors_.network_status)
                impl_->functors_.network_status(kPartialJoinSessionEnded);
            }
          } else {
            LOG(kInfo) << "Message Sent from Anonymous/Partial joined node";
          }
        });

    impl_->network_.SendToDirect(proto_message, bootstrap_connection_id, message_sent);
    return;
  }

  // Non Anonymous, normal node
  proto_message.set_source_id(impl_->kNodeId_.String());

  if (impl_->kNodeId_ != destination_id) {
    impl_->network_.SendToClosestNode(proto_message);
  } else if (impl_->client_mode_) {
    LOG(kVerbose) << "Client sending request to self id";
    impl_->network_.SendToClosestNode(proto_message);
  } else {
    LOG(kInfo) << "Sending request to self";
    ReceiveMessage(proto_message.SerializeAsString(), impl_);
  }
}

void Routing::ReceiveMessage(const std::string& message, std::weak_ptr<RoutingPrivate> impl) {
  std::shared_ptr<RoutingPrivate> pimpl = impl.lock();
  if (!pimpl) {
    LOG(kVerbose) << "Ignoring message received since this node is shutting down";
    return;
  }

  protobuf::Message protobuf_message;
  if (protobuf_message.ParseFromString(message)) {
    bool relay_message(!protobuf_message.has_source_id());
    LOG(kInfo) << "This node [" << DebugId(pimpl->kNodeId_) << "] received message type: "
               << MessageTypeString(protobuf_message) << " from "
               << (relay_message ? HexSubstr(protobuf_message.relay_id()) + " -- RELAY REQUEST" :
                                   HexSubstr(protobuf_message.source_id()))
               << " id: " << protobuf_message.id();
    if (protobuf_message.has_source_id())
      AddExistingRandomNode(NodeId(protobuf_message.source_id()), pimpl);
    pimpl->message_handler_->HandleMessage(protobuf_message);
  } else {
    LOG(kWarning) << "Message received, failed to parse";
  }
#ifdef LOCAL_TEST
  pimpl->LocalTestUtility(protobuf_message);
#endif
}

NodeId Routing::GetRandomExistingNode() {
  std::lock_guard<std::mutex> lock(impl_->random_node_mutex_);
  if (impl_->random_node_vector_.empty())
    return NodeId();
  NodeId node;
  auto queue_size = impl_->random_node_vector_.size();
  node = (queue_size >= 100) ? (impl_->random_node_vector_[0]) :
                               (impl_->random_node_vector_[RandomUint32() % queue_size]);
  LOG(kVerbose) << "RandomNodeQueue : Getting node, queue size now "
                << queue_size;
  if (queue_size >= 100) {
    impl_->random_node_vector_.erase(impl_->random_node_vector_.begin());
  }
  return node;
}

void Routing::AddExistingRandomNode(NodeId node, std::weak_ptr<RoutingPrivate> impl) {
  std::shared_ptr<RoutingPrivate> pimpl = impl.lock();
  if (!pimpl) {
    LOG(kVerbose) << "Ignoring message received since this node is shutting down";
    return;
  }

  if (node.IsValid() && !node.Empty()) {
    std::lock_guard<std::mutex> lock(pimpl->random_node_mutex_);
    if (std::find_if(pimpl->random_node_vector_.begin(), pimpl->random_node_vector_.end(),
                   [node] (const NodeId& vect_node) {
                     return vect_node == node;
                     }) !=  pimpl->random_node_vector_.end())
      return;
    pimpl->random_node_vector_.push_back(node);
    auto queue_size = pimpl->random_node_vector_.size();
    LOG(kVerbose) << "RandomNodeQueue : Added node, queue size now "
                  << queue_size;
    if (queue_size > 100)
      pimpl->random_node_vector_.erase(pimpl->random_node_vector_.begin());
  }
}

void Routing::ConnectionLost(const NodeId& lost_connection_id, std::weak_ptr<RoutingPrivate> impl) {
  std::shared_ptr<RoutingPrivate> pimpl = impl.lock();
  if (!pimpl) {
    LOG(kVerbose) << "Ignoring connection lost callback since this node is shutting down";
    return;
  }

  LOG(kVerbose) << "Routing::ConnectionLost with ----------------------------"
                << DebugId(lost_connection_id);
  NodeInfo dropped_node;
  bool resend(!pimpl->tearing_down_ &&
              (pimpl->routing_table_.GetNodeInfo(lost_connection_id, dropped_node) &&
              pimpl->routing_table_.IsThisNodeInRange(dropped_node.node_id,
                                                      Parameters::closest_nodes_size)));

  // Checking routing table
  dropped_node = pimpl->routing_table_.DropNode(lost_connection_id, true);
  if (!dropped_node.node_id.Empty()) {
    LOG(kWarning) << "[" <<HexSubstr(impl_->keys_.identity) << "]"
                  << "Lost connection with routing node "
                  << DebugId(dropped_node.node_id);
  }

  // Checking non-routing table
  if (dropped_node.node_id.Empty()) {
    resend = false;
    dropped_node = pimpl->non_routing_table_.DropNode(lost_connection_id);
    if (!dropped_node.node_id.Empty()) {
      LOG(kWarning) << "[" <<HexSubstr(impl_->keys_.identity) << "]"
                    << "Lost connection with non-routing node "
                    << HexSubstr(dropped_node.node_id.String());
    } else if (!pimpl->network_.bootstrap_connection_id().Empty() &&
               lost_connection_id == pimpl->network_.bootstrap_connection_id()) {
      LOG(kWarning) << "[" <<HexSubstr(impl_->keys_.identity) << "]"
                    << "Lost temporary connection with bootstrap node. connection id :"
                    << DebugId(lost_connection_id);
      pimpl->network_.clear_bootstrap_connection();
    } else {
      LOG(kWarning) << "[" <<HexSubstr(impl_->keys_.identity) << "]"
                    << "Lost connection with unknown/internal connection id "
                    << DebugId(lost_connection_id);
    }
  }

  if (resend) {
    // Close node lost, get more nodes
    LOG(kWarning) << "Lost close node, getting more.";
    ReSendFindNodeRequest(boost::system::error_code(), pimpl, true);
  }

#ifdef LOCAL_TEST
  pimpl->RemoveConnectionFromBootstrapList(lost_connection_id);
#endif
}

void Routing::RemoveNode(const NodeInfo& node, const bool& internal_rudp_only) {
  if (node.connection_id.Empty() || node.node_id.Empty()) {
    return;
  }
  impl_->network_.Remove(node.connection_id);
  if (internal_rudp_only) {  // No recovery
    LOG(kInfo) << "Routing: removed node : " << DebugId(node.node_id)
               << ". Removed internal rudp connection id : " << DebugId(node.connection_id);
    return;
  }

  LOG(kInfo) << "Routing: removed node : " << DebugId(node.node_id)
             << ". Removed rudp connection id : " << DebugId(node.connection_id);

  // TODO(Prakash): Handle pseudo connection removal here and NRT node removal

  bool resend(impl_->routing_table_.IsThisNodeInRange(node.node_id,
                                                      Parameters::closest_nodes_size));
  if (resend) {
    // Close node removed by routing, get more nodes
    LOG(kWarning) << "Removed close node, sending find node to get more nodes.";
    ReSendFindNodeRequest(boost::system::error_code(), impl_, true);
  }
}

bool Routing::ConfirmGroupMembers(const NodeId& node1, const NodeId& node2) {
  return impl_->routing_table_.ConfirmGroupMembers(node1, node2);
}

void Routing::ReSendFindNodeRequest(const boost::system::error_code& error_code,
                                    std::weak_ptr<RoutingPrivate> impl,
                                    bool ignore_size) {
  if (error_code != boost::asio::error::operation_aborted) {
#ifndef LOCAL_TEST
    std::shared_ptr<RoutingPrivate> pimpl = impl.lock();
    if (!pimpl) {
      LOG(kVerbose) << "Ignoring message received since this node is shutting down";
      return;
    }

    if (pimpl->routing_table_.Size() == 0) {
      LOG(kError) << "This node's [" << HexSubstr(pimpl->keys_.identity)
                  << "] Routing table is empty."
                  << " Reconnecting .... !!!";
       std::async(std::launch::async, [=]
                  {
                  std::shared_ptr<RoutingPrivate> pimpl = impl.lock();
                  if (!pimpl) {
                  LOG(kVerbose) << "Not re bootstrapping since this node is shutting down";
                  return;
                  }

                  Sleep(boost::posix_time::seconds(10));
                  DoJoin(impl_->functors_);
                  });
    } else if (ignore_size ||
               (pimpl->routing_table_.Size() < Parameters::routing_table_size_threshold)) {
      if (!ignore_size)
        LOG(kInfo) << "This node's [" << DebugId(pimpl->kNodeId_)
                   << "] Routing table smaller than " << Parameters::routing_table_size_threshold
                   << " nodes.  Sending another FindNodes. Current routing table size : "
                   << pimpl->routing_table_.Size();
      else
        LOG(kInfo) << "This node's [" << DebugId(pimpl->kNodeId_) << "] close node lost."
                   << "Sending another FindNodes. Current routing table size : "
                   << pimpl->routing_table_.Size();

      int num_nodes_requested(0);
      if (ignore_size && (pimpl->routing_table_.Size() > Parameters::routing_table_size_threshold))
        num_nodes_requested = static_cast<int>(Parameters::closest_nodes_size);
      else
        num_nodes_requested = static_cast<int>(Parameters::max_routing_table_size);

      protobuf::Message find_node_rpc(rpcs::FindNodes(pimpl->kNodeId_,
                                                      pimpl->kNodeId_,
                                                      num_nodes_requested));
      pimpl->network_.SendToClosestNode(find_node_rpc);

      pimpl->recovery_timer_.expires_from_now(
          boost::posix_time::seconds(Parameters::recovery_timeout_in_seconds));
      std::weak_ptr<RoutingPrivate> impl_weak_ptr(pimpl);
      pimpl->recovery_timer_.async_wait([=](boost::system::error_code error_code_local) {
                                            ReSendFindNodeRequest(error_code_local, impl_weak_ptr);
                                          });
    }
#else
    (void)impl;
    (void)ignore_size;
#endif
  } else {
    LOG(kVerbose) << "Cancelled recovery loop!!";
  }
}

}  // namespace routing

}  // namespace maidsafe
