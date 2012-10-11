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

#include "maidsafe/routing/routing_private.h"

#include <cstdint>
#include <functional>
#include <future>
#include "boost/asio/deadline_timer.hpp"
#include "boost/asio/ip/udp.hpp"
#include "boost/filesystem/path.hpp"
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

RoutingPrivate::RoutingPrivate(const Fob& fob, bool client_mode)
    : functors_(),
      bootstrap_nodes_(),
      kFob_([&fob]()->Fob {
          if (fob.identity.IsInitialised())
            return fob;
          Fob fob_temp;
          fob_temp.keys = asymm::GenerateKeyPair();
          fob_temp.identity = Identity(RandomString(64));
          return fob_temp;
      }()),
      kNodeId_(NodeId(kFob_.identity)),
      tearing_down_(false),
      routing_table_(kFob_, client_mode),
      non_routing_table_(kFob_),  // TODO(Prakash) : don't create NRT for client nodes (wrap both)
      asio_service_(2),
      timer_(asio_service_),
      message_handler_(),
      network_(routing_table_, non_routing_table_, timer_),
      joined_(false),
      bootstrap_file_path_(),
      client_mode_(client_mode),
      anonymous_node_(false),
      random_node_queue_(),
      recovery_timer_(asio_service_.service()),
      setup_timer_(asio_service_.service()),
      re_bootstrap_timer_(asio_service_.service()),
      random_node_vector_(),
      random_node_mutex_() {
  asio_service_.Start();
  message_handler_.reset(new MessageHandler(routing_table_, non_routing_table_, network_, timer_));
  if (!CheckBootstrapFilePath())
    LOG(kInfo) << "No bootstrap nodes, require BootStrapFromThisEndpoint()";

  assert((client_mode || fob.identity.IsInitialised()) &&
         "Server Nodes cannot be created without valid keys");
  if (!fob.identity.IsInitialised()) {
    anonymous_node_ = true;
    LOG(kInfo) << "Anonymous node id: " << DebugId(kNodeId_)
               << ", connection id" << DebugId(routing_table_.kConnectionId());
  }
}

void RoutingPrivate::Stop() {
  tearing_down_ = true;
  setup_timer_.cancel();
  recovery_timer_.cancel();
  re_bootstrap_timer_.cancel();
  network_.Stop();
  asio_service_.Stop();
  DisconnectFunctors();
}

RoutingPrivate::~RoutingPrivate() {
  LOG(kVerbose) << "RoutingPrivate::~RoutingPrivate() " << DebugId(kNodeId_)
                << ", connection id" << DebugId(routing_table_.kConnectionId());
}

bool RoutingPrivate::CheckBootstrapFilePath()  {
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
      LOG(kWarning) << "bootstrap file is not a regular file " << local_file << ".  "
                    << is_regular_file_error_code.message();
    }
    return false;
  } else {
    LOG(kVerbose) << "Found bootstrap file at " << local_file;
    bootstrap_file_path_ = local_file;
    bootstrap_nodes_ = ReadBootstrapFile(bootstrap_file_path_);
    network_.set_bootstrap_file_path(bootstrap_file_path_);

    // Appending global_bootstrap_file's contents
    for (auto i: global_bootstrap_nodes)
      bootstrap_nodes_.push_back(i);
    return true;
  }
}

void RoutingPrivate::Join(Functors functors, std::vector<Endpoint> peer_endpoints) {
  ConnectFunctors(functors);
  if (!peer_endpoints.empty()) {
    return BootstrapFromTheseEndpoints(peer_endpoints);
  } else {
    LOG(kInfo) << "Doing a default join";
    if (CheckBootstrapFilePath()) {
      return DoJoin();
    } else {
      LOG(kError) << "Invalid Bootstrap Contacts";
      if (functors_.network_status)
        functors_.network_status(kInvalidBootstrapContacts);
      return;
    }
  }
}

void RoutingPrivate::ConnectFunctors(const Functors& functors) {
  routing_table_.set_remove_node_functor([this](const NodeInfo& node,
                                                const bool& internal_rudp_only) {
      RemoveNode(node, internal_rudp_only);
  });

  routing_table_.set_network_status_functor(functors.network_status);
  routing_table_.set_close_node_replaced_functor(functors.close_node_replaced);
  message_handler_->set_message_received_functor(functors.message_received);
  message_handler_->set_request_public_key_functor(functors.request_public_key);
  functors_ = functors;
}

void RoutingPrivate::DisconnectFunctors() {
  routing_table_.set_remove_node_functor(nullptr);
  routing_table_.set_network_status_functor(nullptr);
  routing_table_.set_close_node_replaced_functor(nullptr);
  message_handler_->set_message_received_functor(nullptr);
  message_handler_->set_request_public_key_functor(nullptr);
  functors_ = Functors();
}

void RoutingPrivate::BootstrapFromTheseEndpoints(const std::vector<Endpoint>& endpoints) {
  LOG(kInfo) << "Doing a BootstrapFromThisEndpoint Join.  Entered first bootstrap endpoint: "
             << endpoints[0]
             << ", this node's ID: " << DebugId(kNodeId_)
             << (client_mode_ ? " Client" : "");
  if (routing_table_.Size() > 0) {
    for (uint16_t i = 0; i < routing_table_.Size(); ++i) {
      NodeInfo remove_node =
          routing_table_.GetClosestNode(kNodeId_);
      network_.Remove(remove_node.connection_id);
      routing_table_.DropNode(remove_node.node_id, true);
    }
    if (functors_.network_status)
      functors_.network_status(static_cast<int>(routing_table_.Size()));
  }
  bootstrap_nodes_.clear();
  bootstrap_nodes_.insert(bootstrap_nodes_.begin(),
                          endpoints.begin(),
                          endpoints.end());
  DoJoin();
}

void RoutingPrivate::DoJoin() {
  int return_value(DoBootstrap());
  if (kSuccess != return_value) {
    if (functors_.network_status)
      functors_.network_status(return_value);
    return;
  }
  assert(!network_.bootstrap_connection_id().IsZero() &&
         "Bootstrap connection id must be populated by now.");
  if (anonymous_node_) {  // No need to do find node for anonymous node
    if (functors_.network_status)
      functors_.network_status(return_value);
    return;
  }

  assert(!anonymous_node_&& "Not allowed for anonymous nodes");
  FindClosestNode(boost::system::error_code(), 0);

  if (functors_.network_status)
    functors_.network_status(return_value);
}

int RoutingPrivate::DoBootstrap() {
  if (bootstrap_nodes_.empty()) {
    LOG(kError) << "No bootstrap nodes.  Aborted join.";
    return kInvalidBootstrapContacts;
  }
  // FIXME race condition if a new connection appears at rudp -- rudp should handle this
  assert(routing_table_.Size() == 0);
  recovery_timer_.cancel();
  setup_timer_.cancel();
  if (!network_.bootstrap_connection_id().IsZero()) {
    LOG(kInfo) << "Removing bootstrap connection to rebootstrap. Connection id : "
               << DebugId(network_.bootstrap_connection_id());
    network_.Remove(network_.bootstrap_connection_id());
    network_.clear_bootstrap_connection_info();
  }

  return network_.Bootstrap(
      bootstrap_nodes_,
      [=](const std::string& message) { OnMessageReceived(message); },
      [=](const NodeId& lost_connection_id) { OnConnectionLost(lost_connection_id);});  // NOLINT
}

void RoutingPrivate::FindClosestNode(const boost::system::error_code& error_code,
                                     int attempts) {
  if (error_code != boost::asio::error::operation_aborted && !tearing_down_) {
    assert(!anonymous_node_ && "Not allowed for anonymous nodes");
    if (attempts == 0) {
      assert(!network_.bootstrap_connection_id().IsZero() && "Only after bootstraping succeeds");
      assert(!network_.this_node_relay_connection_id().IsZero() &&
             "Relay connection id should be set after bootstraping succeeds");
    } else {
      if (routing_table_.Size() > 0) {
        // Exit the loop & start recovery loop
        LOG(kInfo) << "Added a node in routing table."
                   << " Terminating setup loop & Scheduling recovery loop.";
        recovery_timer_.expires_from_now(Parameters::recovery_timeout);
        recovery_timer_.async_wait([=](const boost::system::error_code& error_code) {
                                       ReSendFindNodeRequest(error_code);
                                     });
        return;
      }

      if (attempts >= 10) {
        LOG(kError) << "This node's [" << HexSubstr(kFob_.identity)
                    << "] failed to get closest node."
                    << " Reconnecting ....";
        // TODO(Prakash) : Remove the bootstrap node from the list
        ReBootstrap();
      }
    }

    protobuf::Message find_node_rpc(
        rpcs::FindNodes(kNodeId_,
                        kNodeId_,
                        1,
                        true,
                        network_.this_node_relay_connection_id()));

    rudp::MessageSentFunctor message_sent_functor(
        [=](int message_sent) {
          if (message_sent == kSuccess)
            LOG(kInfo) << "Successfully sent FindNodes RPC to bootstrap connection id : "
                       << DebugId(network_.bootstrap_connection_id());
          else
            LOG(kError) << "Failed to send FindNodes RPC to bootstrap connection id : "
                        << DebugId(network_.bootstrap_connection_id());
        });

    ++attempts;
    network_.SendToDirect(find_node_rpc, network_.bootstrap_connection_id(),
                          message_sent_functor);
    setup_timer_.expires_from_now(Parameters::setup_timeout);
    setup_timer_.async_wait([=](boost::system::error_code error_code_local) {
                                FindClosestNode(error_code_local, attempts);
                              });
  }
}

int RoutingPrivate::ZeroStateJoin(Functors functors,
                                  const Endpoint& local_endpoint,
                                  const Endpoint& peer_endpoint,
                                  const NodeInfo& peer_node) {
  assert((!client_mode_) && "no client nodes allowed in zero state network");
  assert((!anonymous_node_) && "not allowed on anonymous node");
  bootstrap_nodes_.clear();
  bootstrap_nodes_.push_back(peer_endpoint);
  if (bootstrap_nodes_.empty()) {
    LOG(kError) << "No bootstrap nodes.  Aborted join.";
    return kInvalidBootstrapContacts;
  }

  ConnectFunctors(functors);
  int result(network_.Bootstrap(
      bootstrap_nodes_,
      [=](const std::string& message) { OnMessageReceived(message); },
      [=](const NodeId& lost_connection_id) {
          OnConnectionLost(lost_connection_id);
        },
      local_endpoint));

  if (result != kSuccess) {
    LOG(kError) << "Could not bootstrap zero state node from local endpoint : "
                << local_endpoint << " with peer endpoint : " << peer_endpoint;
    return result;
  }

  LOG(kInfo) << "This Node [" << DebugId(kNodeId_) << "]"
             << "bootstrap connection id : "
             << DebugId(network_.bootstrap_connection_id());

  assert(!peer_node.node_id.IsZero() && "empty nodeid passed");
  assert((network_.bootstrap_connection_id() == peer_node.node_id) &&
         "Should bootstrap only with known peer for zero state network");
  LOG(kVerbose) << local_endpoint << " Bootstrapped with remote endpoint " << peer_endpoint;
  rudp::NatType nat_type(rudp::NatType::kUnknown);
  rudp::EndpointPair peer_endpoint_pair;  // zero state nodes must be directly connected endpoint
  rudp::EndpointPair this_endpoint_pair;
  peer_endpoint_pair.external = peer_endpoint_pair.local = peer_endpoint;
  this_endpoint_pair.external = this_endpoint_pair.local = local_endpoint;
  Sleep(boost::posix_time::milliseconds(100));  // FIXME avoiding assert in rudp
  result = network_.GetAvailableEndpoint(peer_node.node_id, peer_endpoint_pair,
                                         this_endpoint_pair, nat_type);
  if (result != kSuccess) {
    LOG(kError) << "Failed to get available endpoint to add zero state node : " << peer_endpoint;
    return result;
  }

  result = network_.Add(peer_node.node_id, peer_endpoint_pair, "invalid");
  if (result != kSuccess) {
    LOG(kError) << "Failed to add zero state node : " << peer_endpoint;
    return result;
  }

  ValidateAndAddToRoutingTable(network_,
                               routing_table_,
                               non_routing_table_,
                               peer_node.node_id,
                               peer_node.node_id,
                               peer_node.public_key,
                               false);
  // Now poll for routing table size to have other zero state peer.
  uint8_t poll_count(0);
  do {
    Sleep(boost::posix_time::milliseconds(100));
  } while ((routing_table_.Size() == 0) && (++poll_count < 50));
  if (routing_table_.Size() != 0) {
    LOG(kInfo) << "Node Successfully joined zero state network, with "
               << DebugId(network_.bootstrap_connection_id())
               << ", Routing table size - " << routing_table_.Size()
               << ", Node id : " << DebugId(kNodeId_);

    recovery_timer_.expires_from_now(Parameters::recovery_timeout);
    recovery_timer_.async_wait([=](const boost::system::error_code& error_code) {
                                   ReSendFindNodeRequest(error_code);
                                 });
    return kSuccess;
  } else {
    LOG(kError) << "Failed to join zero state network, with bootstrap_endpoint "
                << peer_endpoint;
    return kNotJoined;
  }
}

void RoutingPrivate::Send(const NodeId& destination_id,
                          const NodeId& group_claim,
                          const std::string& data,
                          ResponseFunctor response_functor,
                          const boost::posix_time::time_duration& timeout,
                          bool direct,
                          bool cache) {
  if (destination_id.IsZero()) {
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
  proto_message.set_destination_id(destination_id.string());
  proto_message.set_routing_message(false);
  proto_message.add_data(data);
  proto_message.set_type(static_cast<int32_t>(MessageType::kNodeLevel));
  proto_message.set_cacheable(cache);
  proto_message.set_direct(direct);
  proto_message.set_client_node(client_mode_);
  proto_message.set_request(true);
  proto_message.set_hops_to_live(Parameters::hops_to_live);
  uint16_t replication(1);
  if (!group_claim.IsZero())
    proto_message.set_group_claim(group_claim.string());

  if (!direct) {
    replication = Parameters::node_group_size;
    if (response_functor)
      proto_message.set_id(timer_.AddTask(timeout, response_functor,
                                          Parameters::node_group_size));
  } else {
    if (response_functor)
      proto_message.set_id(timer_.AddTask(timeout, response_functor, 1));
  }

  proto_message.set_replication(replication);
  // Anonymous node /Partial join state
  if (anonymous_node_ || (routing_table_.Size() == 0)) {
    proto_message.set_relay_id(kNodeId_.string());
    proto_message.set_relay_connection_id(network_.this_node_relay_connection_id().string());
    NodeId bootstrap_connection_id(network_.bootstrap_connection_id());
    assert(proto_message.has_relay_connection_id() && "did not set this_node_relay_connection_id");
    rudp::MessageSentFunctor message_sent(
        [=](int result) {
          asio_service_.service().post([=]() {
              if (rudp::kSuccess != result) {
                timer_.CancelTask(proto_message.id());
                if (anonymous_node_) {
                  LOG(kError) << "Anonymous Session Ended, Send not allowed anymore";
                  functors_.network_status(kAnonymousSessionEnded);
                } else {
                  LOG(kError) << "Partial join Session Ended, Send not allowed anymore";
                  if (functors_.network_status)  // FIXME do we need this?
                  functors_.network_status(kPartialJoinSessionEnded);
                }
              } else {
                LOG(kInfo) << "Message Sent from Anonymous/Partial joined node";
              }
            });
          });
    network_.SendToDirect(proto_message, bootstrap_connection_id, message_sent);
    return;
  }

  // Non Anonymous, normal node
  proto_message.set_source_id(kNodeId_.string());

  if (kNodeId_ != destination_id) {
    network_.SendToClosestNode(proto_message);
  } else if (client_mode_) {
    LOG(kVerbose) << "Client sending request to self id";
    network_.SendToClosestNode(proto_message);
  } else {
    LOG(kInfo) << "Sending request to self";
    OnMessageReceived(proto_message.SerializeAsString());
  }
}

void RoutingPrivate::OnMessageReceived(const std::string& message) {
  if (!tearing_down_)
    asio_service_.service().post([=]() { DoOnMessageReceived(message); });  // NOLINT (Fraser)
}

void RoutingPrivate::DoOnMessageReceived(const std::string& message) {
  protobuf::Message protobuf_message;
  if (protobuf_message.ParseFromString(message)) {
    bool relay_message(!protobuf_message.has_source_id());
    LOG(kInfo) << "This node [" << DebugId(kNodeId_) << "] received message type: "
               << MessageTypeString(protobuf_message) << " from "
               << (relay_message ? HexSubstr(protobuf_message.relay_id()) + " -- RELAY REQUEST" :
                                   HexSubstr(protobuf_message.source_id()))
               << " id: " << protobuf_message.id();
    if (protobuf_message.has_source_id())
      AddExistingRandomNode(NodeId(protobuf_message.source_id()));
    message_handler_->HandleMessage(protobuf_message);
  } else {
    LOG(kWarning) << "Message received, failed to parse";
  }
}

NodeId RoutingPrivate::GetRandomExistingNode() {
  std::lock_guard<std::mutex> lock(random_node_mutex_);
  if (random_node_vector_.empty())
    return NodeId();
  NodeId node;
  auto queue_size = random_node_vector_.size();
  node = (queue_size >= 100) ? (random_node_vector_[0]) :
                               (random_node_vector_[RandomUint32() % queue_size]);
  LOG(kVerbose) << "RandomNodeQueue : Getting node, queue size now "
                << queue_size;
  if (queue_size >= 100) {
    random_node_vector_.erase(random_node_vector_.begin());
  }
  return node;
}

void RoutingPrivate::AddExistingRandomNode(NodeId node) {
  if (!node.IsZero()) {
    std::lock_guard<std::mutex> lock(random_node_mutex_);
    if (std::find_if(random_node_vector_.begin(), random_node_vector_.end(),
                   [node] (const NodeId& vect_node) {
                       return vect_node == node;
                     }) !=  random_node_vector_.end())
      return;
    random_node_vector_.push_back(node);
    auto queue_size = random_node_vector_.size();
    LOG(kVerbose) << "RandomNodeQueue : Added node, queue size now "
                  << queue_size;
    if (queue_size > 100)
      random_node_vector_.erase(random_node_vector_.begin());
  }
}

void RoutingPrivate::OnConnectionLost(const NodeId& lost_connection_id) {
  if (!tearing_down_)
    asio_service_.service().post([=]() { DoOnConnectionLost(lost_connection_id); });  // NOLINT (Fraser)
}

void RoutingPrivate::DoOnConnectionLost(const NodeId& lost_connection_id) {
  LOG(kVerbose) << "Routing::ConnectionLost with ----------------------------"
                << DebugId(lost_connection_id);
  NodeInfo dropped_node;
  bool resend(!tearing_down_ &&
              (routing_table_.GetNodeInfo(lost_connection_id, dropped_node) &&
               routing_table_.IsThisNodeInRange(dropped_node.node_id,
                                                Parameters::closest_nodes_size)));

  // Checking routing table
  dropped_node = routing_table_.DropNode(lost_connection_id, true);
  if (!dropped_node.node_id.IsZero()) {
    LOG(kWarning) << "[" << HexSubstr(kFob_.identity) << "]"
                  << "Lost connection with routing node " << DebugId(dropped_node.node_id);
  }

  // Checking non-routing table
  if (dropped_node.node_id.IsZero()) {
    resend = false;
    dropped_node = non_routing_table_.DropConnection(lost_connection_id);
    if (!dropped_node.node_id.IsZero()) {
      LOG(kWarning) << "[" << HexSubstr(kFob_.identity) << "]"
                    << "Lost connection with non-routing node "
                    << HexSubstr(dropped_node.node_id.string());
    } else if (!network_.bootstrap_connection_id().IsZero() &&
               lost_connection_id == network_.bootstrap_connection_id()) {
      LOG(kWarning) << "[" << HexSubstr(kFob_.identity) << "]"
                    << "Lost temporary connection with bootstrap node. connection id :"
                    << DebugId(lost_connection_id);
      network_.clear_bootstrap_connection_info();
      if (anonymous_node_) {
        LOG(kError) << "Anonymous Session Ended, Send not allowed anymore";
        functors_.network_status(kAnonymousSessionEnded);
        // TODO(Prakash) cancel all pending tasks
        return;
      }

      if (routing_table_.Size() == 0)
        resend = true;  // This will trigger rebootstrap
    } else {
      LOG(kWarning) << "[" << HexSubstr(kFob_.identity) << "]"
                    << "Lost connection with unknown/internal connection id "
                    << DebugId(lost_connection_id);
    }
  }

  if (resend) {
    // Close node lost, get more nodes
    LOG(kWarning) << "Lost close node, getting more.";
    ReSendFindNodeRequest(boost::system::error_code(), true);
  }
}

void RoutingPrivate::RemoveNode(const NodeInfo& node, const bool& internal_rudp_only) {
  if (node.connection_id.IsZero() || node.node_id.IsZero()) {
    return;
  }
  network_.Remove(node.connection_id);
  if (internal_rudp_only) {  // No recovery
    LOG(kInfo) << "Routing: removed node : " << DebugId(node.node_id)
               << ". Removed internal rudp connection id : " << DebugId(node.connection_id);
    return;
  } else {
      LOG(kInfo) << "Routing: removed node : " << DebugId(node.node_id)
                 << ". Removed rudp connection id : " << DebugId(node.connection_id);
  }

  LOG(kInfo) << "Routing: removed node : " << DebugId(node.node_id)
             << ". Removed rudp connection id : " << DebugId(node.connection_id);

  // TODO(Prakash): Handle pseudo connection removal here and NRT node removal

  bool resend(routing_table_.IsThisNodeInRange(node.node_id, Parameters::closest_nodes_size));
  if (resend) {
    // Close node removed by routing, get more nodes
    LOG(kWarning) << "Removed close node, sending find node to get more nodes.";
    ReSendFindNodeRequest(boost::system::error_code(), true);
  }
}

bool RoutingPrivate::ConfirmGroupMembers(const NodeId& node1, const NodeId& node2) {
  return routing_table_.ConfirmGroupMembers(node1, node2);
}

void RoutingPrivate::ReSendFindNodeRequest(const boost::system::error_code& error_code,
                                           bool ignore_size) {
  if ((error_code != boost::asio::error::operation_aborted) && !tearing_down_) {
    if (routing_table_.Size() == 0) {
      LOG(kError) << "This node's [" << HexSubstr(kFob_.identity)
                  << "] Routing table is empty."
                  << " Scheduling Re-Bootstrap .... !!!";
      ReBootstrap();
      return;
    } else if (ignore_size ||
               (routing_table_.Size() < Parameters::routing_table_size_threshold)) {
      if (!ignore_size)
        LOG(kInfo) << "This node's [" << DebugId(kNodeId_)
                   << "] Routing table smaller than " << Parameters::routing_table_size_threshold
                   << " nodes.  Sending another FindNodes. Routing table size < "
                   << routing_table_.Size() << " >";
      else
        LOG(kInfo) << "This node's [" << DebugId(kNodeId_) << "] close node lost."
                   << "Sending another FindNodes. Current routing table size : "
                   << routing_table_.Size();

      int num_nodes_requested(0);
      if (ignore_size && (routing_table_.Size() > Parameters::routing_table_size_threshold))
        num_nodes_requested = static_cast<int>(Parameters::closest_nodes_size);
      else
        num_nodes_requested = static_cast<int>(Parameters::max_routing_table_size);

      protobuf::Message find_node_rpc(rpcs::FindNodes(kNodeId_, kNodeId_, num_nodes_requested));
      network_.SendToClosestNode(find_node_rpc);

      recovery_timer_.expires_from_now(Parameters::recovery_timeout);
      recovery_timer_.async_wait([=](boost::system::error_code error_code_local) {
                                     ReSendFindNodeRequest(error_code_local);
                                   });
    }
  } else {
    LOG(kVerbose) << "Cancelled recovery loop!!";
  }
}

void RoutingPrivate::ReBootstrap() {
  re_bootstrap_timer_.expires_from_now(Parameters::re_bootstrap_timeout);
  if (!tearing_down_)
    re_bootstrap_timer_.async_wait([=](boost::system::error_code error_code_local) {
                                       DoReBootstrap(error_code_local);
                                     });
}

void RoutingPrivate::DoReBootstrap(const boost::system::error_code& error_code) {
  if (error_code != boost::asio::error::operation_aborted && !tearing_down_) {
    LOG(kError) << "This node's [" << HexSubstr(kFob_.identity)
                << "] Routing table is empty."
                << " Reconnecting .... !!!";
    DoJoin();
  }
}

}  // namespace routing

}  // namespace maidsafe
