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

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/node_id.h"
#include "boost/system/system_error.hpp"
#include "boost/filesystem.hpp"
#include "boost/filesystem/fstream.hpp"


namespace maidsafe {

namespace routing {

void SendOn(protobuf::Message message,
            rudp::ManagedConnections &rudp,
            RoutingTable &routing_table) {
  std::string signature;
  asymm::Sign(message.data(), routing_table.kKeys().private_key, &signature);
  message.set_signature(signature);
  NodeInfo next_node;
   if (routing_table.Size() > 0){
     next_node = routing_table.GetClosestNode(NodeId(message.destination_id()), 0);
   } else {
     DLOG(ERROR) << "Attempt to send on when routing table full ";
     return;
   }
  rudp.Send(next_node.endpoint, message.SerializeAsString());
}


}  // namespace routing

}  // namespace maidsafe
