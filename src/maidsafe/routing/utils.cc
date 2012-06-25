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
  rudp::MessageSentFunctor message_sent_functor;
  if (relay_and_i_am_closest || direct_message || has_relay_ip) {  //  retry only once
    message_sent_functor = [](bool message_sent) {
    message_sent ?  LOG(kError) << "Sent to a relay node" :
                    LOG(kError) << "could not send to a relay node";
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

bool ClosestToMe(protobuf::Message &message, RoutingTable &routing_table) {
  return routing_table.AmIClosestNode(NodeId(message.destination_id()));
}

bool InClosestNodesToMe(protobuf::Message &message, RoutingTable &routing_table) {
  return routing_table.IsMyNodeInRange(NodeId(message.destination_id()),
                                       Parameters::closest_nodes_size);
}

void ValidateThisNode(rudp::ManagedConnections &rudp,
                      RoutingTable &routing_table,
                      const NodeId& node_id,
                      const asymm::PublicKey &public_key,
                      const rudp::EndpointPair &their_endpoint,
                      const rudp::EndpointPair &our_endpoint,
                      const bool &client) {
  NodeInfo node_info;
  node_info.node_id = NodeId(node_id);
  node_info.public_key = public_key;
  node_info.endpoint = their_endpoint.external;
  LOG(kVerbose) << "Calling rudp Add on endpoint = " << our_endpoint.external
                << ", their endpoint = " << their_endpoint.external;
  int result = rudp.Add(our_endpoint.external, their_endpoint.external, node_id.String());

  if (result != 0) {
      LOG(kWarning) << "rudp add failed " << result;
    return;
  }
  LOG(kVerbose) << "rudp.Add result = " << result;
  if (client) {
    //direct_non_routing_table_connections_.push_back(node_info);
    LOG(kError) << " got client and do not know how to add them yet ";
  } else {
    if(routing_table.AddNode(node_info)) {
      LOG(kVerbose) << "Added node to routing table. node id " << HexSubstr(node_id.String());
      SendOn(rpcs::ProxyConnect(node_id,routing_table.kKeys().identity, their_endpoint.external),
                                rudp,
                                routing_table,
                                Endpoint());  // check if this is a boostrap candidate
    } else {
      LOG(kVerbose) << "Not adding node to routing table  node id "
                    << HexSubstr(node_id.String())
                    << " just added rudp connection will be removed now";
      rudp.Remove(their_endpoint.external);
    }
  }
}

}  // namespace routing

}  // namespace maidsafe
