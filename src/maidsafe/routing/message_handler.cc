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

#include "boost/thread/shared_mutex.hpp"
#include "boost/thread/mutex.hpp"
#include "maidsafe/common/rsa.h"
#include "maidsafe/transport/managed_connections.h"
#include "maidsafe/routing/cache_manager.h"
#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/response_handler.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/service.h"
#include "maidsafe/routing/utils.h"
#include "maidsafe/routing/log.h"


namespace maidsafe {

namespace routing {

class Timer;

MessageHandler::MessageHandler(const NodeValidationFunctor &node_validation_functor,
                RoutingTable &routing_table,
                rudp::ManagedConnections &transport,
                Timer &timer_ptr ) :
                node_validation_functor_(node_validation_functor),
                routing_table_(routing_table),
                transport_(transport),
                timer_ptr_(timer_ptr),
                cache_manager_(routing_table, transport),
                service_(node_validation_functor, routing_table, transport),
                response_handler_(node_validation_functor,
                                  routing_table,
                                  transport),
                message_received_signal_()  {}

boost::signals2::signal<void(int, std::string)>
                                     &MessageHandler::MessageReceivedSignal() {
  return message_received_signal_;
}

bool MessageHandler::CheckCacheData(protobuf::Message &message) {
  if (message.type() == 100) {
    if (message.response()) {
      cache_manager_.AddToCache(message);
     } else  {  // request
       if (cache_manager_.GetFromCache(message))
        return true;// this operation sends back the message
     }
  }
  return false;  // means this message is not finished processing
}

void MessageHandler::ProcessMessage(protobuf::Message &message) {
  if (message.source_id() == "ANONYMOUS" ) {  // relay mode
    message.set_source_id(routing_table_.kKeys().identity);
  }
  if ((message.destination_id() == routing_table_.kKeys().identity) &&
      message.has_relay()) {
     transport::Endpoint send_to_endpoint(message.relay().ip(),
                    message.relay().port());
     //TODO(dirvine) FIXME
//      transport_.Send(send_to_endpoint, 
//                      send_to_endpoint,
//                      message.SerializeAsString());
  }
  if (CheckCacheData(message))
    return;  // message was sent on it's way

  // Handle direct to me messages
  if(message.direct()) {
    if (message.destination_id() == routing_table_.kKeys().identity) {
      if(message.response()) {
        timer_ptr_.ExecuteTaskNow(message);
        return;
      } else { // I am closest and it's a request
        try {
          message_received_signal_(static_cast<int>(message.type()),
                                  message.data());
        } catch(const std::exception &e) {
          DLOG(ERROR) << e.what();
        }
      }
      // signal up
    } else if (CheckAndSendToLocalClients(message)) {
      // send to all clients with that node_id;
      return;
    } else {
      SendOn(message, transport_, routing_table_);
      return;
    }
  }

  // is it for us ??
  if (!routing_table_.AmIClosestNode(NodeId(message.destination_id()))) {
    SendOn(message, transport_, routing_table_);
    return;
  }

  // I am closest node
  if (message.type() == 0) {  // ping
    if (message.response()) {
      response_handler_.ProcessPingResponse(message);
      return;
    } else {
      service_.Ping(message);
      return;
    }
  }
   if (message.type() == 1) {  // bootstrap
    if (message.response()) {
        response_handler_.ProcessConnectResponse(message);
      return;
    } else {
        service_.Connect(message);
        return;
    }
  }
  if (message.type() == 2) {  // find_nodes
    if (message.response()) {
      response_handler_.ProcessFindNodeResponse(message);
      return;
    } else {
        service_.FindNodes(message);
        return;
    }
  }
  // if this is set not direct and ID == ME do NOT respond.
  if (message.destination_id() != routing_table_.kKeys().identity) {
    try {
//       message_received_signal_(static_cast<int>(message.type()),
//                                 message.data());
    }
    catch(const std::exception &e) {
      DLOG(ERROR) << e.what();
    }
  } else {  // I am closest and it's direct

  }
  // I am closest so will send to all my replicant nodes
  message.set_direct(true);
  message.set_source_id(routing_table_.kKeys().identity);
  auto close =
        routing_table_.GetClosestNodes(NodeId(message.destination_id()),
                                static_cast<uint16_t>(message.replication()));
  for (auto it = close.begin(); it != close.end(); ++it) {
    message.set_destination_id((*it).String());
    SendOn(message, transport_, routing_table_);
  }
}

// TODO(dirvine) implement client handler
bool MessageHandler::CheckAndSendToLocalClients(protobuf::Message &message) {
  bool found(false);
//   NodeId destination_node(message.destination_id());
//   std::for_each(client_connections_.begin(),
//                 client_connections_.end(),
//                 [&destination_node, &found](const NodeInfo &i)->bool
//                 {
//                   if (i.node_id ==  destination_node) {
//                     found = true;
//                     // transport send TODO(dirvine)
//                   }
//                   return found;  // lambda return
//                 });
  return found;
}



}  // namespace routing

}  // namespace maidsafe