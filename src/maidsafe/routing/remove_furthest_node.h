/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

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
  void RejectRemoval(protobuf::Message& message);
  void RemoveResponse(protobuf::Message& message);
  void RemoveNodeRequest();

 private:
  RemoveFurthestNode(const RemoveFurthestNode&);
  RemoveFurthestNode(const RemoveFurthestNode&&);
  RemoveFurthestNode& operator=(const RemoveFurthestNode&);
  bool IsRemovable(const NodeId& node_id);
  void HandleRemoveRequest(const NodeId& node_id);

  RoutingTable& routing_table_;
  NetworkUtils& network_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_REMOVE_FURTHEST_NODE_H_
