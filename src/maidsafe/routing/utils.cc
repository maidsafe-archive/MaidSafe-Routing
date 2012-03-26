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

#include "maidsafe/transport/managed_connections.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"


namespace maidsafe {

namespace routing {

void SendOn(protobuf::Message &message,
            std::shared_ptr<transport::ManagedConnections> transport,
            std::shared_ptr<RoutingTable> routing_table) {

  std::string signature;
  asymm::Sign(message.data(), keys_.private_key, &signature);
  message.set_signature(signature);
  NodeInfo next_node(transport->GetClosestNode(NodeId(message.destination_id()),
                                               0));
// FIXME SEND transport_->Send(next_node.endpoint, message.SerializeAsString());

}


}  // namespace routing

}  // namespace maidsafe
