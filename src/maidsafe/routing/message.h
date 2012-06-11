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

#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/parameters.h"

namespace maidsafe {

namespace routing {
// Wrapper class currently around protocol buffers to aid logic and allow
// changing serialisation later if required
class Message {
 public:
  Message(const std::string &message,
          RoutingTable &routing_table,
          NonRoutingTable &non_routing_table);
  bool Valid();
  int32_t Type();
  bool ClosestToMe();
  bool InClosestNodes();
  bool InManagedGroup();
  bool ForMe();
  bool ForNonRoutingNode();
  bool ForBootstrapNode();
  bool HasRelay();
  int32_t Id();
  NodeId LastId();
  NodeId DestinationId();
  NodeId SourceId();
  Endpoint RelayEndpoint();
  ConnectType Direct();
  void SetMeAsLast();
  void SetMeAsSource();
  void SetDestination(std::string destination);
  void SetData(std::string data);
  void SetRelayEndpoint(Endpoint endpoint);
  void SetDirect(ConnectType);
  void SetSignature(std::string signature);
  void SetType(int32_t type);
  protobuf::PbMessage GetMessage();
  std::string Serialise();
  std::string Data();
  std::string Signature();
 private:
  RoutingTable &routing_table_;
  NonRoutingTable &non_routing_table_;
  protobuf::PbMessage message_;
  bool ParseMessage();
  std::string my_id_as_string_;
  NodeId my_id_;
  bool valid_;
  bool closest_to_me_;
  bool closest_to_me_set_;
  bool in_closest_nodes_;
  bool in_closest_nodes_set_;
  bool in_managed_group_;
  bool in_managed_group_set_;

};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_RPCS_H_




