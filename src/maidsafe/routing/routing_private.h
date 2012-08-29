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


namespace maidsafe {

namespace routing {

class MessageHandler;

namespace test { class GenericNode; }

struct RoutingPrivate {
 public:
  ~RoutingPrivate();

  friend class Routing;
  friend class test::GenericNode;

 private:
  RoutingPrivate(const RoutingPrivate&);
  RoutingPrivate(const RoutingPrivate&&);
  RoutingPrivate& operator=(const RoutingPrivate&);
  RoutingPrivate(const asymm::Keys& keys, bool client_mode);

  AsioService asio_service_;
  std::vector<boost::asio::ip::udp::endpoint> bootstrap_nodes_;
  const asymm::Keys keys_;
  std::atomic<bool> tearing_down_;
  RoutingTable routing_table_;
  NonRoutingTable non_routing_table_;
  Timer timer_;
  std::map<uint32_t, std::pair<std::unique_ptr<boost::asio::deadline_timer>,
                               MessageReceivedFunctor>> waiting_for_response_;
  std::unique_ptr<MessageHandler> message_handler_;
  NetworkUtils network_;
  bool joined_;
  boost::filesystem::path bootstrap_file_path_;
  bool client_mode_;
  bool anonymous_node_;
  Functors functors_;
  SafeQueue<NodeId> random_node_queue_;
  boost::asio::deadline_timer recovery_timer_;
#ifdef LOCAL_TEST
  void LocalTestUtility(const protobuf::Message message, uint16_t& expected_connect_response);
  static std::vector<boost::asio::ip::udp::endpoint> bootstraps_;
  static std::mutex mutex_;
  static uint16_t network_size_;
#endif
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_PRIVATE_H_
