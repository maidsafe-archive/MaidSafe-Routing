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

#ifndef MAIDSAFE_ROUTING_RPCS_H_
#define MAIDSAFE_ROUTING_RPCS_H_

#include <string>
#include <vector>

#include "boost/asio/ip/udp.hpp"

#include "maidsafe/common/node_id.h"

#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/api_config.h"


namespace maidsafe {

namespace routing {

namespace protobuf { class Message; }

namespace rpcs {

protobuf::Message Ping(const NodeId& node_id, const std::string& identity);

protobuf::Message Connect(const NodeId& node_id,
    const rudp::EndpointPair& our_endpoint,
    const NodeId& this_node_id,
    const NodeId& this_connection_id,
    bool client_node = false,
    rudp::NatType nat_type = rudp::NatType::kUnknown,
    bool relay_message = false,
    NodeId relay_connection_id = NodeId());

protobuf::Message Remove(const NodeId& node_id,
                         const NodeId& this_node_id,
                         const NodeId& this_connection_id,
                         const std::vector<std::string>& attempted_nodes);

protobuf::Message FindNodes(
    const NodeId& node_id,
    const NodeId& this_node_id,
    const int& num_nodes_requested,
    bool relay_message = false,
    NodeId relay_connection_id = NodeId());

protobuf::Message ProxyConnect(
    const NodeId& node_id,
    const NodeId& this_node_id,
    const rudp::EndpointPair& endpoint_pair,
    bool relay_message = false,
    NodeId relay_connection_id = NodeId());

protobuf::Message ConnectSuccess(
    const NodeId& node_id,
    const NodeId& this_node_id,
    const NodeId& this_connection_id,
    const bool& requestor,
    const bool& client_node);

protobuf::Message ConnectSuccessAcknowledgement(
    const NodeId& node_id,
    const NodeId& this_node_id,
    const NodeId& this_connection_id,
    const bool& requestor,
    const std::vector<NodeId>& close_ids,
    const bool& client_node);

protobuf::Message ClosestNodesUpdate(const NodeId& node_id,
    const NodeId& my_node_id,
    const std::vector<NodeInfo>& closest_nodes);

protobuf::Message ClosestNodesUpdateSubscribe(
    const NodeId& node_id,
    const NodeId& this_node_id,
    const NodeId &this_connection_id,
    const bool &client_node,
    const bool& subscribe);

protobuf::Message GetGroup(const NodeId& node_id,
                           const NodeId& my_node_id);

}  // namespace rpcs

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_RPCS_H_




