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

#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/timer.h"

namespace maidsafe {

namespace routing {
typedef boost::asio::ip::udp::endpoint Endpoint;

struct RoutingPrivate {
public:
  ~RoutingPrivate() { DLOG(INFO) << "RoutingPrivate dtor"; }
private:
  RoutingPrivate(const asymm::Keys &keys,
                 const boost::filesystem::path &bootstrap_file_path,
                 bool client_mode);

  RoutingPrivate(const RoutingPrivate&);  // no copy
  RoutingPrivate(const RoutingPrivate&&);  // no move
  RoutingPrivate& operator=(const RoutingPrivate&);  // no assign
  friend class Routing;
  AsioService asio_service_;
  std::vector<boost::asio::ip::udp::endpoint> bootstrap_nodes_;
  const asymm::Keys keys_;
  rudp::ManagedConnections rudp_;
  RoutingTable routing_table_;
  Timer timer_;
  MessageHandler message_handler_;
  boost::signals2::signal<void(int, std::string)> message_received_signal_;
  boost::signals2::signal<void(int16_t)> network_status_signal_;
  boost::signals2::signal<void(std::string, std::string)>
                                                close_node_from_to_signal_;
  boost::signals2::signal<void(const std::string&,
                           const boost::asio::ip::udp::endpoint&,
                           const bool,
                           const boost::asio::ip::udp::endpoint&,
                           NodeValidatedFunctor &)> node_validation_signal_;
  std::map<uint32_t, std::pair<std::unique_ptr<boost::asio::deadline_timer>,
                              MessageReceivedFunctor> > waiting_for_response_;
  std::vector<NodeInfo> direct_non_routing_table_connections_;
  // closest nodes to the client.
  bool joined_;
  const boost::filesystem::path bootstrap_file_path_;
  bool client_mode_;
};


}  // namespace routing

}  // namespace maidsafe
