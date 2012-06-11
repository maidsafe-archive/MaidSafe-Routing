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
#include "maidsafe/routing/message.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/response_handler.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/service.h"
#include "maidsafe/routing/timer.h"
#include "maidsafe/routing/utils.h"

namespace bs2 = boost::signals2;

namespace maidsafe {

namespace routing {

class Timer;

MessageHandler::MessageHandler(
                RoutingTable &routing_table,
                NonRoutingTable &non_routing_table,
                rudp::ManagedConnections &rudp,
                Timer &timer_ptr,
                NodeValidationFunctor node_validation_functor) :
                routing_table_(routing_table),
                non_routing_table_(non_routing_table),
                rudp_(rudp),
                timer_ptr_(timer_ptr),
                cache_manager_(),
                message_received_signal_(),
                node_validation_functor_(node_validation_functor) {}

boost::signals2::signal<void(int, std::string)>
                                     &MessageHandler::MessageReceivedSignal() {
  return message_received_signal_;
}

void MessageHandler::Send(Message& message) {
  assert(!message.Valid() && "invalid message");
  SendOn(message, rudp_, routing_table_);
}

bool MessageHandler::CheckCacheData(Message &message) {
//  if (message.Type() == -100) {
//    cache_manager_.AddToCache(message.Data());
//  } else  if (message.Type() == 100) {
//    if (cache_manager_.GetFromCache(message.Data())) {
//      message.SetDestination(message.SourceId().String());
//      message.SetMeAsSource();
//      assert(!message.Valid() && "invalid message");
//      SendOn(message, rudp_, routing_table_);
//      return true;
//    }
//  } else {
//    return false;  // means this message is not finished processing
//  }
//  return false;
}

void MessageHandler::RoutingMessage(Message& message) {
  switch (message.Type()) {
    case -1 :  // ping
      response::Ping(message);
      break;
    case 1 :
      service::Ping(routing_table_, message);
      break;
    case -2 :  // connect
      response::Connect(message, node_validation_functor_);
      break;
    case 2 :
      service::Connect(routing_table_, rudp_, message);
      break;
    case -3 :  // find_nodes
      response::FindNode(routing_table_, rudp_, message);
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
  SendOn(message, rudp_, routing_table_);
}

void MessageHandler::DirectMessage(Message& message) {
  if (message.HasRelay()) {
      rudp_.Send(message.RelayEndpoint(), message.Serialise());
     return;
  }
  if ((message.Type() < 100) && (message.Type() > -100)) {
    RoutingMessage(message);
    return;
  }
  if (message.Type() > 100) {  // request
    message_received_signal_(static_cast<int>(-message.Type()),
                             message.Data());
    LOG(kInfo) << "Routing message detected";
  } else {  // response
    timer_ptr_.ExecuteTaskNow(message);
    LOG(kInfo) << "Response detected";
  }
}

void MessageHandler::CloseNodesMessage(Message& message) {
  if (message.HasRelay()) {
    rudp_.Send(message.RelayEndpoint() , message.Serialise());
    return;
  }
  if ((message.Direct() == ConnectType::kSingle) && (message.ClosestToMe())) {
    SendOn(message, rudp_, routing_table_);
    return;
  }
  // I am closest so will send to all my replicant nodes
  message.SetDirect(ConnectType::kSingle);
  auto close =
        routing_table_.GetClosestNodes(message.DestinationId(),
                                       Parameters::managed_group_size);
  for (auto i : close) {
    message.SetDestination(i.String());
    SendOn(message, rudp_, routing_table_);
  }
}

void MessageHandler::ProcessMessage(const std::string &message) {
  Message msg(message, routing_table_, non_routing_table_);
  if (msg.ForMe()) {

  }
//  // client connected messages -> out
//  if (message.source_id().empty()) {  // relay mode
//    // if zero state we may be closest
//    if ((routing_table_.Size() <= Parameters::closest_nodes_size) &&
//       (message.type() == 3)) {
//        service::FindNodes(routing_table_, message);
//        SendOn(message, rudp_, routing_table_);
//        return;
//    }
//    message.set_source_id(routing_table_.kKeys().identity);
//    SendOn(message, rudp_, routing_table_);
//  }
//  // message for me !
//  if (message.destination_id() == routing_table_.kKeys().identity) {
//    DirectMessage(message);
//    return;
//  }
//  // cache response to get data that's cacheable
//  if ((message.type() == -100) && (CheckCacheData(message)))
//    return;
//  // I am in closest proximity to this message
//  if (routing_table_.IsMyNodeInRange(NodeId(message.destination_id()),
//                                     Parameters::closest_nodes_size)) {
//    if ((message.type() < 100) && (message.type() > -100)) {
//      RoutingMessage(message);
//      return;
//    } else {
//      CloseNodesMessage(message);
//      return;
//    }
//  }
  // default
//  SendOn(message, rudp_, routing_table_);
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

}  // namespace routing

}  // namespace maidsafe
