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
#include "maidsafe/common/node_id.h"

#include "maidsafe/routing/network_utils.h"
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

void MessageHandler::HandleRoutingMessage(protobuf::Message& message) {
  bool request(message.request());
  switch (static_cast<MessageType>(message.type())) {
    case MessageType::kPing :
      message.request() ? service::Ping(routing_table_, message) : response_handler_->Ping(message);
      break;
    case MessageType::kConnect :
      message.request() ? service::Connect(routing_table_,
                                           non_routing_table_,
                                           network_,
                                           message,
                                           response_handler_->request_public_key_functor()) :
                                           response_handler_->Connect(message);
      break;
    case MessageType::kFindNodes :
      message.request() ? service::FindNodes(routing_table_, message) :
                          response_handler_->FindNodes(message);
      break;
    case MessageType::kConnectSuccess :
      response_handler_->ConnectSuccess(message);
      break;
    default:  // unknown (silent drop)
      return;
  }

  if (!request || !message.IsInitialized())
    return;

  if (routing_table_.Size() == 0)  // This node can only send to bootstrap_endpoint
    network_.SendToDirect(message,
                          network_.bootstrap_connection_id(),
                          network_.bootstrap_connection_id());
  else
    network_.SendToClosestNode(message);
}

void MessageHandler::HandleNodeLevelMessageForThisNode(protobuf::Message& message) {
  if (IsRequest(message)) {
    LOG(kInfo) << "Node Level Request for " << HexSubstr(routing_table_.kKeys().identity)
               << " from " << HexSubstr(message.source_id()) << " id: " << message.id();

    ReplyFunctor response_functor = [=](const std::string& reply_message) {
        if (reply_message.empty())
          return;
        protobuf::Message message_out;
        message_out.set_request(false);
        message_out.set_hops_to_live(Parameters::hops_to_live);
        message_out.set_destination_id(message.source_id());
        message_out.set_type(message.type());
        message_out.set_direct(true);
        message_out.clear_data();
        message_out.set_client_node(message.client_node());
        message_out.set_routing_message(message.routing_message());
        message_out.add_data(reply_message);
        message_out.set_last_id(routing_table_.kKeys().identity);
        message_out.set_source_id(routing_table_.kKeys().identity);
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
            routing_table_.kKeys().identity == message_out.destination_id()) {
          network_.SendToClosestNode(message_out);
          return;
        }
        if (routing_table_.kKeys().identity != message_out.destination_id()) {
          network_.SendToClosestNode(message_out);
        } else {
          LOG(kInfo) << "Sending response to self." << " id: " << message.id();
          HandleMessage(message_out);
        }
    };
    NodeId group_claim(message.has_group_claim() ? NodeId(message.group_claim()) : NodeId());
    if (message_received_functor_)
      message_received_functor_(message.data(0), group_claim, response_functor);
  } else {  // response
    LOG(kInfo) << "Node Level Response for " << HexSubstr(routing_table_.kKeys().identity)
               << " from " << HexSubstr(message.source_id()) << " id: " << message.id();
    timer_.AddResponse(message);
  }
}

void MessageHandler::HandleMessageForThisNode(protobuf::Message& message) {
  if (RelayDirectMessageIfNeeded(message))
    return;

  LOG(kVerbose) << "Message for this node." << " id: " << message.id();
  if (IsRoutingMessage(message))
    HandleRoutingMessage(message);
  else
    HandleNodeLevelMessageForThisNode(message);
}

void MessageHandler::HandleMessageAsClosestNode(protobuf::Message& message) {
  LOG(kVerbose) << "This node is in closest proximity to this message destination ID [ "
                <<  HexSubstr(message.destination_id())
                << " ]." << " id: " << message.id();
  if (IsDirect(message)) {
    return HandleDirectMessageAsClosestNode(message);
  } else {
    return HandleGroupMessageAsClosestNode(message);
  }
}

void MessageHandler::HandleDirectMessageAsClosestNode(protobuf::Message& message) {
  assert(message.direct());
  // Dropping direct messages if this node is closest and destination node is not in routing_table_
  // or non_routing_table_.
  NodeId destination_node_id(message.destination_id());
  if (routing_table_.IsThisNodeClosestTo(destination_node_id)) {
    if (routing_table_.IsConnected(destination_node_id) ||
      non_routing_table_.IsConnected(destination_node_id)) {
      return network_.SendToClosestNode(message);
    } else {
      LOG(kWarning) << "Dropping message. This node ["
                    << HexSubstr(routing_table_.kKeys().identity)
                    << "] is the closest but is not connected to destination node ["
                    << HexSubstr(message.destination_id()) << "], Src ID: "
                    << HexSubstr(message.source_id())
                    << ", Relay ID: " << HexSubstr(message.relay_id()) << " id: " << message.id()
                    << PrintMessage(message);
      return;
    }
  } else {
    return network_.SendToClosestNode(message);
  }
}

void MessageHandler::HandleGroupMessageAsClosestNode(protobuf::Message& message) {
  assert(!message.direct());
  bool have_node_with_group_id(routing_table_.IsConnected(NodeId(message.destination_id())));
  // This node is not closest to the destination node for non-direct message.
  if (!routing_table_.IsThisNodeClosestTo(NodeId(message.destination_id()), !IsDirect(message)) &&
      !have_node_with_group_id) {
    LOG(kInfo) << "This node is not closest, passing it on." << " id: " << message.id();
    return network_.SendToClosestNode(message);
  }

  // This node is closest so will send to all replicant nodes
  uint16_t replication(static_cast<uint16_t>(message.replication()));
  if ((replication < 1) || (replication > Parameters::node_group_size)) {
    LOG(kError) << "Dropping invalid non-direct message." << " id: " << message.id();
    return;
  }

  --replication;  // This node will be one of the group member.
  message.set_direct(true);
  if (have_node_with_group_id)
    ++replication;
  auto close(routing_table_.GetClosestNodes(NodeId(message.destination_id()), replication));

  if (have_node_with_group_id)
    close.erase(close.begin());
  std::string group_id(message.destination_id());
  std::string group_members("[" + DebugId(routing_table_.kNodeId()) + "]");

  for (auto i : close)
    group_members+=std::string("[" + DebugId(i) +"]");
  LOG(kInfo) << "Group members for group_id " << HexSubstr(group_id) << " are: "
             << group_members;

  for (auto i : close) {
    LOG(kInfo) << "Replicating message to : " << HexSubstr(i.String())
               << " [ group_id : " << HexSubstr(group_id)  << "]" << " id: " << message.id();
    message.set_destination_id(i.String());
    NodeInfo node;
    if (routing_table_.GetNodeInfo(i, node)) {
      network_.SendToDirect(message, node.node_id, node.connection_id);
    }
  }

  message.set_destination_id(routing_table_.kKeys().identity);

  if (IsRoutingMessage(message))
    HandleRoutingMessage(message);
  else
    HandleNodeLevelMessageForThisNode(message);
}

void MessageHandler::HandleMessageAsFarNode(protobuf::Message& message) {
  LOG(kVerbose) << "This node is not in closest proximity to this message destination ID [ "
                <<  HexSubstr(message.destination_id())
                <<" ]; sending on." << " id: " << message.id();
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
  if (!ValidateMessage(message)) {
    LOG(kWarning) << "Validate message failed." << " id: " << message.id();
    assert((message.hops_to_live() > 0) &&
           "Message has traversed maximum number of hops allowed");
    return;
  }

  DecreamentHopsToLive(message);

  // If group message request to self id
  if (IsGroupMessageRequestToSelfId(message))
    return HandleGroupMessageToSelfId(message);

  // If this node is a client
  if (routing_table_.client_mode())
    return HandleClientMessage(message);

  // Relay mode message
  if (message.source_id().empty())
    return HandleRelayRequest(message);

  // Invalid source id, unknown message
  if (!NodeId(message.source_id()).IsValid() || NodeId(message.source_id()).Empty()) {
    LOG(kWarning) << "Stray message dropped, need valid source ID for processing."
                  << " id: " << message.id();
    return;
  }

  // Direct message
  if (message.destination_id() == routing_table_.kKeys().identity)
    return HandleMessageForThisNode(message);

  if (IsRelayResponseForThisNode(message))
    return HandleRoutingMessage(message);

  if (non_routing_table_.IsConnected(NodeId(message.destination_id())) && IsDirect(message)) {
    return HandleMessageForNonRoutingNodes(message);
  }

  // This node is in closest proximity to this message
  if (routing_table_.IsThisNodeInRange(NodeId(message.destination_id()),
                                       Parameters::closest_nodes_size) ||
      routing_table_.IsThisNodeClosestTo(NodeId(message.destination_id()), !message.direct())) {
    return HandleMessageAsClosestNode(message);
  } else {
    return HandleMessageAsFarNode(message);
  }
}

void MessageHandler::HandleMessageForNonRoutingNodes(protobuf::Message& message) {
  auto non_routing_nodes(non_routing_table_.GetNodesInfo(NodeId(message.destination_id())));
  assert(!non_routing_nodes.empty() && message.direct());
  if (IsRequest(message) &&
      (!message.client_node() ||
       (message.source_id() != message.destination_id()))) {
    LOG(kWarning) << "This node ["
                  << HexSubstr(routing_table_.kKeys().identity)
                  << " Dropping message as non-client to client message not allowed."
                  << PrintMessage(message);
    return;
  }
  LOG(kInfo) << "This node has message destination in its non routing table. Dest id : "
             << HexSubstr(message.destination_id()) << " message id: " << message.id();
  return network_.SendToClosestNode(message);
}

void MessageHandler::HandleRelayRequest(protobuf::Message& message) {
  assert(!message.has_source_id());
  if ((message.destination_id() == routing_table_.kKeys().identity) && IsRequest(message)) {
    LOG(kVerbose) << "Relay request with this node's ID as destination ID"
                  << " id: " << message.id();
    // If group message request to this node's id sent by relay requester node
    if ((message.destination_id() == routing_table_.kKeys().identity) &&
        message.request() && !message.direct()) {
      message.set_source_id(routing_table_.kKeys().identity);
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
  message.set_source_id(routing_table_.kKeys().identity);
  network_.SendToClosestNode(message);
}

void MessageHandler::HandleDirectRelayRequestMessageAsClosestNode(protobuf::Message& message) {
  assert(message.direct());
  // Dropping direct messages if this node is closest and destination node is not in routing_table_
  // or non_routing_table_.
  NodeId destination_node_id(message.destination_id());
  if (routing_table_.IsThisNodeClosestTo(destination_node_id)) {
    if (routing_table_.IsConnected(destination_node_id) ||
      non_routing_table_.IsConnected(destination_node_id)) {
      message.set_source_id(routing_table_.kKeys().identity);
      return network_.SendToClosestNode(message);
    } else {
      LOG(kWarning) << "Dropping message. This node ["
                    << HexSubstr(routing_table_.kKeys().identity)
                    << "] is the closest but is not connected to destination node ["
                    << HexSubstr(message.destination_id()) << "], Src ID: "
                    << HexSubstr(message.source_id())
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
  bool have_node_with_group_id(routing_table_.IsConnected(NodeId(message.destination_id())));
  // This node is not closest to the destination node for non-direct message.
  if (!routing_table_.IsThisNodeClosestTo(NodeId(message.destination_id()), !IsDirect(message)) &&
      !have_node_with_group_id) {
    LOG(kInfo) << "This node is not closest, passing it on." << " id: " << message.id();
    message.set_source_id(routing_table_.kKeys().identity);
    return network_.SendToClosestNode(message);
  }

  // This node is closest so will send to all replicant nodes
  uint16_t replication(static_cast<uint16_t>(message.replication()));
  if ((replication < 1) || (replication > Parameters::node_group_size)) {
    LOG(kError) << "Dropping invalid non-direct message." << " id: " << message.id();
    return;
  }

  --replication;  // This node will be one of the group member.
  message.set_direct(true);
  if (have_node_with_group_id)
    ++replication;
  auto close(routing_table_.GetClosestNodes(NodeId(message.destination_id()), replication));

  if (have_node_with_group_id)
    close.erase(close.begin());
  std::string group_id(message.destination_id());
  std::string group_members("[" + DebugId(routing_table_.kNodeId()) + "]");

  for (auto i : close)
    group_members+=std::string("[" + DebugId(i) +"]");
  LOG(kInfo) << "Group members for group_id " << HexSubstr(group_id) << " are: "
             << group_members;
  message.set_source_id(routing_table_.kKeys().identity);  // This node relays back the responses
  for (auto i : close) {
    LOG(kInfo) << "Replicating message to : " << HexSubstr(i.String())
               << " [ group_id : " << HexSubstr(group_id)  << "]" << " id: " << message.id();
    message.set_destination_id(i.String());
    NodeInfo node;
    if (routing_table_.GetNodeInfo(i, node)) {
      network_.SendToDirect(message, node.node_id, node.connection_id);
    }
  }

  message.set_destination_id(routing_table_.kKeys().identity);
  message.clear_source_id();
  if (IsRoutingMessage(message))
    HandleRoutingMessage(message);
  else
    HandleNodeLevelMessageForThisNode(message);
}

// Special case when response of a relay comes through an alternative route.
bool MessageHandler::IsRelayResponseForThisNode(protobuf::Message& message) {
  if (IsRoutingMessage(message) && message.has_relay_id() &&
      (message.relay_id() == routing_table_.kKeys().identity)) {
    LOG(kVerbose) << "Relay response through alternative route";
    return true;
  } else {
    return false;
  }
}

bool MessageHandler::RelayDirectMessageIfNeeded(protobuf::Message& message) {
  assert(message.destination_id() == routing_table_.kKeys().identity);
  if (!message.has_relay_id()) {
//    LOG(kVerbose) << "Message don't have relay ID.";
    return false;
  }

  // Only direct responses need to be relayed
  if ((message.destination_id() != message.relay_id()) && IsResponse(message)) {
    message.clear_destination_id();  // to allow network util to identify it as relay message
    LOG(kVerbose) << "Relaying response to " << HexSubstr(message.relay_id())
                  << " id: " << message.id();
    network_.SendToClosestNode(message);
    return true;
  } else {  // not a relay message response, its for this node
//    LOG(kVerbose) << "Not a relay message response, it's for this node";
    return false;
  }
}

void MessageHandler::HandleClientMessage(protobuf::Message& message) {
  assert(routing_table_.client_mode() && "Only client node should handle client messages");
  if (message.source_id().empty()) {  // No relays allowed on client.
    LOG(kWarning) << "Stray message at client node. No relays allowed."
                  << " id: " << message.id();
    return;
  }

  if (IsRoutingMessage(message)) {
    LOG(kInfo) << "Client Routing Response for " << HexSubstr(routing_table_.kKeys().identity)
               << " from " << HexSubstr(message.source_id()) << " id: " << message.id();
    HandleRoutingMessage(message);
  } else if ((message.destination_id() == routing_table_.kKeys().identity)) {
    HandleNodeLevelMessageForThisNode(message);
//     LOG(kInfo) << "Client Node Level Response for " << HexSubstr(routing_table_.kKeys().identity)
//                << " from " << HexSubstr(message.source_id()) << " id: " << message.id();
//     timer_.AddResponse(message);
  }
}

// Special case : If group message request to self id
bool MessageHandler::IsGroupMessageRequestToSelfId(protobuf::Message& message) {
  return ((message.source_id() == routing_table_.kKeys().identity) &&
          (message.destination_id() == routing_table_.kKeys().identity) &&
          message.request() &&
          !message.direct());
}

void MessageHandler::HandleGroupMessageToSelfId(protobuf::Message& message) {
  assert(message.source_id() == routing_table_.kKeys().identity);
  assert(message.destination_id() == routing_table_.kKeys().identity);
  assert(message.request());
  assert(!message.direct());
  LOG(kInfo) << "Sending group message to self id. Passing on to the closest peer to replicate";
  network_.SendToClosestNode(message);
}

void MessageHandler::set_message_received_functor(MessageReceivedFunctor message_received_functor) {
  message_received_functor_ = message_received_functor;
}

void MessageHandler::set_request_public_key_functor(
    RequestPublicKeyFunctor request_public_key_functor) {
  response_handler_->set_request_public_key_functor(request_public_key_functor);
}

void MessageHandler::DecreamentHopsToLive(protobuf::Message& message) {
  message.set_hops_to_live(message.hops_to_live() - 1);
}

}  // namespace routing

}  // namespace maidsafe
