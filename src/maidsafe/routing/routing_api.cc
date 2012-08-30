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

#include "boost/asio/deadline_timer.hpp"
#include "boost/thread/future.hpp"

#include "maidsafe/common/log.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/bootstrap_file_handler.h"
#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/node_id.h"
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

  if (!client_mode)
    assert(!keys.identity.empty() && "Server Nodes cannot be created without valid keys");

  if (keys.identity.empty()) {
    impl_->anonymous_node_ = true;
    LOG(kInfo) << "Anonymous node ID: " << HexSubstr(impl_->keys_.identity);
  }
}

Routing::~Routing() {
  LOG(kVerbose) << "~Routing() - Disconnecting functors.  Routing table Size: "
                << impl_->routing_table_.Size();
  DisconnectFunctors();
}

// TODO(Prakash) For client nodes, copy bootstrap file from sys dir if it's not available @ user dir
bool Routing::CheckBootstrapFilePath() const {
  LOG(kVerbose) << "Application Path " << GetAppInstallDir();
  LOG(kVerbose) << "System Path " << GetSystemAppSupportDir();
  fs::path path;
  fs::path global_bootstrap_file_path(GetSystemAppSupportDir() / "bootstrap-global.dat");

  // Global bootstrap file
  std::vector<Endpoint> global_bootstrap_nodes;
  boost::system::error_code exists_error_code, is_regular_file_error_code;
  if (!fs::exists(global_bootstrap_file_path, exists_error_code) ||
      !fs::is_regular_file(global_bootstrap_file_path, is_regular_file_error_code) ||
      exists_error_code || is_regular_file_error_code) {
    if (exists_error_code) {
      LOG(kWarning) << "Failed to find global bootstrap file at "
                    << global_bootstrap_file_path << ".  " << exists_error_code.message();
    }
    if (is_regular_file_error_code) {
      LOG(kWarning) << "Failed to find global bootstrap file at " << path << ".  "
                    << is_regular_file_error_code.message();
    }
  } else {
    global_bootstrap_nodes = ReadBootstrapFile(global_bootstrap_file_path);
  }

  std::string file_name;
  if (impl_->client_mode_) {
    file_name = "bootstrap";
    path = GetUserAppDir() / file_name;
  } else {
    std::string file_id(
        EncodeToBase32(maidsafe::crypto::Hash<maidsafe::crypto::SHA1>(impl_->keys_.identity)));
    file_name = "bootstrap-" + file_id + ".dat";
    path = GetSystemAppSupportDir() / file_name;
  }
  fs::path local_file = fs::current_path() / file_name;

  if (!fs::exists(local_file, exists_error_code) ||
      !fs::is_regular_file(local_file, is_regular_file_error_code) ||
      exists_error_code || is_regular_file_error_code) {
    if (exists_error_code) {
      LOG(kError) << "Failed to find bootstrap file at " << local_file << ".  "
                  << exists_error_code.message();
    }
    if (is_regular_file_error_code) {
      LOG(kError) << "Failed to check for bootstrap file at " << local_file << ".  "
                  << is_regular_file_error_code.message();
    }
  } else {
    LOG(kVerbose) << "Found bootstrap file at " << local_file;
    impl_->bootstrap_file_path_ = local_file;
    impl_->bootstrap_nodes_ = ReadBootstrapFile(impl_->bootstrap_file_path_);
    impl_->routing_table_.set_bootstrap_file_path(impl_->bootstrap_file_path_);

    // Appending global_bootstrap_file's contents
    for (auto i: global_bootstrap_nodes)
      impl_->bootstrap_nodes_.push_back(i);
    return true;
  }

  if (!fs::exists(path, exists_error_code) ||
      !fs::is_regular_file(path, is_regular_file_error_code) ||
      exists_error_code || is_regular_file_error_code) {
    if (exists_error_code) {
      LOG(kError) << "Failed to check for bootstrap file at " << path << ".  "
                  << exists_error_code.message();
    }
    if (is_regular_file_error_code) {
      LOG(kError) << "Failed to check for bootstrap file at " << path << ".  "
                  << is_regular_file_error_code.message();
    }
    return false;
  } else {
    LOG(kVerbose) << "Found bootstrap file at " << path;
    impl_->bootstrap_file_path_ = path;
    impl_->bootstrap_nodes_ = ReadBootstrapFile(impl_->bootstrap_file_path_);
    impl_->routing_table_.set_bootstrap_file_path(impl_->bootstrap_file_path_);

    // Appending global_bootstrap_file's contents
    for (auto i: global_bootstrap_nodes)
      impl_->bootstrap_nodes_.push_back(i);
    return true;
  }
}

void Routing::Join(Functors functors, Endpoint peer_endpoint) {
#ifdef LOCAL_TEST
  LOG(kInfo) << "RoutingPrivate::bootstraps_.size(): " << RoutingPrivate::bootstraps_.size();
  if (!RoutingPrivate::bootstraps_.empty()) {
#else
  if (!peer_endpoint.address().is_unspecified()) {
#endif
    return BootstrapFromThisEndpoint(functors, peer_endpoint);
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
  impl_->routing_table_.set_network_status_functor(functors.network_status);
  impl_->routing_table_.set_close_node_replaced_functor(functors.close_node_replaced);
  impl_->message_handler_->set_message_received_functor(functors.message_received);
  impl_->message_handler_->set_request_public_key_functor(functors.request_public_key);
  impl_->functors_ = functors;
}

void Routing::DisconnectFunctors() {  // TODO(Prakash) : fix race condition when functors in use
  impl_->routing_table_.set_network_status_functor(nullptr);
  impl_->routing_table_.set_close_node_replaced_functor(nullptr);
  impl_->message_handler_->set_message_received_functor(nullptr);
  impl_->message_handler_->set_request_public_key_functor(nullptr);
  impl_->functors_ = Functors();
}

#ifdef LOCAL_TEST
void Routing::BootstrapFromThisEndpoint(const Functors& functors, const Endpoint& ) {
#else
void Routing::BootstrapFromThisEndpoint(const Functors& functors, const Endpoint& endpoint) {
#endif
  LOG(kInfo) << "Doing a BootstrapFromThisEndpoint Join.  Entered bootstrap endpoint: "
//             << RoutingPrivate::bootstraps_[0]
             << ", this node's ID: " << HexSubstr(impl_->keys_.identity)
             << (impl_->client_mode_ ? " Client" : "");
  if (impl_->routing_table_.Size() > 0) {
    DisconnectFunctors();  // TODO(Prakash): Do we need this ?
    for (uint16_t i = 0; i < impl_->routing_table_.Size(); ++i) {
      NodeInfo remove_node =
          impl_->routing_table_.GetClosestNode(NodeId(impl_->routing_table_.kKeys().identity));
      impl_->network_.Remove(remove_node.endpoint);
      impl_->routing_table_.DropNode(remove_node.endpoint);
    }
    if (impl_->functors_.network_status)
      impl_->functors_.network_status(impl_->routing_table_.Size());
  }
  impl_->bootstrap_nodes_.clear();
#ifdef LOCAL_TEST
  uint16_t start_index(0);
  start_index = (RoutingPrivate::bootstraps_.size() > 2) ? 2 : 0;
  if (start_index == 0) {
    impl_->bootstrap_nodes_.insert(impl_->bootstrap_nodes_.begin(),
                                   RoutingPrivate::bootstraps_.begin() + start_index,
                                   RoutingPrivate::bootstraps_.end());
  } else {
    uint16_t random(RandomUint32() % (RoutingPrivate::bootstraps_.size() - 2));
    for (auto index(0); index < (RoutingPrivate::bootstraps_.size() - 2); ++index) {
      impl_->bootstrap_nodes_.push_back(
          RoutingPrivate::bootstraps_[(random + index) %
              (RoutingPrivate::bootstraps_.size() - 2) + 2]);
    }
  }
  for (auto endpoint : impl_->bootstrap_nodes_)
    LOG(kVerbose) << "Static ep: " << endpoint;
#else
  impl_->bootstrap_nodes_.push_back(endpoint);
#endif
  DoJoin(functors);
}

void Routing::DoJoin(const Functors& functors) {
  int return_value(DoBootstrap(functors));
  if (kSuccess != return_value) {
    if (functors.network_status)
      functors.network_status(return_value);
    return;
  }

  if (impl_->anonymous_node_) {  // No need to do find value for anonymous node
    if (functors.network_status)
      functors.network_status(return_value);
    return;
  }

  if (functors.network_status)
    functors.network_status(DoFindNode());

  impl_->recovery_timer_.expires_from_now(boost::posix_time::seconds(
      Parameters::recovery_timeout_in_seconds));
  impl_->recovery_timer_.async_wait([=](const boost::system::error_code& error_code) {
                                        ReSendFindNodeRequest(error_code);
                                      });
}

int Routing::DoBootstrap(const Functors& functors) {
  if (impl_->bootstrap_nodes_.empty()) {
    LOG(kError) << "No bootstrap nodes.  Aborted join.";
    return kInvalidBootstrapContacts;
  }
  ConnectFunctors(functors);
  std::weak_ptr<RoutingPrivate> impl_weak_ptr(impl_);
  return impl_->network_.Bootstrap(
      impl_->bootstrap_nodes_,
      [=](const std::string& message) { ReceiveMessage(message, impl_weak_ptr); },
      [=](const Endpoint& lost_endpoint) { ConnectionLost(lost_endpoint, impl_weak_ptr); });  // NOLINT (Fraser)
}

int Routing::DoFindNode() {
  protobuf::Message find_node_rpc(
      rpcs::FindNodes(NodeId(impl_->keys_.identity),
                      NodeId(impl_->keys_.identity),
                      true,
                      impl_->network_.this_node_relay_endpoint()));

  boost::promise<bool> message_sent_promise;
  auto message_sent_future = message_sent_promise.get_future();
  rudp::MessageSentFunctor message_sent_functor(
      [&message_sent_promise](int message_sent) {
        message_sent_promise.set_value(rudp::kSuccess == message_sent);
      });

  impl_->network_.SendToDirectEndpoint(find_node_rpc, impl_->network_.bootstrap_endpoint(),
                                       message_sent_functor);

  if (!message_sent_future.timed_wait(boost::posix_time::seconds(10))) {
    LOG(kError) << "Unable to send FindNodes RPC to bootstrap endpoint "
                << impl_->network_.bootstrap_endpoint();
    return kFailedtoSendFindNode;
  } else {
    LOG(kInfo) << "Successfully sent FindNodes RPC to bootstrap endpoint "
               << impl_->network_.bootstrap_endpoint();
    return kSuccess;
  }
}

int Routing::ZeroStateJoin(Functors functors,
                           const Endpoint& local_endpoint,
                           const NodeInfo& peer_node) {
  assert((!impl_->client_mode_) && "no client nodes allowed in zero state network");
  assert((!impl_->anonymous_node_) && "not allwed on anonymous node");
  impl_->bootstrap_nodes_.clear();
  impl_->bootstrap_nodes_.push_back(peer_node.endpoint);
  if (impl_->bootstrap_nodes_.empty()) {
    LOG(kError) << "No bootstrap nodes.  Aborted join.";
    return kInvalidBootstrapContacts;
  }

  ConnectFunctors(functors);
  std::weak_ptr<RoutingPrivate> impl_weak_ptr(impl_);
  int result(impl_->network_.Bootstrap(
      impl_->bootstrap_nodes_,
      [=](const std::string& message) { ReceiveMessage(message, impl_weak_ptr); },
      [=](const Endpoint& lost_endpoint) { ConnectionLost(lost_endpoint, impl_weak_ptr); },
      local_endpoint));

  if (result != kSuccess) {
    LOG(kError) << "Could not bootstrap zero state node with " << peer_node.endpoint;
    return result;
  }

  assert((impl_->network_.bootstrap_endpoint() == peer_node.endpoint) &&
         "This should be only used in zero state network");
  LOG(kVerbose) << local_endpoint << " Bootstrapped with remote endpoint " << peer_node.endpoint;

  rudp::EndpointPair peer_endpoint_pair;  // zero state nodes must be directly connected endpoint
  rudp::EndpointPair this_endpoint_pair;
  peer_endpoint_pair.external = peer_node.endpoint;
  this_endpoint_pair.external = local_endpoint;
  LOG(kInfo) << "Attempting to add bootstrap endpoint us -> " << local_endpoint << " peer -> "
             << peer_node.endpoint;
  result = impl_->network_.Add(local_endpoint, peer_node.endpoint, "junk");
  if (result != kSuccess) {
    LOG(kError) << "Failed to add zero state node : " << peer_node.endpoint;
    return result;
  } else {
    LOG(kInfo) << " Added zero state node ->" << peer_node.endpoint;
  }

  ValidateAndAddToRoutingTable(impl_->network_,
                               impl_->routing_table_,
                               impl_->non_routing_table_,
                               NodeId(peer_node.node_id),
                               peer_node.public_key,
                               peer_endpoint_pair.external,
                               false);
  // Now poll for routing table size to have at other node available
  uint8_t poll_count(0);
  do {
    Sleep(boost::posix_time::milliseconds(100));
  } while ((impl_->routing_table_.Size() == 0) && (++poll_count < 50));
  if (impl_->routing_table_.Size() != 0) {
    LOG(kInfo) << "Node Successfully joined zero state network, with "
               << impl_->network_.bootstrap_endpoint()
               << ", Routing table size - " << impl_->routing_table_.Size()
               << ", Node id : " << HexSubstr(impl_->keys_.identity);

    impl_->recovery_timer_.expires_from_now(
        boost::posix_time::seconds(Parameters::recovery_timeout_in_seconds));
    impl_->recovery_timer_.async_wait([=](const boost::system::error_code& error_code) {
                                          ReSendFindNodeRequest(error_code);
                                       });
#ifdef LOCAL_TEST
    std::lock_guard<std::mutex> lock(RoutingPrivate::mutex_);
    RoutingPrivate::bootstraps_.push_back(local_endpoint);
    RoutingPrivate::bootstrap_nodes_id_.insert(NodeId(impl_->keys_.identity));
#endif
    return kSuccess;
  } else {
    LOG(kError) << "Failed to join zero state network, with bootstrap_endpoint "
                << impl_->network_.bootstrap_endpoint();
    return kNotJoined;
  }
}

int Routing::GetStatus() const {
  if (impl_->routing_table_.Size() == 0) {
    rudp::EndpointPair endpoint;
    rudp::NatType this_nat_type;
    int status = impl_->network_.GetAvailableEndpoint(Endpoint(),
                                                      endpoint, this_nat_type);
    if (rudp::kSuccess != status) {
      if (status == rudp::kNotBootstrapped)
        return kNotJoined;
    }
  } else {
    return impl_->routing_table_.Size();
  }
  return kSuccess;
}

void Routing::Send(const NodeId& destination_id,
                   const NodeId& group_claim,
                   const std::string& data,
                   ResponseFunctor response_functor,
                   const boost::posix_time::time_duration& timeout,
                   bool direct,
                   bool cache) {
  if (destination_id.String().empty()) {
    LOG(kError) << "No destination ID, aborted send";
    if (response_functor)
      response_functor(std::vector<std::string>());
    return;
  }

  if (data.size() > Parameters::max_data_size) {
    LOG(kError) << "Data size not allowed";
    if (response_functor)
      response_functor(std::vector<std::string>());
    return;
  }

  if (data.empty()) {
    LOG(kError) << "No data, aborted send";
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
  // Anonymous node
  if (impl_->anonymous_node_) {
    proto_message.set_relay_id(impl_->routing_table_.kKeys().identity);
    SetProtobufEndpoint(impl_->network_.this_node_relay_endpoint(), proto_message.mutable_relay());
    Endpoint bootstrap_endpoint = impl_->network_.bootstrap_endpoint();
    assert(proto_message.has_relay() && "did not set endpoint");
    assert((impl_->network_.this_node_relay_endpoint() ==
            GetEndpointFromProtobuf(proto_message.relay())) && "Endpoint was not set properly");
    rudp::MessageSentFunctor message_sent(
        [&](int result) {
          if (rudp::kSuccess != result) {
            impl_->timer_.CancelTask(proto_message.id());
            LOG(kError) << "Anonymous Session Ended, Send not allowed anymore";
          } else {
            LOG(kInfo) << "Message Sent from Anonymous node";
          }
        });

    impl_->network_.SendToDirectEndpoint(proto_message, bootstrap_endpoint, message_sent);
    return;
  }

  // Non Anonymous, normal node
  proto_message.set_source_id(impl_->routing_table_.kKeys().identity);

  if (impl_->routing_table_.kKeys().identity != destination_id.String()) {
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
  protobuf::ConnectRequest connection_request;
  if (protobuf_message.ParseFromString(message)) {
    bool relay_message(!protobuf_message.has_source_id());
    LOG(kInfo) << "This node [" << HexSubstr(pimpl->keys_.identity) << "] received message type: "
               << MessageTypeString(protobuf_message) << " from "
               << (relay_message ? HexSubstr(protobuf_message.relay_id()) + " -- RELAY REQUEST" :
                                   HexSubstr(protobuf_message.source_id()))
                << " id: " << protobuf_message.id();
    if (protobuf_message.has_source_id())
      AddExistingRandomNode(NodeId(protobuf_message.source_id()));
    pimpl->message_handler_->HandleMessage(protobuf_message);
  } else {
    LOG(kWarning) << "Message received, failed to parse";
  }
#ifdef LOCAL_TEST
  pimpl->LocalTestUtility(protobuf_message);
#endif
}

NodeId Routing::GetRandomExistingNode() {
  NodeId node;
  impl_->random_node_queue_.TryPop(node);
  LOG(kVerbose) << "RandomNodeQueue : Getting node, queue size now "
                << impl_->random_node_queue_.Size();
  return node;
}

void Routing::AddExistingRandomNode(NodeId node) {
  if (node.IsValid()) {
    impl_->random_node_queue_.Push(node);
    size_t queue_size = impl_->random_node_queue_.Size();
    LOG(kVerbose) << "RandomNodeQueue : Added node, queue size now "
                  << queue_size;
    if (queue_size > 6)
      GetRandomExistingNode();
  }
}

void Routing::ConnectionLost(const Endpoint& lost_endpoint, std::weak_ptr<RoutingPrivate> impl) {
  std::shared_ptr<RoutingPrivate> pimpl = impl.lock();
  if (!pimpl) {
    LOG(kVerbose) << "Ignoring message received since this node is shutting down";
    return;
  }

  LOG(kWarning) << "Routing::ConnectionLost---------------------------------------------------";

  NodeInfo dropped_node;
  bool resend(!pimpl->tearing_down_ &&
              (pimpl->routing_table_.GetNodeInfo(lost_endpoint, dropped_node) &&
              pimpl->routing_table_.IsThisNodeInRange(dropped_node.node_id,
                                                      Parameters::closest_nodes_size)));

  // Checking routing table
  dropped_node = pimpl->routing_table_.DropNode(lost_endpoint);
  if (!dropped_node.node_id.Empty()) {
    LOG(kWarning) << "Lost connection with routing node "
                  << HexSubstr(dropped_node.node_id.String()) << ", endpoint " << lost_endpoint;
    LOG(kWarning) << "Routing::ConnectionLost-----------------------------------------Exiting";
  }

  // Checking non-routing table
  if (!dropped_node.node_id.Empty()) {
    dropped_node = pimpl->non_routing_table_.DropNode(lost_endpoint);
    if (!dropped_node.node_id.Empty()) {
      LOG(kWarning) << "Lost connection with non-routing node "
                    << HexSubstr(dropped_node.node_id.String()) << ", endpoint " << lost_endpoint;
    } else {
      LOG(kWarning) << "Lost connection with unknown/internal endpoint " << lost_endpoint;
    }
  }

  if (resend) {
    // Close node lost, get more nodes
    LOG(kWarning) << "Lost close node, getting more.";
    ReSendFindNodeRequest(boost::system::error_code(), true);
  }

  LOG(kWarning) << "Routing::ConnectionLost--------------------------------------------Exiting";
#ifdef LOCAL_TEST
  pimpl->RemoveConnectionFromBootstrapList(lost_endpoint);
#endif
}

bool Routing::ConfirmGroupMembers(const NodeId& node1, const NodeId& node2) {
  return impl_->routing_table_.ConfirmGroupMembers(node1, node2);
}

void Routing::ReSendFindNodeRequest(const boost::system::error_code& error_code,
                                    bool /*ignore_size*/) {
  if (error_code != boost::asio::error::operation_aborted) {
//     if (impl_->routing_table_.Size() == 0) {
//       LOG(kInfo) << "This node's [" << HexSubstr(impl_->keys_.identity)
//                  << "] Routing table is empty."
//                  << " Need to rebootstrap !!!";
//       return;
//     } else if (ignore_size ||  (impl_->routing_table_.Size() < Parameters::closest_nodes_size)) {
//       LOG(kInfo) << "This node's [" << HexSubstr(impl_->keys_.identity)
//                  << "] Routing table smaller than " << Parameters::closest_nodes_size
//                  << " nodes.  Sending another FindNodes..";
//       protobuf::Message find_node_rpc(rpcs::FindNodes(NodeId(impl_->keys_.identity),
//                                                       NodeId(impl_->keys_.identity)));
//       impl_->network_.SendToClosestNode(find_node_rpc);
//
//       impl_->recovery_timer_.expires_from_now(
//           boost::posix_time::seconds(Parameters::recovery_timeout_in_seconds));
//       impl_->recovery_timer_.async_wait([=](boost::system::error_code error_code) {
//                                             ReSendFindNodeRequest(error_code);
//                                           });
//     }
  } else {
    LOG(kVerbose) << "Cancelled recovery loop!!";
  }
}

}  // namespace routing

}  // namespace maidsafe
