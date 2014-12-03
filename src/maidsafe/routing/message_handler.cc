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
    ReplyFunctor response_functor = [=](const std::string& reply_message) {
      if (reply_message.empty()) {
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
      message_out.set_client_node(routing_table_.client_mode());
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
        LOG(kWarning) << "Message to be sent back had no ID.";

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
        HandleMessage(message_out);
      }
    };
    if (message_received_functor_) {
      message_received_functor_(message.data(0), response_functor);
    } else {
      try {
        InvokeTypedMessageReceivedFunctor(message);  // typed message received
      } catch (...) {
        LOG(kError) << "InvokeTypedMessageReceivedFunctor error";
      }
    }
  } else if (IsResponse(message)) {                // response
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

  if (IsRoutingMessage(message))
    HandleRoutingMessage(message);
  else
    HandleNodeLevelMessageForThisNode(message);
}

void MessageHandler::HandleMessageAsClosestNode(protobuf::Message& message) {
  if (IsDirect(message)) {
    return HandleDirectMessageAsClosestNode(message);
  } else {
    return HandleGroupMessageAsCloseNode(message);
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

void MessageHandler::HandleGroupMessageAsCloseNode(protobuf::Message& message) {
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
      routing_table_.kNodeId() != destination_id &&
      NodeId::CloserToTarget(routing_table_.kNodeId(),
                             close_nodes.at(Parameters::group_size - 1).id, destination_id)) {
    close_nodes.erase(--close_nodes.rbegin().base());
    group_members += "[" + DebugId(routing_table_.kNodeId()) + "]";
  }

  std::string group_id(message.destination_id());
  for (const auto& i : close_nodes)
    group_members += std::string("[" + DebugId(i.id) + "]");

  for (const auto& i : close_nodes) {
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
      HandleRoutingMessage(message);
    } else {
      HandleNodeLevelMessageForThisNode(message);
    }
  } else {
    message.Clear();
  }
}

void MessageHandler::HandleMessageAsFarNode(protobuf::Message& message) {
  network_.SendToClosestNode(message);
}

void MessageHandler::HandleMessage(protobuf::Message& message) {
  if (!message.source_id().empty() && !IsAck(message) &&
      (message.destination_id() != message.source_id()) &&
      (message.destination_id() == routing_table_.kNodeId().string()) &&
      !network_utils_.firewall_.Add(NodeId(message.source_id()), message.id())) {
    return;
  }

  if (!ValidateMessage(message)) {
    LOG(kWarning) << "Validate message failed， id: " << message.id();
    BOOST_ASSERT_MSG((message.hops_to_live() > 0),
                     "Message has traversed maximum number of hops allowed");
    return;
  }

  if (!message.routing_message() && !message.client_node() && message.has_source_id()) {
    NodeInfo node_info;
    node_info.id = NodeId(message.source_id());
    if (routing_table_.CheckNode(node_info))
      response_handler_->CheckAndSendConnectRequest(node_info.id);
  }

  // Decrement hops_to_live
  message.set_hops_to_live(message.hops_to_live() - 1);

  if (IsValidCacheableGet(message) && HandleCacheLookup(message))
    return;  // forwarding message is done by cache manager or vault
  if (IsValidCacheablePut(message)) {
    StoreCacheCopy(message);  // Upper layer should take this on seperate thread
  }

  // If group message request to self id
  if (IsGroupMessageRequestToSelfId(message)) {
    return HandleGroupMessageToSelfId(message);
  }

  // If this node is a client
  if (routing_table_.client_mode()) {
    return HandleClientMessage(message);
  }

  // Relay mode message
  if (message.source_id().empty()) {
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
    return HandleMessageForThisNode(message);
  }

  if (IsRelayResponseForThisNode(message)) {
    return HandleRoutingMessage(message);
  }

  if (client_routing_table_.Contains(NodeId(message.destination_id())) && IsDirect(message)) {
    return HandleMessageForNonRoutingNodes(message);
  }

  // This node is in closest proximity to this message
  if (routing_table_.IsThisNodeInRange(NodeId(message.destination_id()),
                                       Parameters::closest_nodes_size)) {
    return HandleMessageAsClosestNode(message);
  } else {
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
  return network_.SendToClosestNode(message);
}

void MessageHandler::HandleRelayRequest(protobuf::Message& message) {
  assert(!message.has_source_id());
  if ((message.destination_id() == routing_table_.kNodeId().string()) && IsRequest(message)) {
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
  if (message.request() && message.direct() &&
      routing_table_.IsThisNodeClosestTo(NodeId(message.destination_id()))) {
    return HandleDirectRelayRequestMessageAsClosestNode(message);
  } else if (!message.direct() &&
             routing_table_.IsThisNodeInRange(NodeId(message.destination_id()),
                                              Parameters::closest_nodes_size)) {
    return HandleGroupRelayRequestMessageAsCloseNode(message);
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

void MessageHandler::HandleGroupRelayRequestMessageAsCloseNode(protobuf::Message& message) {
  message.set_source_id(routing_table_.kNodeId().string());
  HandleGroupMessageAsCloseNode(message);
}

// Special case when response of a relay comes through an alternative route.
bool MessageHandler::IsRelayResponseForThisNode(protobuf::Message& message) {
  if (IsRoutingMessage(message) && message.has_relay_id() &&
      (message.relay_id() == routing_table_.kNodeId().string())) {
    return true;
  } else {
    return false;
  }
}

bool MessageHandler::RelayDirectMessageIfNeeded(protobuf::Message& message) {
  assert(message.destination_id() == routing_table_.kNodeId().string());
  if (!message.has_relay_id()) {
    return false;
  }

  if (IsRequest(message) && message.has_actual_destination_is_relay_id() &&
          (message.destination_id() != message.relay_id())) {
    message.clear_destination_id();
    message.clear_actual_destination_is_relay_id();  // so that it is picked currectly at recepient
    network_.SendToClosestNode(message);
    return true;
  }

  // Only direct responses need to be relayed
  if (IsResponse(message) && (message.destination_id() != message.relay_id())) {
    message.clear_destination_id();  // to allow network util to identify it as relay message
    network_.SendToClosestNode(message);
    return true;
  }

  // not a relay message response, its for this node
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
    HandleRoutingMessage(message);
  } else if ((message.destination_id() == routing_table_.kNodeId().string())) {
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
  HandleGroupMessageAsCloseNode(message);
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
