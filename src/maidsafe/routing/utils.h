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

#ifndef MAIDSAFE_ROUTING_UTILS_H_
#define MAIDSAFE_ROUTING_UTILS_H_


#include <string>
#include <vector>

#include "boost/asio/ip/udp.hpp"

#include "maidsafe/common/rsa.h"

#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/passport/types.h"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/routing.pb.h"


namespace maidsafe {

class NodeId;
namespace routing {

namespace fs = boost::filesystem;

namespace protobuf {

class Message;
class Endpoint;

}  // namespace protobuf

class Message {
  explicit Message(protobuf::Message message);
};

class NetworkUtils;
class ClientRoutingTable;
class RoutingTable;


int AddToRudp(NetworkUtils& network,
              const NodeId& this_node_id,
              const NodeId& this_connection_id,
              const NodeId& peer_id,
              const NodeId& peer_connection_id,
              rudp::EndpointPair peer_endpoint_pair,
              const bool &requestor,
              const bool& client);

bool ValidateAndAddToRoutingTable(NetworkUtils& network,
                                  RoutingTable& routing_table,
                                  ClientRoutingTable& client_routing_table,
                                  const NodeId& peer_id,
                                  const NodeId& connection_id,
                                  const asymm::PublicKey& public_key,
                                  const bool& client);
void HandleSymmetricNodeAdd(RoutingTable& routing_table, const NodeId& peer_id,
                            const asymm::PublicKey& public_key);
bool IsRoutingMessage(const protobuf::Message& message);
bool IsNodeLevelMessage(const protobuf::Message& message);
bool IsRequest(const protobuf::Message& message);
bool IsResponse(const protobuf::Message& message);
bool IsDirect(const protobuf::Message& message);
bool IsCacheable(const protobuf::Message& message);
bool CheckId(const std::string& id_to_test);
bool ValidateMessage(const protobuf::Message &message);
void SetProtobufEndpoint(const boost::asio::ip::udp::endpoint& endpoint,
                         protobuf::Endpoint* pb_endpoint);
boost::asio::ip::udp::endpoint GetEndpointFromProtobuf(const protobuf::Endpoint& pb_endpoint);
std::string MessageTypeString(const protobuf::Message& message);
std::vector<boost::asio::ip::udp::endpoint> OrderBootstrapList(
                                  std::vector<boost::asio::ip::udp::endpoint> peer_endpoints);
protobuf::NatType NatTypeProtobuf(const rudp::NatType& nat_type);
rudp::NatType NatTypeFromProtobuf(const protobuf::NatType& nat_type_proto);
std::string PrintMessage(const protobuf::Message& message);
std::vector<NodeId> DeserializeNodeIdList(const std::string &node_list_str);
std::string SerializeNodeIdList(const std::vector<NodeId> &node_list);
}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_UTILS_H_
