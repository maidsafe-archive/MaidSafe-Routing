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

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/passport/types.h"

#include "maidsafe/routing/bootstrap_file_operations.h"
#include "maidsafe/routing/message.h"
#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/node_info.h"
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
template <>
void RoutingImpl<VaultNode>::Send(const GroupToSingleRelayMessage& message) {
  assert(!functors_.message_and_caching.message_received &&
         "Not allowed with string type message API");
  protobuf::Message proto_message = CreateNodeLevelMessage(message);
  // append relay information
  SendMessage(message.receiver.relay_node, proto_message);
}

template <>
template <>
void RoutingImpl<ClientNode>::Send(const GroupToSingleRelayMessage& message) {
  assert(!functors_.message_and_caching.message_received &&
         "Not allowed with string type message API");
  protobuf::Message proto_message = CreateNodeLevelMessage(message);
  // append relay information
  SendMessage(message.receiver.relay_node, proto_message);
}

// BEFORE_MONDAY

template <>
template <>
protobuf::Message RoutingImpl<VaultNode>::CreateNodeLevelMessage(
    const GroupToSingleRelayMessage& message) {
  protobuf::Message proto_message;
  proto_message.set_destination_id(message.receiver.relay_node->string());
  proto_message.set_routing_message(false);
  proto_message.add_data(message.contents);
  proto_message.set_type(static_cast<int32_t>(MessageType::kNodeLevel));

  proto_message.set_cacheable(static_cast<int32_t>(message.cacheable));
  proto_message.set_client_node(VaultNode::value);

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

template <>
template <>
protobuf::Message RoutingImpl<ClientNode>::CreateNodeLevelMessage(
    const GroupToSingleRelayMessage& message) {
  protobuf::Message proto_message;
  proto_message.set_destination_id(message.receiver.relay_node->string());
  proto_message.set_routing_message(false);
  proto_message.add_data(message.contents);
  proto_message.set_type(static_cast<int32_t>(MessageType::kNodeLevel));

  proto_message.set_cacheable(static_cast<int32_t>(message.cacheable));
  proto_message.set_client_node(ClientNode::value);

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

template <>
bool RoutingImpl<VaultNode>::IsConnectedClient(const NodeId& node_id) {
  return connections_.client_routing_table.IsConnected(node_id);
}

template <>
void RoutingImpl<ClientNode>::OnRoutingTableChange(const RoutingTableChange& routing_table_change) {
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

  if (connections_.routing_table.size() < Parameters::max_routing_table_size_for_client)
    network_.SendToClosestNode(rpcs::FindNodes(Params<ClientNode>::max_routing_table_size,
                                               PeerNodeId(kNodeId_.data), kNodeId_));
}

template <>
void RoutingImpl<ClientNode>::DoOnConnectionLost(const NodeId& lost_connection_id) {
  LOG(kVerbose) << DebugId(kNodeId_) << "  Routing::ConnectionLost with -----------"
                << DebugId(lost_connection_id);
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
  }

  NodeInfo dropped_node;
  bool resend(connections_.routing_table.GetNodeInfo(lost_connection_id, dropped_node) &&
              connections_.routing_table.IsThisNodeInRange(
                  dropped_node.id, Parameters::max_routing_table_size_for_client));

  // Checking routing table
  dropped_node = connections_.routing_table.DropNode(lost_connection_id, true);
  if (!dropped_node.id.IsZero()) {
    LOG(kWarning) << "[" << DebugId(kNodeId_) << "]"
                  << "Lost connection with routing node " << DebugId(dropped_node.id);
    random_node_helper_.Remove(dropped_node.id);
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

}  // namespace routing

}  // namespace maidsafe
