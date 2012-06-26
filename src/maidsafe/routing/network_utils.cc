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

#include "maidsafe/common/utils.h"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/utils.h"


namespace maidsafe {

namespace routing {

namespace {

void SendOn(protobuf::Message message,
            rudp::ManagedConnections &rudp,
            RoutingTable &routing_table,
            const Endpoint &endpoint,
            const bool &recursive_retry) {
  std::string serialised_message(message.SerializeAsString());
  rudp::MessageSentFunctor message_sent_functor;
  if (!recursive_retry) {  //  send only once to direct endpoint
    message_sent_functor = [](bool message_sent) {
    message_sent ?  LOG(kInfo) << "Sent to a direct node" :
                    LOG(kError) << "could not send to a direct node";
    };
  } else {
      message_sent_functor = [=, &rudp, &routing_table](bool message_sent) {
      if (!message_sent) {
         LOG(kInfo) << " Send retry !! ";
         Endpoint endpoint = routing_table.GetClosestNode(NodeId(message.destination_id()),
                                                  1).endpoint;
          rudp.Send(endpoint, serialised_message, message_sent_functor);
      } else {
        LOG(kInfo) << " Send succeeded ";
      }
    };
  }
  LOG(kVerbose) << " >>>>>>>>>>>>>>> rudp send message to " << endpoint << " <<<<<<<<<<<<<<<<<<<<";
  rudp.Send(endpoint, serialised_message, message_sent_functor);
}

}  // anonymous namespace


void ProcessSend(protobuf::Message message,
                 rudp::ManagedConnections &rudp,
                 RoutingTable &routing_table,
                 Endpoint endpoint) {
  std::string signature;
  asymm::Sign(message.data(), routing_table.kKeys().private_key, &signature);
  message.set_signature(signature);

  // Direct endpoint message
  if (!endpoint.address().is_unspecified()) {  // direct endpoint provided
    SendOn(message, rudp, routing_table, endpoint, false);
    return;
  } 

  // Normal messages
  if (message.has_destination_id()) {  // message has destination id
    if (routing_table.Size() > 0) {
      endpoint = routing_table.GetClosestNode(NodeId(message.destination_id()), 0).endpoint;
      LOG(kVerbose) << " Endpoint to send for message with destination id : "
                    << HexSubstr(message.source_id()) << " , endpoint : " << endpoint;
      SendOn(message, rudp, routing_table, endpoint, true);
    } else {
      LOG(kError) << " No Endpoint to send to, Aborting Send!"
                  << " Attempt to send a type : " << message.type() << " message"
                  << " to " << HexSubstr(message.source_id())
                  << " From " << HexSubstr(routing_table.kKeys().identity);
    }
    return;
  }

  // Relay message responses only
  if (message.has_relay_id() && (IsResponse(message))) {  // relay type message
    if (false /*AmIConnectedToNonRoutingNode(message.relay_id())*/)  // if I have relay id in NRT
      endpoint; // = NRT.getendpoint(message.relay_id());
    else if (message.has_relay()){  //  relay endpoint is the last resort to find endpoint
      endpoint = Endpoint(boost::asio::ip::address::from_string(message.relay().ip()),
                          static_cast<unsigned short>(message.relay().port()));
    }
    message.set_destination_id(message.relay_id());  // so that peer identifies it as direct msg
    SendOn(message, rudp, routing_table, endpoint, false);
  }
}

}  // namespace routing

}  // namespace maidsafe
