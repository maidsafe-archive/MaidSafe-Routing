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

#ifndef MAIDSAFE_ROUTING_ROUTING_IMPL_H_
#define MAIDSAFE_ROUTING_ROUTING_IMPL_H_

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "boost/asio/steady_timer.hpp"
#include "boost/asio/ip/udp.hpp"
#include "boost/system/error_code.hpp"

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/node_id.h"

#include "maidsafe/common/rsa.h"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/client_routing_table.h"
#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/random_node_helper.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/timer.h"
#include "maidsafe/routing/return_codes.h"



namespace maidsafe {

namespace routing {

namespace detail {

// Group Source
template <typename Messsage>
struct is_group_source;

template <typename Messsage>
struct is_group_source : public std::true_type {};

template <>
struct is_group_source<SingleToSingleMessage> : public std::false_type {};
template <>
struct is_group_source<SingleToGroupMessage> : public std::false_type {};
template <>
struct is_group_source<GroupToSingleMessage> : public std::true_type {};
template <>
struct is_group_source<GroupToGroupMessage> : public std::true_type {};
template <>
struct is_group_source<GroupToSingleRelayMessage> : public std::true_type {};

// Group Destination
template <typename Messsage>
struct is_group_destination;

template <typename Messsage>
struct is_group_destination : public std::true_type {};

template <>
struct is_group_destination<SingleToSingleMessage> : public std::false_type {};
template <>
struct is_group_destination<SingleToGroupMessage> : public std::true_type {};
template <>
struct is_group_destination<GroupToSingleMessage> : public std::false_type {};
template <>
struct is_group_destination<GroupToGroupMessage> : public std::true_type {};
template <>
struct is_group_destination<GroupToSingleRelayMessage> : public std::false_type {};

}  // namespace detail

//  class MessageHandler;
struct NodeInfo;

namespace test {
class GenericNode;
}

template <typename NodeType>
class RoutingImpl : public std::enable_shared_from_this<RoutingImpl<NodeType>> {
 public:
  RoutingImpl(const NodeId& node_id, const asymm::Keys& keys)
      : network_status_mutex_(),
        network_status_(kNotJoined),
        network_statistics_(node_id),
        connections_(node_id, keys),
        kNodeId_(node_id),
        running_(true),
        running_mutex_(),
        functors_(),
        random_node_helper_(),
        // TODO(Prakash) : don't create client_routing_table for client nodes (wrap both)
        message_handler_(),
        asio_service_(2),
        network_(connections_),
        timer_(asio_service_),
        re_bootstrap_timer_(asio_service_.service()),
        recovery_timer_(asio_service_.service()),
        setup_timer_(asio_service_.service()) {
    message_handler_.reset(
        new MessageHandler<NodeType>(connections_, network_, timer_, network_statistics_));
    LOG(kVerbose) << NodeType::value << ", " << connections_.kNodeId()
                  << ", routing_table_.kConnectionId()"
                  << connections_.kConnectionId();
  }
  ~RoutingImpl();

  void Join(const Functors& functors,
            const BootstrapContacts& bootstrap_contacts = BootstrapContacts());

  int ZeroStateJoin(const Functors& functors, const boost::asio::ip::udp::endpoint& local_endpoint,
                    const boost::asio::ip::udp::endpoint& peer_endpoint, const NodeInfo& peer_info);

  template <typename T>
  void Send(const T& message);  // New API

  void SendDirect(const NodeId& destination_id, const std::string& data, bool cacheable,
                  ResponseFunctor response_functor);

  void SendGroup(const NodeId& destination_id, const std::string& data, bool cacheable,
                 ResponseFunctor response_functor);

  NodeId GetRandomExistingNode() const { return random_node_helper_.Get(); }

  bool ClosestToId(const NodeId& node_id);

  NodeId RandomConnectedNode();

  bool EstimateInGroup(const NodeId& sender_id, const NodeId& info_id);

  std::future<std::vector<NodeId>> GetGroup(const NodeId& group_id);

  NodeId kNodeId() const;

  int network_status();

  std::vector<NodeInfo> ClosestNodes();

  bool IsConnectedVault(const NodeId& node_id);
  bool IsConnectedClient(const NodeId& node_id);

  friend class test::GenericNode;

 private:
  RoutingImpl(const RoutingImpl&);
  RoutingImpl(const RoutingImpl&&);
  RoutingImpl& operator=(const RoutingImpl&);

  void ConnectFunctors(const Functors& functors);
  void BootstrapFromTheseEndpoints(const BootstrapContacts& bootstrap_contacts);
  void DoJoin(const BootstrapContacts& bootstrap_contacts);
  int DoBootstrap(const BootstrapContacts& bootstrap_contacts);
  void ReBootstrap();
  void DoReBootstrap(const boost::system::error_code& error_code);
  void FindClosestNode(const boost::system::error_code& error_code, int attempts);
  void ReSendFindNodeRequest(const boost::system::error_code& error_code, bool ignore_size);
  void OnMessageReceived(const std::string& message);
  void DoOnMessageReceived(const std::string& message);
  void OnConnectionLost(const NodeId& lost_connection_id);
  void DoOnConnectionLost(const NodeId& lost_connection_id);
  void OnRoutingTableChange(const RoutingTableChange& routing_table_change);
  void RemoveNode(const NodeInfo& node, bool internal_rudp_only);
  bool ConfirmGroupMembers(const NodeId& node1, const NodeId& node2);
  void NotifyNetworkStatus(int return_code) const;
  void Send(const NodeId& destination_id, const std::string& data,
            const DestinationType& destination_type, bool cacheable,
            ResponseFunctor response_functor);
  void SendMessage(const NodeId& destination_id, protobuf::Message& proto_message);
  void PartiallyJoinedSend(protobuf::Message& proto_message);
  protobuf::Message CreateNodeLevelPartialMessage(const NodeId& destination_id,
                                                  const DestinationType& destination_type,
                                                  const std::string& data, bool cacheable);
  void CheckSendParameters(const NodeId& destination_id, const std::string& data);

  template <typename T>
  protobuf::Message CreateNodeLevelMessage(const T& message);
  template <typename T>
  void AddGroupSourceRelatedFields(const T& message, protobuf::Message& proto_message,
                                   std::true_type);
  template <typename T>
  void AddGroupSourceRelatedFields(const T& message, protobuf::Message& proto_message,
                                   std::false_type);

  void AddDestinationTypeRelatedFields(protobuf::Message& proto_message, std::true_type);
  void AddDestinationTypeRelatedFields(protobuf::Message& proto_message, std::false_type);

  std::mutex network_status_mutex_;
  int network_status_;
  NetworkStatistics network_statistics_;
  Connections<NodeType> connections_;
  const NodeId kNodeId_;
  bool running_;
  std::mutex running_mutex_;
  Functors functors_;
  RandomNodeHelper random_node_helper_;
  // The following variables' declarations should remain the last ones in this class and should stay
  // in the order: message_handler_, asio_service_, network_, all timers.  This is important for the
  // proper destruction of the routing library, i.e. to avoid segmentation faults.
  std::unique_ptr<MessageHandler<NodeType>> message_handler_;
  AsioService asio_service_;
  NetworkUtils<NodeType> network_;
  Timer<std::string> timer_;
  boost::asio::steady_timer re_bootstrap_timer_, recovery_timer_, setup_timer_;
};

template <>
template <>
void RoutingImpl<VaultNode>::Send(const GroupToSingleRelayMessage& message);

template <>
template <>
void RoutingImpl<ClientNode>::Send(const GroupToSingleRelayMessage& message);


template <>
template <>
protobuf::Message RoutingImpl<VaultNode>::CreateNodeLevelMessage(
    const GroupToSingleRelayMessage& message);

template <>
template <>
protobuf::Message RoutingImpl<ClientNode>::CreateNodeLevelMessage(
    const GroupToSingleRelayMessage& message);

// Implementations
template <typename NodeType>
template <typename T>
void RoutingImpl<NodeType>::Send(const T& message) {  // FIXME(Fix caching)
  assert(!functors_.message_and_caching.message_received &&
         "Not allowed with string type message API");
  protobuf::Message proto_message = CreateNodeLevelMessage(message);
  SendMessage(message.receiver, proto_message);
}

template <typename NodeType>
template <typename T>
void RoutingImpl<NodeType>::AddGroupSourceRelatedFields(const T& message,
                                                        protobuf::Message& proto_message,
                                                        std::true_type) {
  proto_message.set_group_source(message.sender.group_id->string());
  proto_message.set_direct(false);
}

template <typename NodeType>
template <typename T>
void RoutingImpl<NodeType>::AddGroupSourceRelatedFields(const T&, protobuf::Message&,
                                                        std::false_type) {}

template <typename NodeType>
template <typename T>
protobuf::Message RoutingImpl<NodeType>::CreateNodeLevelMessage(const T& message) {
  protobuf::Message proto_message;
  proto_message.set_destination_id(message.receiver->string());
  proto_message.set_routing_message(false);
  proto_message.add_data(message.contents);
  proto_message.set_type(static_cast<int32_t>(MessageType::kNodeLevel));

  proto_message.set_cacheable(static_cast<int32_t>(message.cacheable));
  proto_message.set_client_node(NodeType::value);

  proto_message.set_request(true);
  proto_message.set_hops_to_live(Parameters::hops_to_live);

  AddGroupSourceRelatedFields(message, proto_message, detail::is_group_source<T>());
  AddDestinationTypeRelatedFields(proto_message, detail::is_group_destination<T>());
  proto_message.set_id(RandomUint32() % 10000);  // Enable for tracing node level messages
  return proto_message;
}

template <typename NodeType>
RoutingImpl<NodeType>::~RoutingImpl() {
  LOG(kVerbose) << "~Impl " << kNodeId_ << ", connection id "
                << connections_.kConnectionId();
  std::lock_guard<std::mutex> lock(running_mutex_);
  running_ = false;
}

template <typename NodeType>
void RoutingImpl<NodeType>::Join(const Functors& functors,
                                 const BootstrapContacts& bootstrap_contacts) {
  ConnectFunctors(functors);
  if (!bootstrap_contacts.empty()) {
    BootstrapFromTheseEndpoints(bootstrap_contacts);
  } else {
    LOG(kInfo) << "Doing a default join";
    DoJoin(bootstrap_contacts);
  }
}

template <typename NodeType>
void RoutingImpl<NodeType>::BootstrapFromTheseEndpoints(
    const BootstrapContacts& bootstrap_contacts) {
  LOG(kInfo) << "Doing a BootstrapFromTheseEndpoints Join.  Entered first bootstrap contact: "
             << bootstrap_contacts[0] << ", this node's ID: " << DebugId(kNodeId_)
             << (NodeType::value ? " Client" : "");
  if (connections_.routing_table.size() > 0) {
    for (uint16_t i = 0; i < connections_.routing_table.size(); ++i) {
      NodeInfo remove_node = connections_.routing_table.GetClosestNode(kNodeId_);
      network_.Remove(remove_node.connection_id);
      connections_.routing_table.DropNode(remove_node.id, true);
    }
    NotifyNetworkStatus(static_cast<int>(connections_.routing_table.size()));
  }
  DoJoin(bootstrap_contacts);
}


template <typename NodeType>
int RoutingImpl<NodeType>::ZeroStateJoin(const Functors& functors, const Endpoint& local_endpoint,
                                         const Endpoint& peer_endpoint, const NodeInfo& peer_info) {
  ConnectFunctors(functors);
  int result(network_.Bootstrap(
      std::vector<Endpoint>(1, peer_endpoint),
      [=](const std::string& message) { OnMessageReceived(message); },
      [=](const NodeId& lost_connection_id) { OnConnectionLost(lost_connection_id); },
      local_endpoint));

  if (result != kSuccess) {
    LOG(kError) << "Could not bootstrap zero state node from local endpoint : " << local_endpoint
                << " with peer endpoint : " << peer_endpoint;
    return result;
  }

  LOG(kInfo) << "[" << kNodeId_
             << "]'s bootstrap connection id : " << network_.bootstrap_connection_id();

  assert(!peer_info.id.IsZero() && "Zero NodeId passed");
  LOG(kVerbose) << network_.bootstrap_connection_id() << ", " << peer_info.id;
  assert((network_.bootstrap_connection_id() == peer_info.id) &&
         "Should bootstrap only with known peer for zero state network");
  LOG(kVerbose) << local_endpoint << " Bootstrapped with remote endpoint " << peer_endpoint;
  rudp::NatType nat_type(rudp::NatType::kUnknown);
  rudp::EndpointPair peer_endpoint_pair;  // zero state nodes must be directly connected endpoint
  rudp::EndpointPair this_endpoint_pair;
  peer_endpoint_pair.external = peer_endpoint_pair.local = peer_endpoint;
  this_endpoint_pair.external = this_endpoint_pair.local = local_endpoint;
  Sleep(std::chrono::milliseconds(100));  // FIXME avoiding assert in rudp
  result =
      network_.GetAvailableEndpoint(peer_info.id, peer_endpoint_pair, this_endpoint_pair, nat_type);
  if (result != rudp::kBootstrapConnectionAlreadyExists) {
    LOG(kError) << "Failed to get available endpoint to add zero state node : " << peer_endpoint;
    return result;
  }

  result = network_.Add(peer_info.id, peer_endpoint_pair, "invalid");
  if (result != kSuccess) {
    LOG(kError) << "Failed to add zero state node : " << peer_endpoint;
    return result;
  }

  ValidateAndAddToRoutingTable(network_, connections_, peer_info.id, peer_info.id,
                               peer_info.public_key, VaultNode());
  // Now poll for routing table size to have other zero state peer.
  uint8_t poll_count(0);
  do {
    Sleep(std::chrono::milliseconds(100));
  } while ((connections_.routing_table.size() == 0) && (++poll_count < 50));
  if (connections_.routing_table.size() != 0) {
    LOG(kInfo) << "Node Successfully joined zero state network, with "
               << network_.bootstrap_connection_id() << ", Routing table size - "
               << connections_.routing_table.size() << ", Node id : " << kNodeId_;

    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return kNetworkShuttingDown;
    recovery_timer_.expires_from_now(Parameters::find_node_interval);
    recovery_timer_.async_wait([=](const boost::system::error_code& error_code) {
      if (error_code != boost::asio::error::operation_aborted)
        ReSendFindNodeRequest(error_code, false);
    });
    return kSuccess;
  } else {
    LOG(kError) << "Failed to join zero state network, with bootstrap_endpoint " << peer_endpoint;
    return kNotJoined;
  }
}

template <typename NodeType>
void RoutingImpl<NodeType>::ConnectFunctors(const Functors& functors) {
  functors_ = functors;
  connections_.routing_table.InitialiseFunctors([this](
      const RoutingTableChange& routing_table_change) {
    OnRoutingTableChange(routing_table_change);
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
  if (!NodeType::value) {
    assert(!functors.message_and_caching.message_received !=
           !functors.typed_message_and_caching.single_to_group_relay.message_received);
  }
  if (functors.message_and_caching.message_received)
    message_handler_->set_message_and_caching_functor(functors.message_and_caching);
  else
    message_handler_->set_typed_message_and_caching_functor(functors.typed_message_and_caching);

  message_handler_->set_request_public_key_functor(functors.request_public_key);
  network_.set_new_bootstrap_contact_functor(functors.new_bootstrap_contact);
}

template <typename NodeType>
void RoutingImpl<NodeType>::OnRoutingTableChange(
    const RoutingTableChange& /*routing_table_change*/) {
  NodeType::Specialisation_is_required;
}

template <>
void RoutingImpl<ClientNode>::OnRoutingTableChange(const RoutingTableChange& routing_table_change);

template <>
void RoutingImpl<VaultNode>::OnRoutingTableChange(const RoutingTableChange& routing_table_change);

template <typename NodeType>
void RoutingImpl<NodeType>::NotifyNetworkStatus(int return_code) const {
  if (functors_.network_status)
    functors_.network_status(return_code);
}

template <typename NodeType>
NodeId RoutingImpl<NodeType>::kNodeId() const {
  return kNodeId_;
}

template <typename NodeType>
int RoutingImpl<NodeType>::network_status() {
  std::lock_guard<std::mutex> lock(network_status_mutex_);
  return network_status_;
}

template <typename NodeType>
void RoutingImpl<NodeType>::OnMessageReceived(const std::string& message) {
  std::lock_guard<std::mutex> lock(running_mutex_);
  if (running_)
    asio_service_.service().post([=]() { DoOnMessageReceived(message); });  // NOLINT (Fraser)
}

template <typename NodeType>
void RoutingImpl<NodeType>::DoOnMessageReceived(const std::string& message) {
  protobuf::Message pb_message;
  if (pb_message.ParseFromString(message)) {
    bool relay_message(!pb_message.has_source_id());
    LOG(kVerbose) << "   [" << DebugId(kNodeId_) << "] rcvd : " << MessageTypeString(pb_message)
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

template <typename NodeType>
void RoutingImpl<NodeType>::OnConnectionLost(const NodeId& lost_connection_id) {
  std::lock_guard<std::mutex> lock(running_mutex_);
  if (running_)
    asio_service_.service().post([=]() { DoOnConnectionLost(lost_connection_id); });  // NOLINT
                                                                                      // (Fraser)
}

template <typename NodeType>
void RoutingImpl<NodeType>::DoOnConnectionLost(const NodeId& lost_connection_id) {
  LOG(kVerbose) << DebugId(kNodeId_) << "  Routing::ConnectionLost with -----------"
                << DebugId(lost_connection_id);
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
  }

  NodeInfo dropped_node;
  bool resend(connections_.routing_table.GetNodeInfo(lost_connection_id, dropped_node) &&
              connections_.routing_table.IsThisNodeInRange(dropped_node.id,
                                                           Parameters::closest_nodes_size));

  // Checking routing table
  dropped_node = connections_.routing_table.DropNode(lost_connection_id, true);
  if (!dropped_node.id.IsZero()) {
    LOG(kWarning) << "[" << DebugId(kNodeId_) << "]"
                  << "Lost connection with routing node " << DebugId(dropped_node.id);
    random_node_helper_.Remove(dropped_node.id);
  }

  // Checking non-routing table
  if (dropped_node.id.IsZero()) {
    resend = false;
    dropped_node = connections_.client_routing_table.DropConnection(lost_connection_id);
    if (!dropped_node.id.IsZero()) {
      LOG(kWarning) << "[" << DebugId(kNodeId_) << "]"
                    << "Lost connection with non-routing node "
                    << HexSubstr(dropped_node.id.string());
    } else if (!network_.bootstrap_connection_id().IsZero() &&
               lost_connection_id == network_.bootstrap_connection_id()) {
      LOG(kWarning) << "[" << DebugId(kNodeId_) << "]"
                    << "Lost temporary connection with bootstrap node. connection id :"
                    << DebugId(lost_connection_id);
      {
        std::lock_guard<std::mutex> lock(running_mutex_);
        if (!running_)
          return;
      }
      network_.clear_bootstrap_connection_info();

      if (connections_.routing_table.size() == 0)
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
    recovery_timer_.async_wait([=](const boost::system::error_code& error_code) {
      if (error_code != boost::asio::error::operation_aborted)
        ReSendFindNodeRequest(error_code, true);
    });
  }
}

template <>
void RoutingImpl<ClientNode>::DoOnConnectionLost(const NodeId& lost_connection_id);

template <typename NodeType>
void RoutingImpl<NodeType>::ReSendFindNodeRequest(const boost::system::error_code& error_code,
                                                  bool ignore_size) {
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (error_code == boost::asio::error::operation_aborted || !running_)
      return;
  }

  if (connections_.routing_table.size() == 0) {
    LOG(kError) << "[" << DebugId(kNodeId_) << "]'s' Routing table is empty."
                << " Scheduling Re-Bootstrap .... !!!";
    ReBootstrap();
    return;
  } else if (ignore_size ||
             (connections_.routing_table.size() < Params<NodeType>::routing_table_size_threshold)) {
    if (!ignore_size)
      LOG(kInfo) << "[" << kNodeId_ << "] Routing table smaller than "
                 << Params<NodeType>::routing_table_size_threshold
                 << " nodes.  Sending another FindNodes. Routing table size < "
                 << connections_.routing_table.size() << " >";
    else
      LOG(kInfo) << "[" << DebugId(kNodeId_) << "] lost close node."
                 << "Sending another FindNodes. Current routing table size : "
                 << connections_.routing_table.size();

    int num_nodes_requested(0);
    if (ignore_size &&
        (connections_.routing_table.size() > Params<NodeType>::routing_table_size_threshold))
      num_nodes_requested = static_cast<int>(Parameters::closest_nodes_size);
    else
      num_nodes_requested = static_cast<int>(Parameters::max_routing_table_size);

    protobuf::Message find_node_rpc(rpcs::FindNodes(kNodeId_, kNodeId_, num_nodes_requested));
    network_.SendToClosestNode(find_node_rpc);

    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
    recovery_timer_.expires_from_now(Parameters::find_node_interval);
    recovery_timer_.async_wait([=](boost::system::error_code error_code_local) {
      if (error_code != boost::asio::error::operation_aborted)
        ReSendFindNodeRequest(error_code_local, false);
    });
  }
}

template <typename NodeType>
void RoutingImpl<NodeType>::ReBootstrap() {
  std::lock_guard<std::mutex> lock(running_mutex_);
  if (!running_)
    return;
  re_bootstrap_timer_.expires_from_now(Parameters::re_bootstrap_time_lag);
  re_bootstrap_timer_.async_wait([=](boost::system::error_code error_code_local) {
    if (error_code_local != boost::asio::error::operation_aborted)
      DoReBootstrap(error_code_local);
  });
}

template <typename NodeType>
void RoutingImpl<NodeType>::DoReBootstrap(const boost::system::error_code& error_code) {
  if (error_code == boost::asio::error::operation_aborted)
    return;
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
    if (connections_.routing_table.size() != 0)
      return;
  }
  LOG(kError) << "[" << DebugId(kNodeId_) << "]'s' Routing table is empty."
              << " ReBootstrapping ....";
  DoJoin(BootstrapContacts());
}

template <typename NodeType>
void RoutingImpl<NodeType>::DoJoin(const BootstrapContacts& bootstrap_contacts) {
  int return_value(DoBootstrap(bootstrap_contacts));
  if (kSuccess != return_value)
    return NotifyNetworkStatus(return_value);

  assert(!network_.bootstrap_connection_id().IsZero() &&
         "Bootstrap connection id must be populated by now.");
  FindClosestNode(boost::system::error_code(), 0);
  NotifyNetworkStatus(return_value);
}

template <typename NodeType>
int RoutingImpl<NodeType>::DoBootstrap(const BootstrapContacts& bootstrap_contacts) {
  // FIXME race condition if a new connection appears at rudp -- rudp should handle this
  assert(connections_.routing_table.size() == 0);
  recovery_timer_.cancel();
  setup_timer_.cancel();
  std::lock_guard<std::mutex> lock(running_mutex_);
  if (!running_)
    return kNetworkShuttingDown;
  if (!network_.bootstrap_connection_id().IsZero()) {
    LOG(kInfo) << "Removing bootstrap connection to rebootstrap. Connection id : "
               << DebugId(network_.bootstrap_connection_id());
    network_.Remove(network_.bootstrap_connection_id());
    network_.clear_bootstrap_connection_info();
  }

  return network_.Bootstrap(
      bootstrap_contacts, [=](const std::string& message) { OnMessageReceived(message); },
      [=](const NodeId& lost_connection_id) { OnConnectionLost(lost_connection_id); });  // NOLINT
}

template <typename NodeType>
void RoutingImpl<NodeType>::FindClosestNode(const boost::system::error_code& error_code,
                                            int attempts) {
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
  }
  if (error_code == boost::asio::error::operation_aborted)
    return;

  if (attempts == 0) {
    assert(!network_.bootstrap_connection_id().IsZero() && "Only after bootstrapping succeeds");
    assert(!network_.this_node_relay_connection_id().IsZero() &&
           "Relay connection id should be set after bootstrapping succeeds");
  } else {
    if (connections_.routing_table.size() > 0) {
      std::lock_guard<std::mutex> lock(running_mutex_);
      if (!running_)
        return;
      // Exit the loop & start recovery loop
      LOG(kVerbose) << "[" << DebugId(kNodeId_) << "] Added a node in routing table."
                    << " Terminating setup loop & Scheduling recovery loop.";
      recovery_timer_.expires_from_now(Parameters::find_node_interval);
      recovery_timer_.async_wait([=](const boost::system::error_code& error_code) {
        if (error_code != boost::asio::error::operation_aborted)
          ReSendFindNodeRequest(error_code, false);
      });
      return;
    }

    if (attempts >= Parameters::maximum_find_close_node_failures) {
      LOG(kError) << "[" << DebugId(kNodeId_) << "] failed to get closest node. ReBootstrapping...";
      // TODO(Prakash) : Remove the bootstrap node from the list
      ReBootstrap();
    }
  }

  int num_nodes_requested(1 + attempts / Parameters::find_node_repeats_per_num_requested);
  protobuf::Message find_node_rpc(rpcs::FindNodes(kNodeId_, kNodeId_, num_nodes_requested, true,
                                                  network_.this_node_relay_connection_id()));
  LOG(kVerbose) << "   [" << DebugId(kNodeId_) << "] (attempt " << attempts << ")"
                << " requesting " << num_nodes_requested << " nodes"
                << "   (id: " << find_node_rpc.id() << ")";

  rudp::MessageSentFunctor message_sent_functor([=](int message_sent) {
    if (message_sent == kSuccess)
      LOG(kVerbose) << "   [" << DebugId(kNodeId_)
                    << "] sent : " << MessageTypeString(find_node_rpc) << " to   "
                    << DebugId(network_.bootstrap_connection_id())
                    << "   (id: " << find_node_rpc.id() << ")";
    else
      LOG(kError) << "Failed to send FindNodes RPC to bootstrap connection id : "
                  << DebugId(network_.bootstrap_connection_id());
  });

  ++attempts;
  network_.SendToDirect(find_node_rpc, network_.bootstrap_connection_id(), message_sent_functor);

  std::lock_guard<std::mutex> lock(running_mutex_);
  if (!running_)
    return;
  setup_timer_.expires_from_now(Parameters::find_close_node_interval);
  setup_timer_.async_wait([=](boost::system::error_code error_code_local) {
    if (error_code_local != boost::asio::error::operation_aborted)
      FindClosestNode(error_code_local, attempts);
  });
}

template <typename NodeType>
void RoutingImpl<NodeType>::SendDirect(const NodeId& destination_id, const std::string& data,
                                       bool cacheable, ResponseFunctor response_functor) {
  assert(!functors_.typed_message_and_caching.single_to_single.message_received &&
         "Not allowed with typed Message API");
  Send(destination_id, data, DestinationType::kDirect, cacheable, response_functor);
}

template <typename NodeType>
void RoutingImpl<NodeType>::SendGroup(const NodeId& destination_id, const std::string& data,
                                      bool cacheable, ResponseFunctor response_functor) {
  assert(!functors_.typed_message_and_caching.single_to_single.message_received &&
         "Not allowed with typed Message API");
  Send(destination_id, data, DestinationType::kGroup, cacheable, response_functor);
}

template <typename NodeType>
void RoutingImpl<NodeType>::Send(const NodeId& destination_id, const std::string& data,
                                 const DestinationType& destination_type, bool cacheable,
                                 ResponseFunctor response_functor) {
  LOG(kVerbose) << "RoutingImpl<NodeType>::Send from " << DebugId(kNodeId_) << " to "
                << DebugId(destination_id);
  CheckSendParameters(destination_id, data);
  protobuf::Message proto_message =
      CreateNodeLevelPartialMessage(destination_id, destination_type, data, cacheable);
  uint16_t expected_response_count(1);
  if (response_functor) {
    if (DestinationType::kGroup == destination_type)
      expected_response_count = 4;
    proto_message.set_id(timer_.NewTaskId());
    timer_.AddTask(Parameters::default_response_timeout, response_functor, expected_response_count,
                   proto_message.id());
  } else {
    proto_message.set_id(0);
  }
  SendMessage(destination_id, proto_message);
}

template <typename NodeType>
void RoutingImpl<NodeType>::SendMessage(const NodeId& destination_id,
                                        protobuf::Message& proto_message) {
  if (connections_.routing_table.size() == 0) {  // Partial join state
    PartiallyJoinedSend(proto_message);
  } else {  // Normal node
    proto_message.set_source_id(kNodeId_.string());
    if (kNodeId_ != destination_id) {
      network_.SendToClosestNode(proto_message);
    } else if (NodeType::value) {
      LOG(kVerbose) << "Client sending request to self id";
      network_.SendToClosestNode(proto_message);
    } else {
      LOG(kInfo) << "Sending request to self";
      OnMessageReceived(proto_message.SerializeAsString());
    }
  }
}

// Partial join state
template <typename NodeType>
void RoutingImpl<NodeType>::PartiallyJoinedSend(protobuf::Message& proto_message) {
  proto_message.set_relay_id(kNodeId_.string());
  proto_message.set_relay_connection_id(network_.this_node_relay_connection_id().string());
  NodeId bootstrap_connection_id(network_.bootstrap_connection_id());
  assert(proto_message.has_relay_connection_id() && "did not set this_node_relay_connection_id");
  rudp::MessageSentFunctor message_sent([=](int result) {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
    asio_service_.service().post([=]() {
      if (rudp::kSuccess != result) {
        if (proto_message.id() != 0) {
          try {
            timer_.CancelTask(proto_message.id());
          }
          catch (const maidsafe_error& error) {
            if (error.code() != make_error_code(CommonErrors::invalid_parameter))
              throw;
          }
        }
        LOG(kError) << "Partial join Session Ended, Send not allowed anymore";
        NotifyNetworkStatus(kPartialJoinSessionEnded);
      } else {
        LOG(kVerbose) << "   [" << DebugId(kNodeId_)
                      << "] sent : " << MessageTypeString(proto_message) << " to   "
                      << HexSubstr(bootstrap_connection_id.string())
                      << "   (id: " << proto_message.id() << ")"
                      << " dst : " << HexSubstr(proto_message.destination_id())
                      << " --Partial-joined--";
      }
    });
  });
  network_.SendToDirect(proto_message, bootstrap_connection_id, message_sent);
}

template <typename NodeType>
protobuf::Message RoutingImpl<NodeType>::CreateNodeLevelPartialMessage(
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
  proto_message.set_client_node(NodeType::value);
  proto_message.set_request(true);
  proto_message.set_hops_to_live(Parameters::hops_to_live);
  uint16_t replication(1);
  if (DestinationType::kGroup == destination_type) {
    proto_message.set_visited(false);
    replication = Parameters::group_size;
  }
  proto_message.set_replication(replication);

  return proto_message;
}

// throws
template <typename NodeType>
void RoutingImpl<NodeType>::CheckSendParameters(const NodeId& destination_id,
                                                const std::string& data) {
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

template <typename NodeType>
bool RoutingImpl<NodeType>::ClosestToId(const NodeId& target_id) {
  return connections_.routing_table.IsThisNodeClosestTo(target_id, true);
}

template <typename NodeType>
NodeId RoutingImpl<NodeType>::RandomConnectedNode() {
  return connections_.routing_table.RandomConnectedNode();
}

template <typename NodeType>
bool RoutingImpl<NodeType>::EstimateInGroup(const NodeId& sender_id, const NodeId& info_id) {
  return ((connections_.routing_table.size() > Parameters::routing_table_ready_to_response) &&
          network_statistics_.EstimateInGroup(sender_id, info_id));
}

template <typename NodeType>
std::future<std::vector<NodeId>> RoutingImpl<NodeType>::GetGroup(const NodeId& group_id) {
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
  network_.SendToClosestNode(get_group_message);
  return std::move(future);
}

template <typename NodeType>
void RoutingImpl<NodeType>::RemoveNode(const NodeInfo& node, bool internal_rudp_only) {
  if (node.connection_id.IsZero() || node.id.IsZero())
    return;

  network_.Remove(node.connection_id);
  if (internal_rudp_only) {  // No recovery
    LOG(kInfo) << "Routing: removed node : " << DebugId(node.id)
               << ". Removed internal rudp connection id : " << DebugId(node.connection_id);
    return;
  }

  LOG(kInfo) << "Routing: removed node : " << DebugId(node.id)
             << ". Removed rudp connection id : " << DebugId(node.connection_id);

  // TODO(Prakash): Handle pseudo connection removal here and NRT node removal

  bool resend(
      connections_.routing_table.IsThisNodeInRange(node.id, Parameters::closest_nodes_size));
  if (resend) {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
    // Close node removed by routing, get more nodes
    LOG(kWarning) << "[" << DebugId(kNodeId_)
                  << "] Removed close node, sending find node to get more nodes.";
    recovery_timer_.expires_from_now(Parameters::recovery_time_lag);
    recovery_timer_.async_wait([=](const boost::system::error_code& error_code) {
      if (error_code != boost::asio::error::operation_aborted)
        ReSendFindNodeRequest(error_code, true);
    });
  }
}

template <typename NodeType>
bool RoutingImpl<NodeType>::ConfirmGroupMembers(const NodeId& node1, const NodeId& node2) {
  return connections_.routing_table.ConfirmGroupMembers(node1, node2);
}

template <typename NodeType>
bool RoutingImpl<NodeType>::IsConnectedVault(const NodeId& node_id) {
  return connections_.routing_table.Contains(node_id);
}

template <typename NodeType>
bool RoutingImpl<NodeType>::IsConnectedClient(const NodeId& /*node_id*/) {
  return false;
}

template <>
bool RoutingImpl<VaultNode>::IsConnectedClient(const NodeId& node_id);


template <typename NodeType>
void RoutingImpl<NodeType>::AddDestinationTypeRelatedFields(protobuf::Message& proto_message,
                                                            std::true_type) {
  proto_message.set_direct(false);
  proto_message.set_replication(Parameters::group_size);
  proto_message.set_visited(false);
  proto_message.set_group_destination(proto_message.destination_id());
}

template <typename NodeType>
void RoutingImpl<NodeType>::AddDestinationTypeRelatedFields(protobuf::Message& proto_message,
                                                            std::false_type) {
  proto_message.set_direct(true);
  proto_message.set_replication(1);
}


}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_IMPL_H_
