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

#ifndef MAIDSAFE_ROUTING_REMOVE_FURTHEST_NODE_H_
#define MAIDSAFE_ROUTING_REMOVE_FURTHEST_NODE_H_

#include <algorithm>


namespace maidsafe {


class NodeId;

namespace routing {

namespace protobuf { class Message; }

class RoutingTable;
class NetworkUtils;
struct NodeInfo;

class RemoveFurthestNode {
 public:
  RemoveFurthestNode(RoutingTable& routing_table,
             NetworkUtils& network);
  void RemoveRequest(protobuf::Message& message);
  void HandleRemoveRequest(const NodeId& node_id);
  void RejectRemoval(protobuf::Message& message);
  void RemoveResponse(protobuf::Message& message);
  void RemoveNodeRequest();

 private:
  RoutingTable& routing_table_;
  NetworkUtils& network_;
  bool IsRemovable(const NodeId& node_id);
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_REMOVE_FURTHEST_NODE_H_
