/* Copyright 2012 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

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
