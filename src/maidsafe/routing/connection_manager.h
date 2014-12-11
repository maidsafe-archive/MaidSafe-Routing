/*  Copyright 2014 MaidSafe.net limited

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

/*
This class is the API for the messages and routing object. It creates a routing table
and allows that to be managed by itself. This object will provide id's to connect
to that will allow routing to send a message closer to a target. If a close node cannot
be directly connected it will be added, only if, it is within the close nodes range.

This requires that the message_handler
1: On reciept of a message id (source + message id + destination) will
  a: send on to any id provided and firewall the message
  b: If multiple destinations are provided then the same happens
2: Prior to sending the node must check the message is not already firewalled
(outgoing message check)

To maintain close nodes effectivly the message_handler should request a close_group
request to its group when it sees any close group request in it's group. This is
obvious as the destination nodes for a messag ein your close group has multiple
destiations. In that case request a close_group message for this node.
*/

#ifndef MAIDSAFE_ROUTING_CONNECTION_MANAGER_H_
#define MAIDSAFE_ROUTING_CONNECTION_MANAGER_H_

#include <functional>
#include <map>
#include <mutex>
#include <vector>

#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/node_info.h"

namespace maidsafe {

namespace routing {

class ConnectionManager {
 public:
  ConnectionManager(rudp::ManagedConnections& rudp, Address our_id,
                    std::function<void(CloseGroupDifference)> group_changed_functor)
      : routing_table_(our_id),
        rudp_(rudp),
        current_close_group_(),
        group_changed_functor_(group_changed_functor) {
    assert(group_changed_functor_ && "functor required to be set");
  }
  ConnectionManager(const ConnectionManager&) = delete;
  ConnectionManager(ConnectionManager&&) = delete;
  ~ConnectionManager() = default;
  ConnectionManager& operator=(const ConnectionManager&) = delete;
  ConnectionManager& operator=(ConnectionManager&&) = delete;

  bool SuggestNodeToAdd(const Address& node_to_add) const;
  std::vector<NodeInfo> GetTarget(const Address& target_node) const;
  void LostNetworkConnection(const Address& node);
  // routing wishes to drop a specific node (may be a node we cannot connect to)
  void DropNode(const Address& their_id);
  void AddNode(NodeInfo node_to_add, rudp::EndpointPair their_endpoint_pair);
  std::vector<NodeInfo> OurCloseGroup() const { return routing_table_.OurCloseGroup(); }

 private:
  void GroupChanged();

  std::mutex mutex_;
  RoutingTable routing_table_;
  rudp::ManagedConnections& rudp_;
  std::vector<Address> current_close_group_;
  std::function<void(CloseGroupDifference)> group_changed_functor_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_CONNECTION_MANAGER_H_
