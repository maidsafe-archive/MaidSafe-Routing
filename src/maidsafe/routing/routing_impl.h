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
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/random_node_helper.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/timer.h"
#include "maidsafe/routing/remove_furthest_node.h"
#include "maidsafe/routing/group_change_handler.h"


namespace maidsafe {

namespace routing {

class MessageHandler;
struct NodeInfo;

namespace test { class GenericNode; }

class Routing::Impl {
 public:
  Impl(bool client_mode, bool anonymous, const NodeId& node_id, const asymm::Keys& keys);
  ~Impl();

  void Join(const Functors& functors,
            const std::vector<boost::asio::ip::udp::endpoint>& peer_endpoints =
                std::vector<boost::asio::ip::udp::endpoint>());

  int ZeroStateJoin(const Functors& functors,
                    const boost::asio::ip::udp::endpoint& local_endpoint,
                    const boost::asio::ip::udp::endpoint& peer_endpoint,
                    const NodeInfo& peer_info);

  void Send(const NodeId& destination_id,
            const std::string& data,
            const ResponseFunctor& response_functor,
            const DestinationType& destination_type,
            const bool& cacheable);

  NodeId GetRandomExistingNode() const { return random_node_helper_.Get(); }

  bool IsNodeIdInGroupRange(const NodeId& node_id);

  NodeId kNodeId() const;

  int network_status();

  std::vector<NodeInfo> ClosestNodes();

  bool IsConnectedToVault(const NodeId& node_id);
  bool IsConnectedToClient(const NodeId& node_id);

  void DisconnectFunctors();

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

  std::mutex network_status_mutex_;
  int network_status_;
  RoutingTable routing_table_;
  const NodeId kNodeId_;
  const bool kAnonymousNode_;
  bool running_;
  std::mutex running_mutex_;
  Functors functors_;
  RandomNodeHelper random_node_helper_;
  NonRoutingTable non_routing_table_;
  RemoveFurthestNode remove_furthest_node_;
  GroupChangeHandler group_change_handler_;
  // The following variables' declarations should remain the last ones in this class and should stay
  // in the order: message_handler_, asio_service_, network_, all timers.  This is important for the
  // proper destruction of the routing library, i.e. to avoid segmentation faults.
  std::unique_ptr<MessageHandler> message_handler_;
  AsioService asio_service_;
  NetworkUtils network_;
  Timer timer_;
  boost::asio::deadline_timer re_bootstrap_timer_, recovery_timer_, setup_timer_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_IMPL_H_
