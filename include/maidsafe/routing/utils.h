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

#ifndef MAIDSAFE_ROUTING_UTILS_H_
#define MAIDSAFE_ROUTING_UTILS_H_

#include <string>
#include <vector>

#include "boost/asio/ip/udp.hpp"

#include "maidsafe/common/rsa.h"

#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/passport/types.h"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/routing.pb.h"

namespace maidsafe {

class NodeId;
namespace routing {

namespace fs = boost::filesystem;

class ClientRoutingTable;

enum class MessageType : int32_t {
  kPing = 1,
  kConnect = 2,
  kFindNodes = 3,
  kConnectSuccess = 4,
  kConnectSuccessAcknowledgement = 5,
  kGetGroup = 6,
  kInformClientOfNewCloseNode = 7,
  kMaxRouting = 100,
  kNodeLevel = 101
};

template <MessageType action, bool type, typename Source>
struct MessageWrapper {
  typedef Source SourceType;

  explicit MessageWrapper(protobuf::Message message_in) : message(message_in) {}

//  MessageWrapper(const MessageWrapper& other);
//  MessageWrapper(MessageWrapper&& other);
//  MessageWrapper& operator=(MessageWrapper other);

  friend void swap(MessageWrapper& lhs, MessageWrapper& rhs) {
    using std::swap;
    swap(lhs.id, rhs.id);
    swap(lhs.contents, rhs.contents);
  }

  protobuf::Message message;
};

typedef MessageWrapper<MessageType::kConnectSuccessAcknowledgement, true, VaultNode>
            ConnectSuccessAcknowledgementRequestFromVault;

typedef MessageWrapper<MessageType::kConnectSuccessAcknowledgement, true, ClientNode>
            ConnectSuccessAcknowledgementRequestFromClient;

GroupRangeStatus GetProximalRange(const NodeId& target_id, const NodeId& node_id,
                                  const NodeId& this_node_id,
                                  const crypto::BigInt& proximity_radius,
                                  const std::vector<NodeId>& holders);

bool IsRoutingMessage(const protobuf::Message& message);
bool IsNodeLevelMessage(const protobuf::Message& message);
bool IsRequest(const protobuf::Message& message);
bool IsResponse(const protobuf::Message& message);
bool IsDirect(const protobuf::Message& message);
bool IsCacheableGet(const protobuf::Message& message);
bool IsCacheablePut(const protobuf::Message& message);
bool IsClientToClientMessageWithDifferentNodeIds(const protobuf::Message& message,
                                                 const bool is_destination_client);
bool CheckId(const std::string& id_to_test);
bool ValidateMessage(const protobuf::Message& message);
NodeId NodeInNthBucket(const NodeId& node_id, int bucket);
void SetProtobufEndpoint(const boost::asio::ip::udp::endpoint& endpoint,
                         protobuf::Endpoint* pb_endpoint);
boost::asio::ip::udp::endpoint GetEndpointFromProtobuf(const protobuf::Endpoint& pb_endpoint);
std::string MessageTypeString(const protobuf::Message& message);
std::vector<boost::asio::ip::udp::endpoint> OrderBootstrapList(
    std::vector<boost::asio::ip::udp::endpoint> peer_endpoints);
protobuf::NatType NatTypeProtobuf(const rudp::NatType& nat_type);
rudp::NatType NatTypeFromProtobuf(const protobuf::NatType& nat_type_proto);
std::string PrintMessage(const protobuf::Message& message);
std::vector<NodeId> DeserializeNodeIdList(const std::string& node_list_str);
std::string SerializeNodeIdList(const std::vector<NodeId>& node_list);
SingleToSingleMessage CreateSingleToSingleMessage(const protobuf::Message& proto_message);
SingleToGroupMessage CreateSingleToGroupMessage(const protobuf::Message& proto_message);
GroupToSingleMessage CreateGroupToSingleMessage(const protobuf::Message& proto_message);
GroupToGroupMessage CreateGroupToGroupMessage(const protobuf::Message& proto_message);
SingleToGroupRelayMessage CreateSingleToGroupRelayMessage(const protobuf::Message& proto_message);
}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_UTILS_H_
