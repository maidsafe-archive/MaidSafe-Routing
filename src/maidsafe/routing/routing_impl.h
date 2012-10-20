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
#include <memory>
#include <string>
#include <vector>

#include "boost/asio/deadline_timer.hpp"
#include "boost/asio/ip/udp.hpp"
#include "boost/date_time/posix_time/posix_time_config.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/system/error_code.hpp"

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/safe_queue.h"

#include "maidsafe/private/utils/fob.h"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/random_node_helper.h"
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
  Impl(const Fob& fob, bool client_mode);
  ~Impl();

  void Join(const Functors& functors,
            const std::vector<boost::asio::ip::udp::endpoint>& peer_endpoints =
                std::vector<boost::asio::ip::udp::endpoint>());

  int ZeroStateJoin(const Functors& functors,
                    const boost::asio::ip::udp::endpoint& local_endpoint,
                    const boost::asio::ip::udp::endpoint& peer_endpoint,
                    const NodeInfo& peer_info);

  void Send(const NodeId& destination_id,
            const NodeId& group_claim,
            const std::string& data,
            const ResponseFunctor& response_functor,
            const boost::posix_time::time_duration& timeout,
            bool direct,
            bool cacheable);

  NodeId GetRandomExistingNode() const { return random_node_helper_.Get(); }

  void DisconnectFunctors();

  friend class test::GenericNode;

 private:
  Impl(const Impl&);
  Impl(const Impl&&);
  Impl& operator=(const Impl&);

  bool CheckBootstrapFilePath();
  void ConnectFunctors(const Functors& functors);
  void BootstrapFromTheseEndpoints(const std::vector<boost::asio::ip::udp::endpoint>& endpoints);
  void DoJoin();
  int DoBootstrap();
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

  Functors functors_;
  std::vector<boost::asio::ip::udp::endpoint> bootstrap_nodes_;
  const Fob kFob_;
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
  RandomNodeHelper random_node_helper_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_PRIVATE_H_
