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

#include "maidsafe/routing/message_handler.h"

#include <vector>

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"

#include "maidsafe/routing/client_routing_table.h"
#include "maidsafe/routing/message.h"
#include "maidsafe/routing/network.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/service.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

MessageHandler::MessageHandler(RoutingTable& routing_table,
                               ClientRoutingTable& client_routing_table, Network& network,
                               Timer<std::string>& timer, NetworkUtils& network_utils,
                               AsioService& asio_service)
    : routing_table_(routing_table),
      client_routing_table_(client_routing_table),
      network_utils_(network_utils),
      network_(network),
      cache_manager_(routing_table_.client_mode()
                         ? nullptr
                         : (new CacheManager(routing_table_.kNodeId(), network_))),
      timer_(timer),
      public_key_holder_(asio_service, network),
      response_handler_(new ResponseHandler(routing_table, client_routing_table, network_,
                                            public_key_holder_)),
      service_(new Service(routing_table, client_routing_table, network_, public_key_holder_)),
      message_received_functor_(),
      typed_message_received_functors_() {}

void MessageHandler::HandleRoutingMessage(protobuf::Message& message) {
  bool request(message.request());
  switch (static_cast<MessageType>(message.type())) {
    case MessageType::kPing:
      message.request() ? service_->Ping(message) : response_handler_->Ping(message);
      break;
    case MessageType::kConnect:
      message.request() ? service_->Connect(message) : response_handler_->Connect(message);
      break;
    case MessageType::kFindNodes:
      message.request() ? service_->FindNodes(message) : response_handler_->FindNodes(message);
      break;
    case MessageType::kConnectSuccess:
      message.request() ? service_->ConnectSuccess(message)
                        : response_handler_->ConnectSuccess(message);
      break;
    case MessageType::kConnectSuccessAcknowledgement:
      response_handler_->ConnectSuccessAcknowledgement(message);
      break;
    case MessageType::kGetGroup:
      message.request() ? service_->GetGroup(message)
                        : response_handler_->GetGroup(timer_, message);
      break;
    case MessageType::kAcknowledgement:
      network_utils_.acknowledgement_.HandleMessage(message.ack_id());
      message.Clear();
      break;
    case MessageType::kInformClientOfNewCloseNode:
      assert(message.request());
      response_handler_->InformClientOfNewCloseNode(message);
      break;
    default:  // unknown (silent drop)
      return;
  }

  if (!request || !message.IsInitialized())
    return;

  message.set_ack_id(network_utils_.acknowledgement_.GetId());

  if (message.destination_id() == routing_table_.kNodeId().string())
    if (RelayDirectMessageIfNeeded(message))
      return;

  if (routing_table_.size() == 0)  // This node can only send to bootstrap_endpoint
    network_.SendToDirect(message, network_.bootstrap_connection_id(),
                          network_.bootstrap_connection_id());
  else
    network_.SendToClosestNode(message);
}

void MessageHandler::HandleNodeLevelMessageForThisNode(protobuf::Message& message) {
  if (IsRequest(message) &&
      !IsClientToClientMessageWithDifferentNodeIds(message, routing_table_.client_mode())) {
    LOG(kSuccess) << " [" << DebugId(routing_table_.kNodeId())
                  << "] rcvd : " << MessageTypeString(message) << " from "
                  << HexSubstr(message.source_id()) << "   (id: " << message.id()
                  << ")  --NodeLevel--";
    ReplyFunctor response_functor = [=](const std::string & reply_message) {
      if (reply_message.empty()) {
        LOG(kInfo) << "Empty response for message id :" << message.id();
        return;
      }
      LOG(kSuccess) << " [" << routing_table_.kNodeId() << "] repl : "
                    << MessageTypeString(message) << " from " << HexSubstr(message.source_id())
                    << "   (id: " << message.id() << ")  --NodeLevel Replied--";
      protobuf::Message message_out;
      message_out.set_request(false);
      message_out.set_ack_id(RandomUint32());
      message_out.set_hops_to_live(Parameters::hops_to_live);
      message_out.set_destination_id(message.source_id());
      message_out.set_type(message.type());
      message_out.set_direct(true);
      message_out.clear_data();
      message_out.set_client_node(message.client_node());
      message_out.set_routing_message(message.routing_message());
      message_out.add_data(reply_message);
      if (IsCacheableGet(message))
        message_out.set_cacheable(static_cast<int32_t>(Cacheable::kPut));
      message_out.set_last_id(routing_table_.kNodeId().string());
      message_out.set_source_id(routing_table_.kNodeId().string());
      message_out.set_ack_id(network_utils_.acknowledgement_.GetId());
      if (message.has_id())
        message_out.set_id(message.id());
      else
        LOG(kInfo) << "Message to be sent back had no ID.";

      if (message.has_relay_id())
        message_out.set_relay_id(message.relay_id());

      if (message.has_relay_connection_id()) {
        message_out.set_relay_connection_id(message.relay_connection_id());
      }
      if (routing_table_.client_mode() &&
          routing_table_.kNodeId().string() == message_out.destination_id()) {
        network_.SendToClosestNode(message_out);
        return;
      }
      if (routing_table_.kNodeId().string() != message_out.destination_id()) {
        network_.SendToClosestNode(message_out);
      } else {
        LOG(kInfo) << "Sending response to self." << " id: " << message.id();
        HandleMessage(message_out);
      }
    };
    if (message_received_functor_) {
      LOG(kVerbose) << "calling message_received_functor_ " << " id: " << message.id();
      message_received_functor_(message.data(0), response_functor);
    } else {
      LOG(kVerbose) << "calling InvokeTypedMessageReceivedFunctor " << " id: " << message.id();
      try {
        InvokeTypedMessageReceivedFunctor(message);  // typed message received
      } catch (...) {
        LOG(kError) << "InvokeTypedMessageReceivedFunctor error";
      }
    }
  } else if (IsResponse(message)) {                // response
    LOG(kInfo) << "[" << DebugId(routing_table_.kNodeId())
               << "] rcvd : " << MessageTypeString(message) << " from "
               << HexSubstr(message.source_id()) << "   (id: " << message.id()
               << ")  --NodeLevel--";
    try {
      if (!message.has_id() || message.data_size() != 1)
        BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
      timer_.AddResponse(message.id(), message.data(0));
    }
    catch (const maidsafe_error& e) {
      LOG(kError) << e.what();
      return;
    }
    if (message.has_average_distace())
      network_utils_.statistics_.UpdateNetworkAverageDistance(NodeId(message.average_distace()));
  } else {
    LOG(kWarning) << "This node [" << DebugId(routing_table_.kNodeId())
                  << " Dropping message as client to client message not allowed."
                  << PrintMessage(message);
    message.Clear();
  }
}

void MessageHandler::HandleMessageForThisNode(protobuf::Message& message) {
  if (RelayDirectMessageIfNeeded(message))
    return;

  LOG(kVerbose) << "Message for this node."
                << " id: " << message.id();
  if (IsRoutingMessage(message))
    HandleRoutingMessage(message);
  else
    HandleNodeLevelMessageForThisNode(message);
}

void MessageHandler::HandleMessageAsClosestNode(protobuf::Message& message) {
  LOG(kVerbose) << "This node is in closest proximity to this message destination ID [ "
                << HexSubstr(message.destination_id()) << " ]."
                << " id: " << message.id();
  if (IsDirect(message)) {
    return HandleDirectMessageAsClosestNode(message);
  } else {
    return HandleGroupMessageAsClosestNode(message);
  }
}

void MessageHandler::HandleDirectMessageAsClosestNode(protobuf::Message& message) {
  assert(message.direct());
  // Dropping direct messages if this node is closest and destination node is not in routing_table_
  // or client_routing_table_.
  NodeId destination_node_id(message.destination_id());
  if (routing_table_.IsThisNodeClosestTo(destination_node_id)) {
    if (routing_table_.Contains(destination_node_id) ||
        client_routing_table_.Contains(destination_node_id)) {
      return network_.SendToClosestNode(message);
    } else if (!message.has_visited() || !message.visited()) {
      message.set_visited(true);
      return network_.SendToClosestNode(message);
    } else {
      network_utils_.acknowledgement_.AdjustAckHistory(message);
      network_.SendAck(message);
      LOG(kWarning) << "Dropping message. This node [" << routing_table_.kNodeId()
                    << "] is the closest but is not connected to destination node ["
                    << HexSubstr(message.destination_id())
                    << "], Src ID: " << HexSubstr(message.source_id())
                    << ", Relay ID: " << HexSubstr(message.relay_id()) << " id: " << message.id()
                    << PrintMessage(message);
      return;
    }
  } else {
    // if (IsCacheableRequest(message))
    //   return HandleCacheLookup(message);  // forwarding message is done by cache manager
    // else if (IsCacheableResponse(message))
    //   StoreCacheCopy(message);  //  Upper layer should take this on seperate thread

    return network_.SendToClosestNode(message);
  }
}

void MessageHandler::HandleGroupMessageAsClosestNode(protobuf::Message& message) {
  assert(!message.direct());

  NodeId destination_id(message.destination_id());
  auto close_nodes(routing_table_.GetClosestNodes(destination_id, Parameters::group_size + 1));
  close_nodes.erase(std::remove_if(std::begin(close_nodes), std::end(close_nodes),
                                   [&destination_id](const NodeInfo& node_info) {
                                     return node_info.id == destination_id;
                                   }), std::end(close_nodes));
  while (close_nodes.size() > Parameters::group_size)
    close_nodes.pop_back();

  std::string group_members;
  if (close_nodes.size() == Parameters::group_size &&
      NodeId::CloserToTarget(routing_table_.kNodeId(),
                             close_nodes.at(Parameters::group_size - 1).id, destination_id)) {
    //MAID-9 Check below
    close_nodes.erase(--close_nodes.rbegin().base());
    group_members += "[" + DebugId(routing_table_.kNodeId()) + "]";
  }

  std::string group_id(message.destination_id());

  for (const auto& i : close_nodes)
    group_members += std::string("[" + DebugId(i.id) + "]");    
  LOG(kInfo) << "Group nodes for group_id " << HexSubstr(group_id) << " : " << group_members;

  for (const auto& i : close_nodes) {
    LOG(kInfo) << "[" << routing_table_.kNodeId() << "] - "
               << "Replicating message to : " << HexSubstr(i.id.string())
               << " [ group_id : " << HexSubstr(group_id) << "]"
               << " id: " << message.id();
    message.clear_ack_node_ids();
    message.set_ack_id(0);
    message.set_destination_id(i.id.string());
    NodeInfo node;
    if (routing_table_.GetNodeInfo(i.id, node)) {
      network_.SendToDirect(message, node.id, node.connection_id);
    } else {
      network_.SendToClosestNode(message);
    }
  }

  if (!network_utils_.firewall_.Add(NodeId(group_id), message.id())) {
    message.Clear();
    return;
  }

  network_.SendAck(message);

  if (close_nodes.size() < Parameters::group_size) {
    message.clear_ack_node_ids();
    message.set_destination_id(routing_table_.kNodeId().string());

    if (IsRoutingMessage(message)) {
      LOG(kVerbose) << "HandleGroupMessageAsClosestNode if, msg id: " << message.id();
      HandleRoutingMessage(message);
    } else {
      LOG(kVerbose) << "HandleGroupMessageAsClosestNode else, msg id: " << message.id();
      HandleNodeLevelMessageForThisNode(message);
    }
  } else {
    message.Clear();
  }
}

void MessageHandler::HandleMessageAsFarNode(protobuf::Message& message) {
  LOG(kVerbose) << "[" << routing_table_.kNodeId()
                << "] is not in closest proximity to this message destination ID [ "
                << HexSubstr(message.destination_id()) << " ]; sending on."
                << " id: " << message.id();
  network_.SendToClosestNode(message);
}

void MessageHandler::HandleMessage(protobuf::Message& message) {
  LOG(kVerbose) << "[" << routing_table_.kNodeId() << "]"
                << " MessageHandler::HandleMessage handle message with id: " << message.id();
  if (!message.source_id().empty() && !IsAck(message) &&
      (message.destination_id() != message.source_id()) &&
      (message.destination_id() == routing_table_.kNodeId().string()) &&
      !network_utils_.firewall_.Add(NodeId(message.source_id()), message.id())) {
    LOG(kVerbose) << "Filtered " << NodeId(message.source_id()) << ", " << message.id();
    return;
  }

  if (!ValidateMessage(message)) {
    LOG(kWarning) << "Validate message failed， id: " << message.id();
    BOOST_ASSERT_MSG((message.hops_to_live() > 0),
                     "Message has traversed maximum number of hops allowed");
    return;
  }

  // Decrement hops_to_live
  message.set_hops_to_live(message.hops_to_live() - 1);

  if (IsValidCacheableGet(message) && HandleCacheLookup(message))
    return;  // forwarding message is done by cache manager or vault
  if (IsValidCacheablePut(message)) {
    LOG(kVerbose) << "StoreCacheCopy: " << message.id();
    StoreCacheCopy(message);  // Upper layer should take this on seperate thread
  }

  // If group message request to self id
  if (IsGroupMessageRequestToSelfId(message)) {
    LOG(kInfo) << "MessageHandler::HandleMessage " << message.id() << " HandleGroupMessageToSelfId";
    return HandleGroupMessageToSelfId(message);
  }

  // If this node is a client
  if (routing_table_.client_mode()) {
    LOG(kInfo) << "MessageHandler::HandleMessage " << message.id() << " HandleClientMessage";
    return HandleClientMessage(message);
  }

  // Relay mode message
  if (message.source_id().empty()) {
    LOG(kInfo) << "MessageHandler::HandleMessage " << message.id() << " HandleRelayRequest";
    return HandleRelayRequest(message);
  }

  // Invalid source id, unknown message
  if (NodeId(message.source_id()).IsZero()) {
    LOG(kWarning) << "Stray message dropped, need valid source ID for processing."
                  << " id: " << message.id();
    return;
  }

  // Direct message
  if (message.destination_id() == routing_table_.kNodeId().string()) {
    LOG(kInfo) << "MessageHandler::HandleMessage " << message.id() << " HandleMessageForThisNode";
    return HandleMessageForThisNode(message);
  }

  if (IsRelayResponseForThisNode(message)) {
    LOG(kInfo) << "MessageHandler::HandleMessage " << message.id() << " HandleRoutingMessage";
    return HandleRoutingMessage(message);
  }

  if (client_routing_table_.Contains(NodeId(message.destination_id())) && IsDirect(message)) {
    LOG(kInfo) << "MessageHandler::HandleMessage " << message.id()
               << " HandleMessageForNonRoutingNodes";
    return HandleMessageForNonRoutingNodes(message);
  }

  // This node is in closest proximity to this message
  if (routing_table_.IsThisNodeInRange(NodeId(message.destination_id()),
                                       Parameters::closest_nodes_size)) {
    LOG(kInfo) << "MessageHandler::HandleMessage " << message.id() << " HandleMessageAsClosestNode";
    return HandleMessageAsClosestNode(message);
  } else {
    LOG(kInfo) << "MessageHandler::HandleMessage " << message.id() << " HandleMessageAsFarNode";
    return HandleMessageAsFarNode(message);
  }
}

void MessageHandler::HandleMessageForNonRoutingNodes(protobuf::Message& message) {
  auto client_routing_nodes(client_routing_table_.GetNodesInfo(NodeId(message.destination_id())));
  assert(!client_routing_nodes.empty() && message.direct());
// Below bit is not needed currently as SendToClosestNode will do this check anyway
// TODO(Team) consider removing the check from SendToClosestNode() after
// adding more client tests
  if (IsClientToClientMessageWithDifferentNodeIds(message, true)) {
    LOG(kWarning) << "This node [" << DebugId(routing_table_.kNodeId())
                  << " Dropping message as client to client message not allowed."
                  << PrintMessage(message);
    network_utils_.acknowledgement_.AdjustAckHistory(message);
    network_.SendAck(message);
    return;
  }
  LOG(kInfo) << "This node has message destination in its ClientRoutingTable. Dest id : "
             << HexSubstr(message.destination_id()) << " message id: " << message.id();
  return network_.SendToClosestNode(message);
}

void MessageHandler::HandleRelayRequest(protobuf::Message& message) {
  assert(!message.has_source_id());
  if ((message.destination_id() == routing_table_.kNodeId().string()) && IsRequest(message)) {
    LOG(kVerbose) << "Relay request with this node's ID as destination ID"
                  << " id: " << message.id();
    // If group message request to this node's id sent by relay requester node
    if ((message.destination_id() == routing_table_.kNodeId().string()) && message.request() &&
        !message.direct()) {
      message.set_source_id(routing_table_.kNodeId().string());
      return HandleGroupMessageToSelfId(message);
    } else {
      return HandleMessageForThisNode(message);
    }
  }

  // This node may be closest for group messages.
  if (message.request() && routing_table_.IsThisNodeClosestTo(NodeId(message.destination_id()))) {
    if (message.direct()) {
      return HandleDirectRelayRequestMessageAsClosestNode(message);
    } else {
      return HandleGroupRelayRequestMessageAsClosestNode(message);
    }
  }

  // This node is now the src ID for the relay message and will send back response to original node.
  message.set_source_id(routing_table_.kNodeId().string());
  network_.SendToClosestNode(message);
}

void MessageHandler::HandleDirectRelayRequestMessageAsClosestNode(protobuf::Message& message) {
  assert(message.direct());
  // Dropping direct messages if this node is closest and destination node is not in routing_table_
  // or client_routing_table_.
  NodeId destination_node_id(message.destination_id());
  if (routing_table_.IsThisNodeClosestTo(destination_node_id)) {
    if (routing_table_.Contains(destination_node_id) ||
        client_routing_table_.Contains(destination_node_id)) {
      message.set_source_id(routing_table_.kNodeId().string());
      return network_.SendToClosestNode(message);
    } else {
      LOG(kWarning) << "Dropping message. This node [" << DebugId(routing_table_.kNodeId())
                    << "] is the closest but is not connected to destination node ["
                    << HexSubstr(message.destination_id())
                    << "], Src ID: " << HexSubstr(message.source_id())
                    << ", Relay ID: " << HexSubstr(message.relay_id()) << " id: " << message.id()
                    << PrintMessage(message);
      return;
    }
  } else {
    return network_.SendToClosestNode(message);
  }
}

void MessageHandler::HandleGroupRelayRequestMessageAsClosestNode(protobuf::Message& message) {
  assert(!message.direct());
  bool have_node_with_group_id(routing_table_.Contains(NodeId(message.destination_id())));
  // This node is not closest to the destination node for non-direct message.
  if (!routing_table_.IsThisNodeClosestTo(NodeId(message.destination_id()), !IsDirect(message))) {
    LOG(kInfo) << "This node is not closest, passing it on."
               << " id: " << message.id();
    message.set_source_id(routing_table_.kNodeId().string());
    return network_.SendToClosestNode(message);
  }

  // This node is closest so will send to all replicant nodes
  auto replication(static_cast<unsigned int>(message.replication()));
  if ((replication < 1) || (replication > Parameters::group_size)) {
    LOG(kError) << "Dropping invalid non-direct message."
                << " id: " << message.id();
    return;
  }

  --replication;  // This node will be one of the group member.
  message.set_direct(true);
  if (have_node_with_group_id)
    ++replication;
  auto close(routing_table_.GetClosestNodes(NodeId(message.destination_id()), replication));

  if (have_node_with_group_id)
    close.erase(close.begin());
  NodeId group_id(NodeId(message.destination_id()));
  std::string group_members("[" + DebugId(routing_table_.kNodeId()) + "]");

  for (const auto& i : close)
    group_members += std::string("[" + DebugId(i.id) + "]");
  LOG(kInfo) << "Group members for group_id " << group_id << " are: " << group_members;
  // This node relays back the responses
  message.set_source_id(routing_table_.kNodeId().string());
  for (const auto& i : close) {
    LOG(kInfo) << "Replicating message to : " << i.id
               << " [ group_id : " << group_id << "]"
               << " id: " << message.id();
    message.set_destination_id(i.id.string());
    NodeInfo node;
    if (routing_table_.GetNodeInfo(i.id, node)) {
      network_.SendToDirect(message, node.id, node.connection_id);
    }
  }

  message.set_destination_id(routing_table_.kNodeId().string());
//  message.clear_source_id();
  if (IsRoutingMessage(message))
    HandleRoutingMessage(message);
  else
    HandleNodeLevelMessageForThisNode(message);
}

// Special case when response of a relay comes through an alternative route.
bool MessageHandler::IsRelayResponseForThisNode(protobuf::Message& message) {
  if (IsRoutingMessage(message) && message.has_relay_id() &&
      (message.relay_id() == routing_table_.kNodeId().string())) {
    LOG(kVerbose) << "Relay response through alternative route";
    return true;
  } else {
    return false;
  }
}

bool MessageHandler::RelayDirectMessageIfNeeded(protobuf::Message& message) {
  assert(message.destination_id() == routing_table_.kNodeId().string());
  if (!message.has_relay_id()) {
    //    LOG(kVerbose) << "Message don't have relay ID.";
    return false;
  }

  if (IsRequest(message) && message.has_actual_destination_is_relay_id() &&
          (message.destination_id() != message.relay_id())) {
    message.clear_destination_id();
    message.clear_actual_destination_is_relay_id();  // so that it is picked currectly at recepient
    LOG(kVerbose) << "Relaying request to " << HexSubstr(message.relay_id())
                  << " id: " << message.id();
    network_.SendToClosestNode(message);
    return true;
  }

  // Only direct responses need to be relayed
  if (IsResponse(message) && (message.destination_id() != message.relay_id())) {
    message.clear_destination_id();  // to allow network util to identify it as relay message
    LOG(kVerbose) << "Relaying response to " << HexSubstr(message.relay_id())
                  << " id: " << message.id();
    network_.SendToClosestNode(message);
    return true;
  }

  // not a relay message response, its for this node
  //    LOG(kVerbose) << "Not a relay message response, it's for this node";
  return false;
}

void MessageHandler::HandleClientMessage(protobuf::Message& message) {
  assert(routing_table_.client_mode() && "Only client node should handle client messages");
  if (message.source_id().empty()) {  // No relays allowed on client.
    LOG(kWarning) << "Stray message at client node. No relays allowed."
                  << " id: " << message.id();
    return;
  }
  if (IsRoutingMessage(message)) {
    LOG(kVerbose) << "Client Routing Response for " << DebugId(routing_table_.kNodeId())
                  << " from " << HexSubstr(message.source_id()) << " id: " << message.id();
    HandleRoutingMessage(message);
  } else if ((message.destination_id() == routing_table_.kNodeId().string())) {
    LOG(kVerbose) << "Client NodeLevel Response for " << DebugId(routing_table_.kNodeId())
                  << " from " << HexSubstr(message.source_id()) << " id: " << message.id();
    HandleNodeLevelMessageForThisNode(message);
  } else {
    LOG(kWarning) << DebugId(routing_table_.kNodeId()) << " silently drop message "
                  << " from " << HexSubstr(message.source_id()) << " id: " << message.id();
  }
}

// Special case : If group message request to self id
bool MessageHandler::IsGroupMessageRequestToSelfId(protobuf::Message& message) {
  return ((message.source_id() == routing_table_.kNodeId().string()) &&
          (message.destination_id() == routing_table_.kNodeId().string()) && message.request() &&
          !message.direct());
}

void MessageHandler::HandleGroupMessageToSelfId(protobuf::Message& message) {
  assert(message.source_id() == routing_table_.kNodeId().string());
  assert(message.destination_id() == routing_table_.kNodeId().string());
  assert(message.request());
  assert(!message.direct());
  LOG(kInfo) << "Sending group message to self id. Passing on to the closest peer to replicate";
  network_.SendToClosestNode(message);
}

void MessageHandler::InvokeTypedMessageReceivedFunctor(const protobuf::Message& proto_message) {
  if ((!proto_message.has_group_source() && !proto_message.has_group_destination()) &&
      typed_message_received_functors_.single_to_single) {  // Single to Single
    typed_message_received_functors_.single_to_single(CreateSingleToSingleMessage(proto_message));
  } else if ((!proto_message.has_group_source() && proto_message.has_group_destination()) &&
             typed_message_received_functors_.single_to_group) {
    // Single to Group
    if (proto_message.has_relay_id() && proto_message.has_relay_connection_id()) {
      typed_message_received_functors_.single_to_group_relay(
          CreateSingleToGroupRelayMessage(proto_message));
    } else {
      typed_message_received_functors_.single_to_group(CreateSingleToGroupMessage(proto_message));
    }
  } else if ((proto_message.has_group_source() && !proto_message.has_group_destination()) &&
             typed_message_received_functors_.group_to_single) {
    typed_message_received_functors_.group_to_single(CreateGroupToSingleMessage(proto_message));
  } else if ((proto_message.has_group_source() && proto_message.has_group_destination()) &&
             typed_message_received_functors_.group_to_group) {  // Group to Group
    typed_message_received_functors_.group_to_group(CreateGroupToGroupMessage(proto_message));
  } else {
    assert(false);
  }
}

void MessageHandler::set_message_and_caching_functor(MessageAndCachingFunctors functors) {
  message_received_functor_ = functors.message_received;
  if (!routing_table_.client_mode())
    cache_manager_->InitialiseFunctors(functors);
  // Initialise caching functors here
}

void MessageHandler::set_typed_message_and_caching_functor(TypedMessageAndCachingFunctor functors) {
  typed_message_received_functors_.single_to_single = functors.single_to_single.message_received;
  typed_message_received_functors_.single_to_group = functors.single_to_group.message_received;
  typed_message_received_functors_.group_to_single = functors.group_to_single.message_received;
  typed_message_received_functors_.group_to_group = functors.group_to_group.message_received;
  typed_message_received_functors_.single_to_group_relay =
      functors.single_to_group_relay.message_received;
  // Initialise caching functors here
  if (!routing_table_.client_mode())
    cache_manager_->InitialiseFunctors(functors);
}

void MessageHandler::set_request_public_key_functor(
    RequestPublicKeyFunctor request_public_key_functor) {
  response_handler_->set_request_public_key_functor(request_public_key_functor);
  service_->set_request_public_key_functor(request_public_key_functor);
}

bool MessageHandler::HandleCacheLookup(protobuf::Message& message) {
  assert(!routing_table_.client_mode());
  assert(IsCacheableGet(message));
  return cache_manager_->HandleGetFromCache(message);
}

void MessageHandler::StoreCacheCopy(const protobuf::Message& message) {
  assert(!routing_table_.client_mode());
  assert(IsCacheablePut(message));
  cache_manager_->AddToCache(message);
}

bool MessageHandler::IsValidCacheableGet(const protobuf::Message& message) {
  // TODO(Prakash): need to differentiate between typed and un typed api
  return (IsCacheableGet(message) && IsNodeLevelMessage(message) && Parameters::caching &&
          !routing_table_.client_mode());
}

bool MessageHandler::IsValidCacheablePut(const protobuf::Message& message) {
  // TODO(Prakash): need to differentiate between typed and un typed api
  return (IsNodeLevelMessage(message) && Parameters::caching && !routing_table_.client_mode() &&
          IsCacheablePut(message));
}

}  // namespace routing

}  // namespace maidsafe
