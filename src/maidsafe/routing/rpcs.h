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

#include <memory>
#include "maidsafe/common/rsa.h"
#include "maidsafe/transport/managed_connections.h"

namespace maidsafe {

namespace transport { class ManagedConnections; }

namespace routing {

namespace protobuf { class Message; }
class Routing;
class Endpoint;
class RoutingTable;
class NodeId;

// Send request to the network
class Rpcs {
 public:
  Rpcs(std::shared_ptr<RoutingTable> routing_table,
       transport::ManagedConnections &transport);
  void Ping(const NodeId &node_id);
  void Connect(const NodeId &node_id,
                const transport::Endpoint &our_endpoint);
  void FindNodes(const NodeId &node_id);

 private:
  std::shared_ptr<RoutingTable> routing_table_;
  transport::ManagedConnections transport_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_RPCS_H_




