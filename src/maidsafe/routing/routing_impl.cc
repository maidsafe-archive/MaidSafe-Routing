/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#include "maidsafe/routing/routing_impl.h"

#include <cstdint>
#include <type_traits>

#include "maidsafe/common/log.h"
#include "maidsafe/common/make_unique.h"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/passport/types.h"

#include "maidsafe/routing/bootstrap_file_operations.h"
#include "maidsafe/routing/message.h"
#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/utils.h"
#include "maidsafe/routing/network_statistics.h"

namespace fs = boost::filesystem;

namespace maidsafe {

namespace routing {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // unnamed namespace

namespace detail {}  // namespace detail

template <>
void Routing::Impl::Send(const GroupToSingleRelayMessage& message) {
  assert(!functors_.message_and_caching.message_received &&
         "Not allowed with string type message API");
  protobuf::Message proto_message = CreateNodeLevelMessage(message);
  // append relay information
  SendMessage(message.receiver.relay_node, proto_message);
}

template <>
protobuf::Message Routing::Impl::CreateNodeLevelMessage(const GroupToSingleRelayMessage& message) {
  protobuf::Message proto_message;
  proto_message.set_destination_id(message.receiver.relay_node->string());
  proto_message.set_routing_message(false);
  proto_message.add_data(message.contents);
  proto_message.set_type(static_cast<int32_t>(MessageType::kNodeLevel));

  proto_message.set_cacheable(static_cast<int32_t>(message.cacheable));
  proto_message.set_client_node(routing_table_->client_mode());

  proto_message.set_request(true);
  proto_message.set_hops_to_live(Parameters::hops_to_live);

  AddGroupSourceRelatedFields(message, proto_message,
                              detail::is_group_source<GroupToSingleRelayMessage>());
  AddDestinationTypeRelatedFields(proto_message,
                                  detail::is_group_destination<GroupToSingleRelayMessage>());

  // add relay information
  proto_message.set_relay_id(message.receiver.node_id->string());
  proto_message.set_relay_connection_id(message.receiver.connection_id.string());
  proto_message.set_actual_destination_is_relay_id(true);

  proto_message.set_id(RandomUint32() % 10000);  // Enable for tracing node level messages
  return proto_message;
}

Routing::Impl::Impl(bool client_mode, const NodeId& node_id, const asymm::Keys& keys)
    : network_status_mutex_(),
      network_status_(kNotJoined),
      network_statistics_(node_id),
      routing_table_(maidsafe::make_unique<RoutingTable>(client_mode, node_id, keys)),
      kNodeId_(node_id),
      running_(true),
      running_mutex_(),
      functors_(),
      random_node_helper_(),
      // TODO(Prakash) : don't create client_routing_table for client nodes (wrap both)
      client_routing_table_(node_id),
      message_handler_(),
      asio_service_(2),
      network_(maidsafe::make_unique<NetworkUtils>(*routing_table_, client_routing_table_)),
      timer_(asio_service_),
      re_bootstrap_timer_(asio_service_.service()),
      recovery_timer_(asio_service_.service()),
      setup_timer_(asio_service_.service()) {
  message_handler_.reset(new MessageHandler(*routing_table_, client_routing_table_, *network_,
                                            timer_, network_statistics_));
  LOG(kInfo) << (client_mode ? "client " : "non-client ") << "node. Id : " << kNodeId_;
  assert((client_mode || !node_id.IsZero()) && "Server Nodes cannot be created without valid keys");
}

void Routing::Impl::Stop() {
  LOG(kVerbose) << "Routing::Impl::Stop() " << kNodeId_ << ", connection id "
    << routing_table_->kConnectionId();
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    running_ = false;
  }
  timer_.CancelAll();
  re_bootstrap_timer_.cancel();
  recovery_timer_.cancel();
  setup_timer_.cancel();
  asio_service_.Stop();
  // Need to destroy network_ & routing_table_ as they hold a lambda capture (functor) of
  // shared_from_this()
  network_.reset();
  routing_table_.reset();
}

void Routing::Impl::Join(const Functors& functors) {
  LOG(kInfo) << "Doing a default join";
  ConnectFunctors(functors);
  Bootstrap();
}

void Routing::Impl::ConnectFunctors(const Functors& functors) {
  functors_ = functors;
  std::shared_ptr<Routing::Impl> this_ptr(shared_from_this());
  routing_table_->InitialiseFunctors([this_ptr](const RoutingTableChange& routing_table_change) {
                                      this_ptr->OnRoutingTableChange(routing_table_change);
                                    });
  // only one of MessageAndCachingFunctors or TypedMessageAndCachingFunctor should be provided
  assert(!functors.message_and_caching.message_received !=
         !functors.typed_message_and_caching.single_to_single.message_received);
  assert(!functors.message_and_caching.message_received !=
         !functors.typed_message_and_caching.single_to_group.message_received);
  assert(!functors.message_and_caching.message_received !=
         !functors.typed_message_and_caching.group_to_single.message_received);
  assert(!functors.message_and_caching.message_received !=
         !functors.typed_message_and_caching.group_to_group.message_received);
  if (!routing_table_->client_mode()) {
    assert(!functors.message_and_caching.message_received !=
           !functors.typed_message_and_caching.single_to_group_relay.message_received);
  }
  if (functors.message_and_caching.message_received)
    message_handler_->set_message_and_caching_functor(functors.message_and_caching);
  else
    message_handler_->set_typed_message_and_caching_functor(functors.typed_message_and_caching);

  message_handler_->set_request_public_key_functor(functors.request_public_key);
}

void Routing::Impl::Bootstrap() {
  {
    std::lock_guard<std::mutex>  lock(running_mutex_);
    if (!running_)
      return;
  }
  int return_value(DoBootstrap());
  if (kSuccess != return_value) {
    re_bootstrap_timer_.expires_from_now(Parameters::re_bootstrap_time_lag);
    std::shared_ptr<Routing::Impl> this_ptr(shared_from_this());
    re_bootstrap_timer_.async_wait(
        [this_ptr](boost::system::error_code error_code) {
          if (error_code != boost::asio::error::operation_aborted)
            this_ptr->Bootstrap();
        });
    NotifyNetworkStatus(return_value);
    return;
  }

  assert(!network_->bootstrap_connection_id().IsZero() &&
         "Bootstrap connection id must be populated by now.");
  FindClosestNode(boost::system::error_code(), 0);
  NotifyNetworkStatus(return_value);
}

int Routing::Impl::DoBootstrap() {
  // FIXME race condition if a new connection appears at rudp -- rudp should handle this
  assert(routing_table_->size() == 0);
  recovery_timer_.cancel();
  setup_timer_.cancel();
  std::lock_guard<std::mutex> lock(running_mutex_);
  if (!running_)
    return kNetworkShuttingDown;
  if (!network_->bootstrap_connection_id().IsZero()) {
    LOG(kInfo) << "Removing bootstrap connection to rebootstrap. Connection id : "
               << network_->bootstrap_connection_id();
    network_->Remove(network_->bootstrap_connection_id());
    network_->clear_bootstrap_connection_info();
  }
  std::shared_ptr<Routing::Impl> this_ptr(shared_from_this());
  return network_->Bootstrap(
      [this_ptr](const std::string& message) { this_ptr->OnMessageReceived(message); },
      [this_ptr](const NodeId& lost_connection_id) {
        this_ptr->OnConnectionLost(lost_connection_id);
      });
}

void Routing::Impl::FindClosestNode(const boost::system::error_code& error_code, int attempts) {
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
  }
  if (error_code == boost::asio::error::operation_aborted)
    return;

  if (attempts == 0) {
    assert(!network_->bootstrap_connection_id().IsZero() && "Only after bootstrapping succeeds");
    assert(!network_->this_node_relay_connection_id().IsZero() &&
           "Relay connection id should be set after bootstrapping succeeds");
  } else {
    if (routing_table_->size() > 0) {
      std::lock_guard<std::mutex> lock(running_mutex_);
      if (!running_)
        return;
      // Exit the loop & start recovery loop
      LOG(kVerbose) << "[" << kNodeId_ << "] Added a node in routing table."
                    << " Terminating setup loop & Scheduling recovery loop.";
      recovery_timer_.expires_from_now(Parameters::find_node_interval);
      std::shared_ptr<Routing::Impl> this_ptr(shared_from_this());
      recovery_timer_.async_wait([this_ptr](const boost::system::error_code& error_code) {
        if (error_code != boost::asio::error::operation_aborted)
          this_ptr->ReSendFindNodeRequest(error_code, false);
      });
      return;
    }

    if (static_cast<unsigned int>(attempts) >= Parameters::maximum_find_close_node_failures) {
      LOG(kError) << "[" << kNodeId_ << "] failed to get closest node. ReBootstrapping...";
      // TODO(Prakash) : Remove the bootstrap node from the list
      ReBootstrap();
      return;
    }
  }

  int num_nodes_requested(1 + attempts / Parameters::find_node_repeats_per_num_requested);
  protobuf::Message find_node_rpc(rpcs::FindNodes(kNodeId_, kNodeId_, num_nodes_requested, true,
                                                  network_->this_node_relay_connection_id()));
  LOG(kVerbose) << "   [" << kNodeId_ << "] (attempt " << attempts << ")  requesting "
                << num_nodes_requested << " nodes (id: " << find_node_rpc.id() << ")";
  std::shared_ptr<Routing::Impl> this_ptr(shared_from_this());
  rudp::MessageSentFunctor message_sent_functor([this_ptr, find_node_rpc](int message_sent) {
    if (message_sent == kSuccess)
      LOG(kVerbose) << "   [" << this_ptr->kNodeId_ << "] sent : "
                    << MessageTypeString(find_node_rpc)
                    << " to   " << this_ptr->network_->bootstrap_connection_id() << "   (id: "
                    << find_node_rpc.id() << ")";
    else
      LOG(kError) << "Failed to send FindNodes RPC to bootstrap connection id : "
                  << this_ptr->network_->bootstrap_connection_id();
  });

  ++attempts;
  network_->SendToDirect(find_node_rpc, network_->bootstrap_connection_id(), message_sent_functor);

  std::lock_guard<std::mutex> lock(running_mutex_);
  if (!running_)
    return;
  setup_timer_.expires_from_now(Parameters::find_close_node_interval);
  setup_timer_.async_wait([this_ptr, attempts](boost::system::error_code error_code_local) {
    if (error_code_local != boost::asio::error::operation_aborted)
      this_ptr->FindClosestNode(error_code_local, attempts);
  });
}

void Routing::Impl::ReBootstrap() {
  recovery_timer_.cancel();
  Bootstrap();
}

int Routing::Impl::ZeroStateJoin(const Functors& functors, const Endpoint& local_endpoint,
                                 const Endpoint& peer_endpoint, const NodeInfo& peer_info) {
  assert((!routing_table_->client_mode()) && "no client nodes allowed in zero state network");
  ConnectFunctors(functors);
  std::shared_ptr<Routing::Impl> this_ptr(shared_from_this());
  int result(network_->ZeroStateBootstrap(
      [this_ptr](const std::string& message) { this_ptr->OnMessageReceived(message); },
      [this_ptr](const NodeId& lost_connection_id) {
        this_ptr->OnConnectionLost(lost_connection_id);
      },
      local_endpoint));

  if (result != kSuccess) {
    LOG(kError) << "Could not bootstrap zero state node from local endpoint : " << local_endpoint
                << " with peer endpoint : " << peer_endpoint;
    return result;
  }

  LOG(kInfo) << "[" << kNodeId_ << "]'s bootstrap connection id : "
             << network_->bootstrap_connection_id();

  assert(!peer_info.id.IsZero() && "Zero NodeId passed");
  assert((network_->bootstrap_connection_id() == peer_info.id) &&
         "Should bootstrap only with known peer for zero state network");
  LOG(kVerbose) << local_endpoint << " Bootstrapped with remote endpoint " << peer_endpoint;
  rudp::NatType nat_type(rudp::NatType::kUnknown);
  rudp::EndpointPair peer_endpoint_pair;  // zero state nodes must be directly connected endpoint
  rudp::EndpointPair this_endpoint_pair;
  peer_endpoint_pair.external = peer_endpoint_pair.local = peer_endpoint;
  this_endpoint_pair.external = this_endpoint_pair.local = local_endpoint;
  Sleep(std::chrono::milliseconds(100));  // FIXME avoiding assert in rudp
  result = network_->GetAvailableEndpoint(peer_info.id, peer_endpoint_pair, this_endpoint_pair,
                                         nat_type);
  if (result != rudp::kBootstrapConnectionAlreadyExists) {
    LOG(kError) << "Failed to get available endpoint to add zero state node : " << peer_endpoint;
    return result;
  }

  result = network_->Add(peer_info.id, peer_endpoint_pair, "invalid");
  if (result != kSuccess) {
    LOG(kError) << "Failed to add zero state node : " << peer_endpoint;
    return result;
  }

  ValidateAndAddToRoutingTable(*network_, *routing_table_, client_routing_table_, peer_info.id,
                               peer_info.id, peer_info.public_key, false);
  // Now poll for routing table size to have other zero state peer.
  uint8_t poll_count(0);
  do {
    Sleep(std::chrono::milliseconds(100));
  } while ((routing_table_->size() == 0) && (++poll_count < 50));
  if (routing_table_->size() != 0) {
    LOG(kInfo) << "Node Successfully joined zero state network, with "
               << network_->bootstrap_connection_id() << ", Routing table size - "
               << routing_table_->size() << ", Node id : " << kNodeId_;

    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return kNetworkShuttingDown;
    recovery_timer_.expires_from_now(Parameters::find_node_interval);
    std::shared_ptr<Routing::Impl> this_ptr(shared_from_this());
    recovery_timer_.async_wait([this_ptr](const boost::system::error_code& error_code) {
      if (error_code != boost::asio::error::operation_aborted)
        this_ptr->ReSendFindNodeRequest(error_code, false);
    });
    return kSuccess;
  } else {
    LOG(kError) << "Failed to join zero state network, with bootstrap_endpoint " << peer_endpoint;
    return kNotJoined;
  }
}

void Routing::Impl::SendDirect(const NodeId& destination_id, const std::string& data,
                               bool cacheable, ResponseFunctor response_functor) {
  assert(!functors_.typed_message_and_caching.single_to_single.message_received &&
         "Not allowed with typed Message API");
  Send(destination_id, data, DestinationType::kDirect, cacheable, response_functor);
}

void Routing::Impl::SendGroup(const NodeId& destination_id, const std::string& data,
                              bool cacheable, ResponseFunctor response_functor) {
  assert(!functors_.typed_message_and_caching.single_to_single.message_received &&
         "Not allowed with typed Message API");
  Send(destination_id, data, DestinationType::kGroup, cacheable, response_functor);
}

void Routing::Impl::Send(const NodeId& destination_id, const std::string& data,
                         const DestinationType& destination_type, bool cacheable,
                         ResponseFunctor response_functor) {
  LOG(kVerbose) << "Routing::Impl::Send from " << kNodeId_ << " to " << destination_id;
  CheckSendParameters(destination_id, data);
  protobuf::Message proto_message =
      CreateNodeLevelPartialMessage(destination_id, destination_type, data, cacheable);
  unsigned int expected_response_count(1);
  if (response_functor) {
    if (DestinationType::kGroup == destination_type)
      expected_response_count = Parameters::group_size;
    proto_message.set_id(timer_.NewTaskId());
    timer_.AddTask(Parameters::default_response_timeout, response_functor, expected_response_count,
                   proto_message.id());
  } else {
    proto_message.set_id(0);
  }
  SendMessage(destination_id, proto_message);
}

void Routing::Impl::SendMessage(const NodeId& destination_id, protobuf::Message& proto_message) {
  if (routing_table_->size() == 0) {  // Partial join state
    PartiallyJoinedSend(proto_message);
  } else {  // Normal node
    proto_message.set_source_id(kNodeId_.string());
    if (kNodeId_ != destination_id) {
      network_->SendToClosestNode(proto_message);
    } else if (routing_table_->client_mode()) {
      LOG(kVerbose) << "Client sending request to self id";
      network_->SendToClosestNode(proto_message);
    } else {
      LOG(kInfo) << "Sending request to self";
      OnMessageReceived(proto_message.SerializeAsString());
    }
  }
}

// Partial join state
void Routing::Impl::PartiallyJoinedSend(protobuf::Message& proto_message) {
  proto_message.set_relay_id(kNodeId_.string());
  proto_message.set_relay_connection_id(network_->this_node_relay_connection_id().string());
  NodeId bootstrap_connection_id(network_->bootstrap_connection_id());
  assert(proto_message.has_relay_connection_id() && "did not set this_node_relay_connection_id");
  std::shared_ptr<Routing::Impl> this_ptr(shared_from_this());
  rudp::MessageSentFunctor message_sent([this_ptr, bootstrap_connection_id,
                                        proto_message](int result) {
    std::lock_guard<std::mutex> lock(this_ptr->running_mutex_);
    if (!this_ptr->running_)
      return;
    this_ptr->asio_service_.service().post([this_ptr, result, proto_message,
                                           bootstrap_connection_id]() {
      if (rudp::kSuccess != result) {
        if (proto_message.id() != 0) {
          try {
            this_ptr->timer_.CancelTask(proto_message.id());
          }
          catch (const maidsafe_error& error) {
            if (error.code() != make_error_code(CommonErrors::invalid_parameter))
              throw;
          }
        }
        LOG(kError) << "Partial join Session Ended, Send not allowed anymore";
        this_ptr->NotifyNetworkStatus(kPartialJoinSessionEnded);
      } else {
        LOG(kVerbose) << "   [" << this_ptr->kNodeId_ << "] sent : "
                      << MessageTypeString(proto_message) << " to   " << bootstrap_connection_id
                      << "   (id: " << proto_message.id()
                      << ") dst: " << NodeId(proto_message.destination_id()) << "--Partial-joined-";
      }
    });
  });
  network_->SendToDirect(proto_message, bootstrap_connection_id, message_sent);
}

protobuf::Message Routing::Impl::CreateNodeLevelPartialMessage(
    const NodeId& destination_id, const DestinationType& destination_type, const std::string& data,
    bool cacheable) {
  protobuf::Message proto_message;
  proto_message.set_destination_id(destination_id.string());
  proto_message.set_routing_message(false);
  proto_message.add_data(data);
  proto_message.set_type(static_cast<int32_t>(MessageType::kNodeLevel));
  if (cacheable)
    proto_message.set_cacheable(static_cast<int32_t>(Cacheable::kGet));
  proto_message.set_direct((DestinationType::kDirect == destination_type));
  proto_message.set_client_node(routing_table_->client_mode());
  proto_message.set_request(true);
  proto_message.set_hops_to_live(Parameters::hops_to_live);
  unsigned int replication(1);
  if (DestinationType::kGroup == destination_type) {
    proto_message.set_visited(false);
    replication = Parameters::group_size;
  }
  proto_message.set_replication(replication);

  return proto_message;
}

// throws
void Routing::Impl::CheckSendParameters(const NodeId& destination_id, const std::string& data) {
  if (destination_id.IsZero()) {
    LOG(kError) << "Invalid destination ID, aborted send";
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::invalid_node_id));
  }

  if (data.empty() || (data.size() > Parameters::max_data_size)) {
    LOG(kError) << "Data size not allowed : " << data.size();
    // FIXME (need error type here)
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::invalid_parameter));
  }
}

bool Routing::Impl::ClosestToId(const NodeId& target_id) {
  return routing_table_->IsThisNodeClosestTo(target_id, true);
}

boost::optional<NodeId> Routing::Impl::RandomConnectedNode() {
  return routing_table_->RandomConnectedNode();
}

bool Routing::Impl::EstimateInGroup(const NodeId& sender_id, const NodeId& info_id) {
  return ((routing_table_->size() > Parameters::routing_table_ready_to_response) &&
          network_statistics_.EstimateInGroup(sender_id, info_id));
}

std::future<std::vector<NodeId>> Routing::Impl::GetGroup(const NodeId& group_id) {
  auto promise(std::make_shared<std::promise<std::vector<NodeId>>>());
  auto future(promise->get_future());
  auto callback = [promise](const std::string& response) {
    std::vector<NodeId> nodes_id;
    if (!response.empty()) {
      protobuf::GetGroup get_group;
      if (get_group.ParseFromString(response)) {
        try {
          for (const auto& id : get_group.group_nodes_id())
            nodes_id.push_back(NodeId(id));
        }
        catch (std::exception& ex) {
          LOG(kError) << "Failed to parse response of GetGroup : " << ex.what();
        }
      }
    }
    promise->set_value(nodes_id);
  };
  protobuf::Message get_group_message(rpcs::GetGroup(group_id, kNodeId_));
  get_group_message.set_id(timer_.NewTaskId());
  timer_.AddTask(Parameters::default_response_timeout, callback, 1, get_group_message.id());
  network_->SendToClosestNode(get_group_message);
  return future;
}

void Routing::Impl::OnMessageReceived(const std::string& message) {
  std::lock_guard<std::mutex> lock(running_mutex_);
  if (running_) {
    std::shared_ptr<Routing::Impl> this_ptr(shared_from_this());
    asio_service_.service().post([this_ptr, message]() { this_ptr->DoOnMessageReceived(message); });  // NOLINT (Fraser)
  }
}

void Routing::Impl::DoOnMessageReceived(const std::string& message) {
  protobuf::Message pb_message;
  if (pb_message.ParseFromString(message)) {
    bool relay_message(!pb_message.has_source_id());
    LOG(kVerbose) << "   [" << kNodeId_ << "] rcvd : " << MessageTypeString(pb_message)
                  << " from " << (relay_message ? HexSubstr(pb_message.relay_id())
                                                : HexSubstr(pb_message.source_id())) << " to "
                  << HexSubstr(pb_message.destination_id()) << "   (id: " << pb_message.id() << ")"
                  << (relay_message ? " --Relay--" : "");
    if ((!pb_message.client_node() && pb_message.has_source_id()) ||
        (!pb_message.direct() && !pb_message.request())) {
      NodeId source_id(pb_message.source_id());
      if (!source_id.IsZero())
        random_node_helper_.Add(source_id);
    }
    {
      std::lock_guard<std::mutex> lock(running_mutex_);
      if (!running_)
        return;
    }
    message_handler_->HandleMessage(pb_message);
  } else {
    LOG(kWarning) << "Message received, failed to parse";
  }
}

void Routing::Impl::OnConnectionLost(const NodeId& lost_connection_id) {
  std::lock_guard<std::mutex> lock(running_mutex_);
  if (running_) {
    std::shared_ptr<Routing::Impl> this_ptr(shared_from_this());
    asio_service_.service().post([this_ptr, lost_connection_id]() {
      this_ptr->DoOnConnectionLost(lost_connection_id);
    });
  }
}

void Routing::Impl::DoOnConnectionLost(const NodeId& lost_connection_id) {
  LOG(kVerbose) << DebugId(kNodeId_) << "  Routing::ConnectionLost with -----------"
                << DebugId(lost_connection_id);
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
  }

  NodeInfo dropped_node;
  bool resend(
      routing_table_->GetNodeInfo(lost_connection_id, dropped_node) &&
      routing_table_->IsThisNodeInRange(dropped_node.id, Parameters::closest_nodes_size));

  // Checking routing table
  dropped_node = routing_table_->DropNode(lost_connection_id, true);
  if (!dropped_node.id.IsZero()) {
    LOG(kWarning) << "[" << DebugId(kNodeId_) << "]"
                  << "Lost connection with routing node " << DebugId(dropped_node.id);
    random_node_helper_.Remove(dropped_node.id);
  }

  // Checking non-routing table
  if (dropped_node.id.IsZero()) {
    resend = false;
    dropped_node = client_routing_table_.DropConnection(lost_connection_id);
    if (!dropped_node.id.IsZero()) {
      LOG(kWarning) << "[" << DebugId(kNodeId_) << "]"
                    << "Lost connection with non-routing node "
                    << HexSubstr(dropped_node.id.string());
    } else if (!network_->bootstrap_connection_id().IsZero() &&
               lost_connection_id == network_->bootstrap_connection_id()) {
      LOG(kWarning) << "[" << DebugId(kNodeId_) << "]"
                    << "Lost temporary connection with bootstrap node. connection id :"
                    << DebugId(lost_connection_id);
      {
        std::lock_guard<std::mutex> lock(running_mutex_);
        if (!running_)
          return;
      }
      network_->clear_bootstrap_connection_info();

      if (routing_table_->size() == 0)
        resend = true;  // This will trigger rebootstrap
    } else {
      LOG(kWarning) << "[" << DebugId(kNodeId_) << "]"
                    << "Lost connection with unknown/internal connection id "
                    << DebugId(lost_connection_id);
    }
  }

  if (resend) {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
    // Close node lost, get more nodes
    LOG(kWarning) << "Lost close node, getting more.";
    recovery_timer_.expires_from_now(Parameters::recovery_time_lag);
    std::shared_ptr<Routing::Impl> this_ptr(shared_from_this());
    recovery_timer_.async_wait([this_ptr](const boost::system::error_code& error_code) {
      if (error_code != boost::asio::error::operation_aborted)
        this_ptr->ReSendFindNodeRequest(error_code, true);
    });
  }
}

void Routing::Impl::RemoveNode(const NodeInfo& node, bool internal_rudp_only) {
  if (node.connection_id.IsZero() || node.id.IsZero())
    return;

  network_->Remove(node.connection_id);
  if (internal_rudp_only) {  // No recovery
    LOG(kInfo) << "Routing: removed node : " << DebugId(node.id)
               << ". Removed internal rudp connection id : " << DebugId(node.connection_id);
    return;
  }

  LOG(kInfo) << "Routing: removed node : " << DebugId(node.id)
             << ". Removed rudp connection id : " << DebugId(node.connection_id);

  // TODO(Prakash): Handle pseudo connection removal here and NRT node removal

  bool resend(routing_table_->IsThisNodeInRange(node.id, Parameters::closest_nodes_size));
  if (resend) {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
    // Close node removed by routing, get more nodes
    LOG(kWarning) << "[" << DebugId(kNodeId_)
                  << "] Removed close node, sending find node to get more nodes.";
    recovery_timer_.expires_from_now(Parameters::recovery_time_lag);
    std::shared_ptr<Routing::Impl> this_ptr(shared_from_this());
    recovery_timer_.async_wait([this_ptr](const boost::system::error_code& error_code) {
      if (error_code != boost::asio::error::operation_aborted)
        this_ptr->ReSendFindNodeRequest(error_code, true);
    });
  }
}

bool Routing::Impl::ConfirmGroupMembers(const NodeId& node1, const NodeId& node2) {
  return routing_table_->ConfirmGroupMembers(node1, node2);
}

void Routing::Impl::ReSendFindNodeRequest(const boost::system::error_code& error_code,
                                          bool ignore_size) {
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (error_code == boost::asio::error::operation_aborted || !running_)
      return;
  }

  if (routing_table_->size() == 0) {
    LOG(kError) << "[" << kNodeId_ << "]'s' Routing table is empty. Scheduling Re-Bootstrap.. !!!";
    ReBootstrap();
    return;
  } else if (ignore_size || (routing_table_->size() < routing_table_->kThresholdSize())) {
    if (!ignore_size)
      LOG(kInfo) << "[" << kNodeId_ << "] Routing table smaller than "
                 << routing_table_->kThresholdSize() << " nodes.  Sending another FindNodes. "
                 << " Routing table size " << routing_table_->size() << " >";
    else
      LOG(kInfo) << "[" << kNodeId_ << "] lost close node."
                 << "Sending another FindNodes. Current routing table size : "
                 << routing_table_->size();

    int num_nodes_requested(0);
    if (ignore_size && (routing_table_->size() > routing_table_->kThresholdSize()))
      num_nodes_requested = static_cast<int>(Parameters::closest_nodes_size);
    else
      num_nodes_requested = static_cast<int>(Parameters::max_routing_table_size);

    protobuf::Message find_node_rpc(rpcs::FindNodes(kNodeId_, kNodeId_, num_nodes_requested));
    network_->SendToClosestNode(find_node_rpc);

    recovery_timer_.expires_from_now(Parameters::find_node_interval);
    std::shared_ptr<Routing::Impl> this_ptr(shared_from_this());
    recovery_timer_.async_wait([this_ptr, error_code](boost::system::error_code error_code_local) {
      if (error_code != boost::asio::error::operation_aborted)
        this_ptr->ReSendFindNodeRequest(error_code_local, false);
    });
  }
}

void Routing::Impl::NotifyNetworkStatus(int return_code) const {
  if (functors_.network_status)
    functors_.network_status(return_code);
}

NodeId Routing::Impl::kNodeId() const { return kNodeId_; }

int Routing::Impl::network_status() {
  std::lock_guard<std::mutex> lock(network_status_mutex_);
  return network_status_;
}

std::vector<NodeInfo> Routing::Impl::ClosestNodes() {
  return routing_table_->GetClosestNodes(kNodeId(), Parameters::closest_nodes_size);
}

bool Routing::Impl::IsConnectedVault(const NodeId& node_id) {
  return routing_table_->Contains(node_id);
}

bool Routing::Impl::IsConnectedClient(const NodeId& node_id) {
  return client_routing_table_.IsConnected(node_id);
}

// New API
void Routing::Impl::AddDestinationTypeRelatedFields(protobuf::Message& proto_message,
                                                    std::true_type) {
  proto_message.set_direct(false);
  proto_message.set_replication(Parameters::group_size);
  proto_message.set_visited(false);
  proto_message.set_group_destination(proto_message.destination_id());
}

void Routing::Impl::AddDestinationTypeRelatedFields(protobuf::Message& proto_message,
                                                    std::false_type) {
  proto_message.set_direct(true);
  proto_message.set_replication(1);
}

void Routing::Impl::OnRoutingTableChange(const RoutingTableChange& routing_table_change) {
  {
    std::lock_guard<std::mutex> lock(network_status_mutex_);
    network_status_ = routing_table_change.health;
  }
  NotifyNetworkStatus(routing_table_change.health);
  LOG(kVerbose) << kNodeId_ << " Updating network status !!! " << routing_table_change.health;

  if (routing_table_change.removed.node.id != NodeId()) {
    RemoveNode(routing_table_change.removed.node,
               routing_table_change.removed.routing_only_removal);
    LOG(kVerbose) << "Routing table removed node id : " << routing_table_change.removed.node.id
                  << ", connection id : " << routing_table_change.removed.node.connection_id;
  }

  if (routing_table_->client_mode()) {
    if (routing_table_->size() < Parameters::max_routing_table_size_for_client)
      network_->SendToClosestNode(rpcs::FindNodes(kNodeId_, kNodeId_,
                                 Parameters::max_routing_table_size_for_client));
    return;
  }

  if (routing_table_change.close_nodes_change != nullptr) {
    if (functors_.close_nodes_change)
      functors_.close_nodes_change(routing_table_change.close_nodes_change);
    network_statistics_.UpdateLocalAverageDistance(
        routing_table_change.close_nodes_change->new_close_nodes());
    // IpcSendCloseNodes(); TO BE MOVED FROM RT TO UTILS
  }

  if (routing_table_change.close_nodes_change && routing_table_change.insertion) {
    auto clients(client_routing_table_.GetNodesInfo());
    for (auto client : clients)
      InformClientOfNewCloseNode(*network_, client, routing_table_change.added_node, kNodeId());
  }

  if (routing_table_->size() > Parameters::routing_table_size_threshold)
    network_->SendToClosestNode(rpcs::FindNodes(kNodeId_, kNodeId_,
                                               Parameters::max_routing_table_size));
}

}  // namespace routing

}  // namespace maidsafe
