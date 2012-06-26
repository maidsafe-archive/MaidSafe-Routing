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

#include "maidsafe/common/rsa.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/log.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/response_handler.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/service.h"
#include "maidsafe/routing/timer.h"
#include "maidsafe/routing/utils.h"

namespace bs2 = boost::signals2;

namespace maidsafe {

namespace routing {

class Timer;

MessageHandler::MessageHandler(std::shared_ptr<AsioService> asio_service,
                               RoutingTable &routing_table,
                               rudp::ManagedConnections &rudp,
                               Timer &timer_ptr,
                               MessageReceivedFunctor message_received_functor,
                               RequestPublicKeyFunctor node_validation_functor)
    : asio_service_(asio_service),
      routing_table_(routing_table),
      rudp_(rudp),
      bootstrap_endpoint_(),
      my_relay_endpoint_(),
      timer_ptr_(timer_ptr),
      cache_manager_(),
      message_received_functor_(message_received_functor),
      node_validation_functor_(node_validation_functor) {}

void MessageHandler::Send(protobuf::Message& message) {
  SendOn(message, rudp_, routing_table_);
}

bool MessageHandler::CheckCacheData(protobuf::Message &message) {
  if (message.type() == -100) {
    cache_manager_.AddToCache(message);
  } else  if (message.type() == 100) {
    if (cache_manager_.GetFromCache(message)) {
      message.set_source_id(routing_table_.kKeys().identity);
      SendOn(message, rudp_, routing_table_);
      return true;
    }
  } else {
    return false;  // means this message is not finished processing
  }
  return false;
}

void MessageHandler::RoutingMessage(protobuf::Message& message) {
  LOG(kInfo) << "MessageHandler::RoutingMessage";
  bool is_response(message.type() < 0);
  switch (message.type()) {
    case -1 :  // ping
      response::Ping(message);
      break;
    case 1 :
      service::Ping(routing_table_, message);
      break;
    case -2 :  // connect
      response::Connect(routing_table_, rudp_, message, node_validation_functor_);
      break;
    case 2 :
      service::Connect(routing_table_, rudp_, message, node_validation_functor_);
      break;
    case -3 :  // find_nodes
      response::FindNode(routing_table_, rudp_, message, bootstrap_endpoint_);
      break;
    case 3 :
      service::FindNodes(routing_table_, message);
      break;
    case -4 :  // proxy_connect
      response::ProxyConnect(message);
      break;
    case 4 :
      service::ProxyConnect(routing_table_, rudp_, message);
      break;
    default:  // unknown (silent drop)
      return;
  }
  if (is_response)
    return;

  Endpoint direct_endpoint;
  if (routing_table_.Size() == 0) {  // I can only send to bootstrap_endpoint
    direct_endpoint = bootstrap_endpoint_;
  }
  SendOn(message, rudp_, routing_table_, direct_endpoint);
}

void MessageHandler::MessageForMe(protobuf::Message &message) {
   if (message.type() > 100) {  // request
    LOG(kVerbose) <<"Message for me !!";
    ReplyFunctor response_functor = [=](const std::string& reply_message)  {
      if (reply_message.empty())
        return;
    protobuf::Message message_out;
    message_out.set_type(-message.type());
    message_out.set_destination_id(message.source_id());
    message_out.set_direct(static_cast<int32_t>(ConnectType::kSingle));
    message_out.set_data(reply_message);
    message_out.set_last_id(routing_table_.kKeys().identity);
    message_out.set_source_id(routing_table_.kKeys().identity);
    SendOn(message_out, rudp_, routing_table_);
    };
    if (message.type() < 0)
      message.set_type(-message.type());
    message_received_functor_(static_cast<int>(message.type()), message.data(), NodeId(),
                              response_functor);
    LOG(kInfo) << "Routing message detected";
  } else {  // response
    timer_ptr_.ExecuteTaskNow(message);
    LOG(kInfo) << "Response detected";
  }
}

void MessageHandler::DirectMessage(protobuf::Message& message) {
  if (RelayDirectMessageIfNeeded(message)) {
    return;
  }
  LOG(kVerbose) <<"Direct Message for me!!!";
  if ((message.type() < 100) && (message.type() > -100)) {
    LOG(kVerbose) <<"Direct RoutingMessage type";
    RoutingMessage(message);
  } else {
    MessageForMe(message);
  }
}

void MessageHandler::CloseNodesMessage(protobuf::Message& message) {
  if (message.has_relay()) {
    Endpoint send_to_endpoint;
    send_to_endpoint.address(boost::asio::ip::address::from_string(message.relay().ip()));
    send_to_endpoint.port(static_cast<unsigned short>(message.relay().port()));
    rudp::MessageSentFunctor message_sent_functor;  // TODO (FIXME)
    rudp_.Send(send_to_endpoint, message.SerializeAsString(), message_sent_functor);
    return;
  }
  if ((message.direct()) && (!routing_table_.AmIClosestNode(NodeId(message.destination_id())))) {
    SendOn(message, rudp_, routing_table_);
    return;
  }
  // I am closest so will send to all my replicant nodes
  message.set_direct(true);
  auto close =
        routing_table_.GetClosestNodes(NodeId(message.destination_id()),
                                       static_cast<uint16_t>(message.replication()));
  for (auto i : close) {
    message.set_destination_id(i.String());
    SendOn(message, rudp_, routing_table_);
  }
  if ((message.type() < 100) && (message.type() > -100)) {
    LOG(kVerbose) <<"I am closest node RoutingMessage";
    RoutingMessage(message);
  } else {
    LOG(kVerbose) <<"I am closest node Message";
    MessageForMe(message);
  }
}

void MessageHandler::GroupMessage(protobuf::Message &message) {
if (!routing_table_.IsMyNodeInRange(NodeId(message.destination_id()), 1))
  return;

  LOG(kVerbose) <<"I am in closest proximity to this group message";
  if ((message.type() < 100) && (message.type() > -100)) {
    LOG(kVerbose) <<"I am closest node RoutingMessage";
    RoutingMessage(message);
  } else {
    LOG(kVerbose) <<"I am closest node Message";
    MessageForMe(message);
  }
}

void MessageHandler::ProcessMessage(protobuf::Message &message) {
  // client connected messages -> out
  if (message.source_id().empty()) {  // relay mode
    // if zero state we may be closest
    if (routing_table_.Size() <= Parameters::closest_nodes_size) {
      if (message.type() == 3) {
        service::FindNodes(routing_table_, message);
        SendOn(message, rudp_, routing_table_);
        return;
      }
    }
    message.set_source_id(routing_table_.kKeys().identity);
    SendOn(message, rudp_, routing_table_);
  }
  // message for me !
  if (message.destination_id() == routing_table_.kKeys().identity) {
    LOG(kVerbose) << "Direct message!";
    DirectMessage(message);
    return;
  }
  // cache response to get data that's cacheable
  if ((message.type() == -100) && (CheckCacheData(message)))
    return;
  // I am in closest proximity to this message
  if (routing_table_.IsMyNodeInRange(NodeId(message.destination_id()),
                                     Parameters::closest_nodes_size)) {
    LOG(kVerbose) <<"I am in closest proximity to this message";
    if ((message.type() < 100) && (message.type() > -100)) {
      RoutingMessage(message);
      return;
    } else {
      CloseNodesMessage(message);
      return;
    }
  } else {
    LOG(kVerbose) <<"I am not in closest proximity to this message";
  }
  // default
  SendOn(message, rudp_, routing_table_);
}

void MessageHandler::SwapWithMySourceIdIfNeeded(protobuf::Message &message) {
  if(message.has_source_id())
    return;
  if (message.has_relay()) {
    message.set_source_id(routing_table_.kKeys().identity);
  } else if(message.has_relay_id()) {
    message.set_source_id(routing_table_.kKeys().identity);
    // TODO(Prakash): Add this node to my non RT
  }
}

bool MessageHandler::RelayDirectMessageIfNeeded(protobuf::Message &message) {
  if (message.type() < 0) { //  Only direct responses need to be relayed
    Endpoint relay_endpoint;
    if ((message.has_relay_id()) && (message.relay_id() != routing_table_.kKeys().identity)) {
      LOG(kVerbose) <<"DirectMessage -- message has_relay_id and relay id is not me";
      bool i_am_connected_to_relay_id_node(false); //TODO(Prakash): FIXME if relay_id in RT and NRT
      if (i_am_connected_to_relay_id_node) {
        relay_endpoint = Endpoint(); //TODO(Prakash): Find relay id in my RT & non RT
        message.set_destination_id(message.relay_id());
        SendOn(message, rudp_, routing_table_, relay_endpoint);
        return true;
      } else {
        LOG(kVerbose) <<"DirectMessage - Droping message with relay id not in my RT & NRT.";
        return true;
      }
    } else if (message.has_relay()) {  // node with unpublished node_id
      relay_endpoint.address(boost::asio::ip::address::from_string(message.relay().ip()));
      relay_endpoint.port(static_cast<unsigned short>(message.relay().port()));
      LOG(kVerbose) << "has relay ip " << relay_endpoint << ", my relay ip " << my_relay_endpoint_;
      if (relay_endpoint != my_relay_endpoint_) {
        LOG(kVerbose) <<"DirectMessage -- replying to the relay_endpoint.";
        message.set_destination_id(NodeId().String());
        SendOn(message, rudp_, routing_table_, relay_endpoint);
        return true;
      }
    }
  }
  return false;
}

// // TODO(dirvine) implement client handler
// bool MessageHandler::CheckAndSendToLocalClients(protobuf::Message &message) {
//   bool found(false);
// //   NodeId destination_node(message.destination_id());
// //   std::for_each(client_connections_.begin(),
// //                 client_connections_.end(),
// //                 [&destination_node, &found](const NodeInfo &i)->bool
// //                 {
// //                   if (i.node_id ==  destination_node) {
// //                     found = true;
// //                     // rudp send TODO(dirvine)
// //                   }
// //                   return found;  // lambda return
// //                 });
//   return found;
// }

void MessageHandler::set_message_received_functor(MessageReceivedFunctor message_received) {
  message_received_functor_ = message_received;
}

void MessageHandler::set_node_validation_functor(RequestPublicKeyFunctor node_validation) {
  node_validation_functor_ = node_validation;
}

void MessageHandler::set_bootstrap_endpoint(Endpoint endpoint) {
  bootstrap_endpoint_ = endpoint;
}

void MessageHandler::set_my_relay_endpoint(Endpoint endpoint) {
  my_relay_endpoint_ = endpoint;
}

Endpoint MessageHandler::bootstrap_endpoint() {
return bootstrap_endpoint_;
}

}  // namespace routing

}  // namespace maidsafe
