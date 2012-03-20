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


#include "maidsafe/routing/rpcs.h"


namespace maidsafe {

namespace routing {


void Rpcs::Ping(protobuf::Message &message) {
  protobuf::PingRequest ping_request;
  ping_request.set_ping(true);
  message.set_destination_id(message.source_id());
  message.set_source_id(node_id_.String());
  message.set_data(ping_request.SerializeAsString());
  message.set_direct(true);
  message.set_response(true);
  message.set_replication(1);
  message.set_type(0);
  NodeId send_to(message.destination_id());
  SendOn(message, send_to);
}



}  // namespace routing

}  // namespace maidsafe
