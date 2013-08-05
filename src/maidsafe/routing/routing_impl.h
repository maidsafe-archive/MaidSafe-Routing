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

#ifndef MAIDSAFE_ROUTING_ROUTING_IMPL_H_
#define MAIDSAFE_ROUTING_ROUTING_IMPL_H_

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "boost/asio/deadline_timer.hpp"
#include "boost/asio/ip/udp.hpp"
#include "boost/date_time/posix_time/posix_time_config.hpp"
#include "boost/system/error_code.hpp"

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/node_id.h"

#include "maidsafe/common/rsa.h"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/client_routing_table.h"
#include "maidsafe/routing/group_change_handler.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/random_node_helper.h"
#include "maidsafe/routing/remove_furthest_node.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/timer.h"


namespace maidsafe {

namespace routing {

class MessageHandler;
struct NodeInfo;

namespace test { class GenericNode; }

class Routing::Impl {
 public:
  Impl(bool client_mode, const NodeId& node_id, const asymm::Keys& keys);
  ~Impl();

  void Join(const Functors& functors,
            const std::vector<boost::asio::ip::udp::endpoint>& peer_endpoints =
                std::vector<boost::asio::ip::udp::endpoint>());

  int ZeroStateJoin(const Functors& functors,
                    const boost::asio::ip::udp::endpoint& local_endpoint,
                    const boost::asio::ip::udp::endpoint& peer_endpoint,
                    const NodeInfo& peer_info);

  template <typename T>
  void Send(const T& message);  // New API

  void SendDirect(const NodeId& destination_id,
                  const std::string& data,
                  const bool& cacheable,
                  ResponseFunctor response_functor);

  void SendGroup(const NodeId& destination_id,
                 const std::string& data,
                 const bool& cacheable,
                 ResponseFunctor response_functor);

  NodeId GetRandomExistingNode() const { return random_node_helper_.Get(); }

  bool ClosestToId(const NodeId& node_id);

  GroupRangeStatus IsNodeIdInGroupRange(const NodeId& group_id);

  GroupRangeStatus IsNodeIdInGroupRange(const NodeId& group_id, const NodeId& node_id);

  NodeId RandomConnectedNode();

  bool EstimateInGroup(const NodeId& sender_id, const NodeId& info_id);

  std::future<std::vector<NodeId>> GetGroup(const NodeId& group_id);

  NodeId kNodeId() const;

  int network_status();

  std::vector<NodeInfo> ClosestNodes();

  bool IsConnectedVault(const NodeId& node_id);
  bool IsConnectedClient(const NodeId& node_id);

  friend class test::GenericNode;

 private:
  Impl(const Impl&);
  Impl(const Impl&&);
  Impl& operator=(const Impl&);

  void ConnectFunctors(const Functors& functors);
  void BootstrapFromTheseEndpoints(const std::vector<boost::asio::ip::udp::endpoint>& endpoints);
  void DoJoin(const std::vector<boost::asio::ip::udp::endpoint>& endpoints);
  int DoBootstrap(const std::vector<boost::asio::ip::udp::endpoint>& endpoints);
  void ReBootstrap();
  void DoReBootstrap(const boost::system::error_code &error_code);
  void FindClosestNode(const boost::system::error_code& error_code, int attempts);
  void ReSendFindNodeRequest(const boost::system::error_code& error_code, bool ignore_size);
  void OnMessageReceived(const std::string& message);
  void DoOnMessageReceived(const std::string& message);
  void OnConnectionLost(const NodeId& lost_connection_id);
  void DoOnConnectionLost(const NodeId& lost_connection_id);
  void RemoveNode(const NodeInfo& node, bool internal_rudp_only);
  bool ConfirmGroupMembers(const NodeId& node1, const NodeId& node2);
  void NotifyNetworkStatus(int return_code) const;
  void Send(const NodeId& destination_id, const std::string& data,
            const DestinationType& destination_type, const bool& cacheable,
            ResponseFunctor response_functor);
  void SendMessage(const NodeId& destination_id, protobuf::Message& proto_message);
  void PartiallyJoinedSend(protobuf::Message& proto_message);
  protobuf::Message CreateNodeLevelPartialMessage(
      const NodeId& destination_id,
      const DestinationType& destination_type,
      const std::string& data,
      const bool& cacheable);
  void CheckSendParameters(const NodeId& destination_id, const std::string& data);

  template <typename T>
  protobuf::Message CreateNodeLevelMessage(const T& message);
  template<typename T>
  void AddGroupSourceRelatedFields(protobuf::Message& proto_message, std::true_type);
  template<typename T>
  void AddGroupSourceRelatedFields(protobuf::Message& proto_message, std::false_type);

  template<typename Message>
  void AddDestinationTypeRelatedFields(protobuf::Message& proto_message, std::true_type);
  template<typename Message>
  void AddDestinationTypeRelatedFields(protobuf::Message& proto_message, std::false_type);

  std::mutex network_status_mutex_;
  int network_status_;
  RoutingTable routing_table_;
  const NodeId kNodeId_;
  bool running_;
  std::mutex running_mutex_;
  Functors functors_;
  RandomNodeHelper random_node_helper_;
  ClientRoutingTable client_routing_table_;
  RemoveFurthestNode remove_furthest_node_;
  GroupChangeHandler group_change_handler_;
  NetworkStatistics network_statistics_;
  // The following variables' declarations should remain the last ones in this class and should stay
  // in the order: message_handler_, asio_service_, network_, all timers.  This is important for the
  // proper destruction of the routing library, i.e. to avoid segmentation faults.
  std::unique_ptr<MessageHandler> message_handler_;
  AsioService asio_service_;
  NetworkUtils network_;
  Timer<std::string> timer_;
  boost::asio::deadline_timer re_bootstrap_timer_, recovery_timer_, setup_timer_;
};

// Implementations
template <typename T>
void Routing::Impl::Send(const T& message) {  // FIXME(Fix caching)
  protobuf::Message proto_message = CreateNodeLevelMessage(message);
  SendMessage(message.receiver.string(), proto_message);
}

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_IMPL_H_
