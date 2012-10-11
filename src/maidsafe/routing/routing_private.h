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

#ifndef MAIDSAFE_ROUTING_ROUTING_PRIVATE_H_
#define MAIDSAFE_ROUTING_ROUTING_PRIVATE_H_

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "boost/asio/deadline_timer.hpp"
#include "boost/asio/ip/udp.hpp"
#include "boost/filesystem/path.hpp"

#include "maidsafe/common/rsa.h"
#include "maidsafe/common/safe_queue.h"
#include "maidsafe/common/asio_service.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/timer.h"
#include "maidsafe/routing/service.h"

namespace maidsafe {

namespace routing {

class MessageHandler;

namespace test { class GenericNode; }

class RoutingPrivate {
 public:
  RoutingPrivate(const asymm::Keys& keys, bool client_mode);
  ~RoutingPrivate();
  void Stop();
  void Join(Functors functors,
            std::vector<boost::asio::ip::udp::endpoint> peer_endpoints =
                std::vector<boost::asio::ip::udp::endpoint>());

  int ZeroStateJoin(Functors functors,
                    const boost::asio::ip::udp::endpoint& local_endpoint,
                    const boost::asio::ip::udp::endpoint& peer_endpoint,
                    const NodeInfo& peer_node_info);

  void Send(const NodeId& destination_id,      // ID of final destination
            const NodeId& group_claim,         // ID claimed of sending group
            const std::string& data,           // message content (serialised data)
            ResponseFunctor response_functor,
            const boost::posix_time::time_duration& timeout,
            bool direct,  // whether this is to a close node group or direct
            bool cachable);

  NodeId GetRandomExistingNode();

  void DisconnectFunctors();

  friend class test::GenericNode;

 private:
  RoutingPrivate(const RoutingPrivate&);
  RoutingPrivate(const RoutingPrivate&&);
  RoutingPrivate& operator=(const RoutingPrivate&);

  bool CheckBootstrapFilePath();
  void AddExistingRandomNode(NodeId node);
  void RemoveLostRandomNode(NodeId node);
  void ConnectFunctors(const Functors& functors);
  void BootstrapFromTheseEndpoints(const std::vector<boost::asio::ip::udp::endpoint>& endpoints);
  void DoJoin();
  int DoBootstrap();
  void ReBootstrap();
  void DoReBootstrap(const boost::system::error_code &error_code);
  void FindClosestNode(const boost::system::error_code& error_code, int attempts);
  void ReSendFindNodeRequest(const boost::system::error_code& error_code, bool ignore_size = false);
  void OnMessageReceived(const std::string& message);
  void DoOnMessageReceived(const std::string& message);
  void OnConnectionLost(const NodeId& lost_connection_id);
  void DoOnConnectionLost(const NodeId& lost_connection_id);
  void RemoveNode(const NodeInfo& node, const bool& internal_rudp_only);
  bool ConfirmGroupMembers(const NodeId& node1, const NodeId& node2);

  Functors functors_;
  std::vector<boost::asio::ip::udp::endpoint> bootstrap_nodes_;
  const asymm::Keys keys_;
  const NodeId kNodeId_;
  std::atomic<bool> tearing_down_;
  RoutingTable routing_table_;
  NonRoutingTable non_routing_table_;
  AsioService asio_service_;
  Timer timer_;
  std::unique_ptr<MessageHandler> message_handler_;
  NetworkUtils network_;
  bool joined_;
  boost::filesystem::path bootstrap_file_path_;
  bool client_mode_;
  bool anonymous_node_;
  SafeQueue<NodeId> random_node_queue_;
  boost::asio::deadline_timer recovery_timer_;
  boost::asio::deadline_timer setup_timer_;
  boost::asio::deadline_timer re_bootstrap_timer_;
  std::vector<NodeId> random_node_vector_;
  std::mutex random_node_mutex_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_PRIVATE_H_
