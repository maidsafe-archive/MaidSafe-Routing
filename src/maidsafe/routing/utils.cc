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

namespace maidsafe {

namespace routing {

void SendOn(protobuf::Message message,
            rudp::ManagedConnections &rudp,
            RoutingTable &routing_table,
            Endpoint endpoint) {
  std::string signature;
  asymm::Sign(message.data(), routing_table.kKeys().private_key, &signature);
  message.set_signature(signature);
  bool relay_and_i_am_closest(false);
  bool has_relay_ip(false);
  bool direct_message(!endpoint.address().is_unspecified());
  if (!direct_message) {
    LOG(kVerbose) << "message.has_relay()" << message.has_relay();
    LOG(kVerbose) << "message.has_relay_id()" << message.has_relay_id();
    LOG(kVerbose) << "routing_table.AmIClosest" << routing_table.AmIClosestNode(NodeId(message.destination_id()));
    relay_and_i_am_closest =
        ((message.has_relay_id()) && (routing_table.AmIClosestNode(NodeId(message.destination_id()))));
    has_relay_ip = message.has_relay();
    if (relay_and_i_am_closest) {
      //TODO(Prakash) find the endpoint of the node from my RT
      endpoint = Endpoint();
    } else if (has_relay_ip) {  // Message to new guy whose ID is not in anyones RT
      endpoint = Endpoint(boost::asio::ip::address::from_string(message.relay().ip()),
                          static_cast<unsigned short>(message.relay().port()));
      LOG(kInfo) << "Sending to non routing table node message type : "
                 << message.type() << " message"
                 << " to " << HexSubstr(message.source_id())
                 << " From " << HexSubstr(routing_table.kKeys().identity);

    } else if (routing_table.Size() > 0) {
      endpoint = routing_table.GetClosestNode(NodeId(message.destination_id()), 0).endpoint;
      LOG(kVerbose) << " Endpoint to send to routing table size > 0 " << endpoint;
    } else {
      LOG(kError) << " No Endpoint to send to, Aborting Send!"
                  << " Attempt to send a type : " << message.type() << " message"
                  << " to " << HexSubstr(message.source_id())
                  << " From " << HexSubstr(routing_table.kKeys().identity);
      return;
    }
  } else {
    LOG(kVerbose) << " Direct message to " << endpoint;
  }
  std::string serialised_message(message.SerializeAsString());
  uint16_t attempt_count(0);

  rudp::MessageSentFunctor message_sent_functor = [&](bool message_sent) {
      if (!message_sent) {
        ++attempt_count;
        if (relay_and_i_am_closest || direct_message || has_relay_ip) {  //  retry only once
          if (attempt_count == 1) {
            rudp.Send(endpoint, serialised_message, message_sent_functor);
          } else {
            LOG(kError) << " Send error on " << (relay_and_i_am_closest? "relay" : "direct")
                        << " message!!!" << "attempt" << attempt_count << " times";
            return;
          }
        }
        if ((attempt_count < Parameters::closest_nodes_size) &&
            (routing_table.Size() > attempt_count)) {
          LOG(kInfo) << " Sending attempt " << attempt_count;
          endpoint = routing_table.GetClosestNode(NodeId(message.destination_id()),
                                                  attempt_count).endpoint;
          rudp.Send(endpoint, serialised_message, message_sent_functor);
        } else {
          LOG(kError) << " Send error !!! failed " << "attempt" << attempt_count << " times";
        }
      } else {
        LOG(kInfo) << " Send succeeded at attempt " << attempt_count;
      }
    };
  LOG(kVerbose) << " >>>>>>>>>>>>>>> rudp send message to " << endpoint << " <<<<<<<<<<<<<<<<<<<<";
  rudp.Send(endpoint, serialised_message, message_sent_functor);
}

bool ClosestToMe(protobuf::Message &message, RoutingTable &routing_table) {
  return routing_table.AmIClosestNode(NodeId(message.destination_id()));
}

bool InClosestNodesToMe(protobuf::Message &message, RoutingTable &routing_table) {
  return routing_table.IsMyNodeInRange(NodeId(message.destination_id()),
                                       Parameters::closest_nodes_size);
}


}  // namespace routing

}  // namespace maidsafe
