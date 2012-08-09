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

#include "maidsafe/routing/message_handler.h"

#include "maidsafe/common/log.h"

#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/service.h"
#include "maidsafe/routing/timer.h"
#include "maidsafe/routing/utils.h"


namespace maidsafe {

namespace routing {

MessageHandler::MessageHandler(AsioService& asio_service,
                               RoutingTable& routing_table,
                               NonRoutingTable& non_routing_table,
                               NetworkUtils& network,
                               Timer& timer)
    : asio_service_(asio_service),
      routing_table_(routing_table),
      non_routing_table_(non_routing_table),
      network_(network),
      timer_(timer),
      cache_manager_(),
      response_handler_(new ResponseHandler(routing_table,
                                            non_routing_table, network_)),
      message_received_functor_() {}

bool MessageHandler::CheckCacheData(protobuf::Message& message) {
  if (message.type() == static_cast<int32_t>(MessageType::kMinRouting)) {
    cache_manager_.AddToCache(message);
  } else if (message.type() == static_cast<int32_t>(MessageType::kMaxRouting)) {
    if (cache_manager_.GetFromCache(message)) {
      message.set_source_id(routing_table_.kKeys().identity);
      network_.SendToClosestNode(message);
      return true;
    }
  } else {
    return false;  // means this message is not finished processing
  }
  return false;
}

void MessageHandler::HandleRoutingMessage(protobuf::Message& message) {
  LOG(kInfo) << "MessageHandler::RoutingMessage";
  bool is_response(IsResponse(message));
  switch (static_cast<MessageType>(message.type())) {
    case MessageType::kPingRequest :
      service::Ping(routing_table_, message);
      break;
    case MessageType::kPingResponse :
      response_handler_->Ping(message);
      break;
    case MessageType::kConnectRequest :
      service::Connect(routing_table_, non_routing_table_, network_, message,
                       response_handler_->request_public_key_functor());
      break;
    case MessageType::kConnectResponse :
      response_handler_->Connect(message);
      break;
    case MessageType::kFindNodesRequest :
      service::FindNodes(routing_table_, message);
      break;
    case MessageType::kFindNodesResponse :
      response_handler_->FindNodes(message);
      break;
    case MessageType::kProxyConnectRequest :
      service::ProxyConnect(routing_table_, network_, message);
      break;
    case MessageType::kProxyConnectResponse :
      response_handler_->ProxyConnect(message);
      break;
    default:  // unknown (silent drop)
      return;
  }
  if (is_response)
    return;

  if (!message.IsInitialized()) {
    LOG(kWarning) << "Uninitialised message dropped.";
    return;
  }

  if (routing_table_.Size() == 0)  // This node can only send to bootstrap_endpoint
    network_.SendToDirectEndpoint(message, network_.bootstrap_endpoint());
  else
    network_.SendToClosestNode(message);
}

void MessageHandler::HandleNodeLevelMessageForThisNode(protobuf::Message& message) {
  if (IsRequest(message)) {
    LOG(kInfo) << "Node Level Request for " << HexSubstr(routing_table_.kKeys().identity)
               << " from " << HexSubstr(message.source_id());

    ReplyFunctor response_functor = [=](const std::string& reply_message) {
        if (reply_message.empty())
          return;
        protobuf::Message message_out;
        message_out.set_type(-message.type());
        message_out.set_destination_id(message.source_id());
        message_out.set_direct(static_cast<int32_t>(ConnectType::kSingle));
        message_out.clear_data();
        message_out.add_data(reply_message);
        message_out.set_last_id(routing_table_.kKeys().identity);
        message_out.set_source_id(routing_table_.kKeys().identity);
        if (message.has_id())
          message_out.set_id(message.id());
        else
          LOG(kInfo) << "Message to be sent back had no ID.";

        if (message.has_relay_id())
          message_out.set_relay_id(message.relay_id());

        if (message.has_relay()) {
          auto relay_endpoint = GetEndpointFromProtobuf(message.relay());
          SetProtobufEndpoint(relay_endpoint, message_out.mutable_relay());
        }

        if (routing_table_.kKeys().identity != message_out.destination_id()) {
          network_.SendToClosestNode(message_out);
        } else {
          LOG(kInfo) << "Sending response to self.";
          HandleMessage(message_out);
        }
    };

    if (message_received_functor_)
      message_received_functor_(static_cast<int>(message.type()), message.data(0), NodeId(),
                                response_functor);
  } else {  // response
    LOG(kInfo) << "Node Level Response for " << HexSubstr(routing_table_.kKeys().identity)
               << " from " << HexSubstr(message.source_id());
    timer_.ExecuteTask(message);
  }
}

void MessageHandler::HandleDirectMessage(protobuf::Message& message) {
  if (RelayDirectMessageIfNeeded(message))
    return;

  LOG(kVerbose) << "Direct Message for this node.";
  if (IsRoutingMessage(message))
    HandleRoutingMessage(message);
  else
    HandleNodeLevelMessageForThisNode(message);
}

void MessageHandler::HandleMessageAsClosestNode(protobuf::Message& message) {
  LOG(kVerbose) << "This node is in closest proximity to this message destination ID [ "
                <<  HexSubstr(message.destination_id())
                << " ].";
  if (IsDirect(message)) {
    return HandleDirectMessageAsClosestNode(message);
  } else {
    return HandleGroupMessageAsClosestNode(message);
  }
}

void MessageHandler::HandleDirectMessageAsClosestNode(protobuf::Message& message) {
  assert(message.direct() == static_cast<int32_t>(ConnectType::kSingle));
  // Dropping direct messages if this node is closest and destination node is not in routing_table_
  // or non_routing_table_.
  NodeId destination_node_id(message.destination_id());
  if (routing_table_.IsThisNodeClosestTo(destination_node_id)) {
    if (routing_table_.IsConnected(destination_node_id) ||
      non_routing_table_.IsConnected(destination_node_id)) {
      return network_.SendToClosestNode(message);
    } else {  // Case when response comes back through different route for relay messages.
      if (IsRoutingMessage(message)) {
        if (message.has_relay_id() && (message.relay_id() == routing_table_.kKeys().identity))
          return HandleRoutingMessage(message);
      }
      LOG(kWarning) << "Dropping message. This node ["
                    << HexSubstr(routing_table_.kKeys().identity)
                    << "] is the closest but is not connected to destination node ["
                    << HexSubstr(message.destination_id()) << "].  Message type: "
                    << message.type() << ", Src ID: " << HexSubstr(message.source_id())
                    << ", Relay ID: " << HexSubstr(message.relay_id());
      return;
    }
  } else {
    return network_.SendToClosestNode(message);
  }
}

void MessageHandler::HandleGroupMessageAsClosestNode(protobuf::Message& message) {
  assert(message.direct() != static_cast<int32_t>(ConnectType::kSingle));
  bool have_node_with_group_id(routing_table_.IsConnected(NodeId(message.destination_id())));
  // This node is not closest to the destination node for non-direct message.
  if (!routing_table_.IsThisNodeClosestTo(NodeId(message.destination_id())) &&
      !have_node_with_group_id) {
    LOG(kInfo) << "This node is not closest, passing it on.";
    return network_.SendToClosestNode(message);
  }

  // This node is closest so will send to all replicant nodes
  uint16_t replication(static_cast<uint16_t>(message.replication()));
  if ((replication < 1) || (replication > Parameters::node_group_size)) {
    LOG(kError) << "Dropping invalid non-direct message.";
    return;
  }

  --replication;  // This node will be one of the group member.
  message.set_direct(static_cast<int32_t>(ConnectType::kSingle));
  if (have_node_with_group_id)
    ++replication;
  auto close(routing_table_.GetClosestNodes(NodeId(message.destination_id()), replication));

  if (have_node_with_group_id)
    close.erase(close.begin());
  std::string group_id(message.destination_id());
  for (auto i : close) {
    LOG(kInfo) << "Replicating message to : " << HexSubstr(i.String())
               << " [ group_id : " << HexSubstr(group_id)  << "]";
    message.set_destination_id(i.String());
    NodeInfo node;
    if (routing_table_.GetNodeInfo(i, node))
      network_.SendToDirectEndpoint(message, node.endpoint);
  }

  message.set_destination_id(routing_table_.kKeys().identity);

  if (IsRoutingMessage(message))
    HandleRoutingMessage(message);
  else
    HandleNodeLevelMessageForThisNode(message);
}

void MessageHandler::HandleMessageAsFarNode(protobuf::Message& message) {
  LOG(kVerbose) << "This node is not closest to this message destination ID [ "
                <<  HexSubstr(message.destination_id())
                <<" ]; sending on.";
  network_.SendToClosestNode(message);
}

void MessageHandler::HandleGroupMessage(protobuf::Message& message) {
  if (!routing_table_.IsThisNodeInRange(NodeId(message.destination_id()), 1))
    return;

  LOG(kVerbose) << "This node is in closest proximity to this group message";
  if (IsRoutingMessage(message))
    HandleRoutingMessage(message);
  else
    HandleNodeLevelMessageForThisNode(message);
}

void MessageHandler::HandleMessage(protobuf::Message& message) {
  if (!message.IsInitialized()) {
    LOG(kWarning) << "Uninitialised message dropped.";
    return;
  }

  if (!ValidateMessage(message)) {
    LOG(kWarning) << "Validate message failed.";
    return;
  }

  // Invalid destination id, unknown message
  if (!(NodeId(message.destination_id()).IsValid())) {
    LOG(kWarning) << "Stray message dropped, need destination ID for processing.";
    return;
  }

  // If this node is a client
  if (routing_table_.client_mode())
    return HandleClientMessage(message);

  // Relay mode message
  if (message.source_id().empty())
    return HandleRelayRequest(message);

  // Invalid source id, unknown message
  if (!(NodeId(message.source_id()).IsValid())) {
    LOG(kWarning) << "Stray message dropped, need valid source ID for processing.";
    return;
  }

  // Direct message
  if (message.destination_id() == routing_table_.kKeys().identity)
    return HandleDirectMessage(message);

  // This node is in closest proximity to this message
  if (routing_table_.IsThisNodeInRange(NodeId(message.destination_id()),
                                       Parameters::closest_nodes_size)) {
    return HandleMessageAsClosestNode(message);
  } else {
    return HandleMessageAsFarNode(message);
  }
}

void MessageHandler::HandleRelayRequest(protobuf::Message& message) {
  assert(!message.has_source_id());
  if ((message.destination_id() == routing_table_.kKeys().identity) && IsRequest(message)) {
    LOG(kVerbose) << "Relay request with this node's ID as destination ID";
    return HandleDirectMessage(message);
  }

  // If small network yet, this node may be closest.
  if ((routing_table_.Size() <= Parameters::closest_nodes_size) &&
      (message.type() == static_cast<int32_t>(MessageType::kFindNodesRequest))) {
    service::FindNodes(routing_table_, message);
    return network_.SendToClosestNode(message);
  }

  // This node is now the src ID for the relay message and will send back response to original node.
  message.set_source_id(routing_table_.kKeys().identity);
  network_.SendToClosestNode(message);
}

bool MessageHandler::RelayDirectMessageIfNeeded(protobuf::Message& message) {
  assert(message.destination_id() == routing_table_.kKeys().identity);
  LOG(kVerbose) << "Relaying Direct Message.";

  if (!message.has_relay_id()) {
    LOG(kVerbose) << "Message don't have relay ID.";
    return false;
  }

  // Only direct responses need to be relayed
  if ((message.destination_id() != message.relay_id()) && IsResponse(message)) {
    message.clear_destination_id();  // to allow network util to identify it as relay message
    LOG(kVerbose) << "Relaying response to " << HexSubstr(message.relay_id());
    network_.SendToClosestNode(message);
    return true;
  } else {  // not a relay message response, its for this node
    LOG(kVerbose) << "Not a relay message response, it's for this node";
    return false;
  }
}

void MessageHandler::HandleClientMessage(protobuf::Message& message) {
  assert(routing_table_.client_mode() && "Only client node should handle client messages");
  if (IsRequest(message) || message.source_id().empty()) {  // No requests/relays allowed on client.
    LOG(kWarning) << "Stray message at client node. No requests/relays allowed.";
    return;
  }

  if (IsRoutingMessage(message)) {
    LOG(kInfo) << "Client Routing Response for " << HexSubstr(routing_table_.kKeys().identity)
               << " from " << HexSubstr(message.source_id());
    HandleRoutingMessage(message);
  } else if ((message.destination_id() == routing_table_.kKeys().identity)) {
    LOG(kInfo) << "Client Node Level Response for " << HexSubstr(routing_table_.kKeys().identity)
               << " from " << HexSubstr(message.source_id());
    timer_.ExecuteTask(message);
  }
}

// // TODO(dirvine) implement client handler
// bool MessageHandler::CheckAndSendToLocalClients(protobuf::Message& message) {
//   bool found(false);
// //   NodeId destination_node(message.destination_id());
// //   std::for_each(client_connections_.begin(),
// //                 client_connections_.end(),
// //                 [&destination_node, &found](const NodeInfo& i)->bool
// //                 {
// //                   if (i.node_id ==  destination_node) {
// //                     found = true;
// //                     // rudp send TODO(dirvine)
// //                   }
// //                   return found;  // lambda return
// //                 });
//   return found;
// }

void MessageHandler::set_message_received_functor(MessageReceivedFunctor message_received_functor) {
  message_received_functor_ = message_received_functor;
}

void MessageHandler::set_request_public_key_functor(
    RequestPublicKeyFunctor request_public_key_functor) {
  response_handler_->set_request_public_key_functor(request_public_key_functor);
}

}  // namespace routing

}  // namespace maidsafe
