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

#ifndef MAIDSAFE_ROUTING_RPCS_H_
#define MAIDSAFE_ROUTING_RPCS_H_

#include <string>

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/parameters.h"

namespace maidsafe {

namespace routing {

namespace protobuf { class Message;}  // namespace protobuf

namespace rpcs {

const protobuf::Message Ping(const NodeId &node_id,
                             const std::string &identity);

const protobuf::Message Connect(const NodeId &node_id,
                                const rudp::EndpointPair &our_endpoint,
                                const NodeId &my_node_id,
                                bool client_node = false,
                                bool relay_message = false,
                                Endpoint local_endpoint = Endpoint());

const protobuf::Message FindNodes(const NodeId &node_id,
                                  const NodeId &my_node_id,
                                  bool relay_message = false,
                                  Endpoint local_endpoint = Endpoint());

const protobuf::Message ProxyConnect(const NodeId &node_id,
                                     const NodeId &my_node_id,
                                     const rudp::EndpointPair &endpoint_pair,
                                     bool relay_message = false,
                                     Endpoint local_endpoint = Endpoint());
}  // namespace rpcs

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_RPCS_H_




