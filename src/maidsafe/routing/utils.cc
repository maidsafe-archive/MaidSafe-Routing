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
#include "maidsafe/routing/message.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"

namespace maidsafe {

namespace routing {

void SendOn(Message &message,
            rudp::ManagedConnections &rudp,
            RoutingTable &routing_table,
            Endpoint endpoint) {
  std::string signature;
//  asymm::Sign(message.Data(), routing_table.kKeys().private_key, &signature);
//  message.SetSignature(signature);
  if ((endpoint.address().is_unspecified()) && (routing_table.Size() > 0)) {
    endpoint = routing_table.GetClosestNode(message.DestinationId(), 0).endpoint;
  } else {
    LOG(kError) << " No Endpoint to send to (no RT), Aborting Send!"
                << " Attempt to send a type : " << message.Type() << " message"
                << " to " << HexSubstr(message.SourceId().String())
                << " From " << HexSubstr(routing_table.kKeys().identity);
    return;
  }
  int send_status = rudp.Send(endpoint, message.Serialise());
  if (send_status != rudp::kSuccess)
    LOG(kError) << " Send error !!! = " << send_status;
}


}  // namespace routing

}  // namespace maidsafe
