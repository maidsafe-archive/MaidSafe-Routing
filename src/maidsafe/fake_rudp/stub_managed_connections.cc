/*******************************************************************************
 *  Copyright 2012 MaidSafe.net limited                                        *
 *                                                                             *
 *  The following source code is property of MaidSafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the licence   *
 *  file licence.txt found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of MaidSafe.net. *
 ******************************************************************************/


#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "boost/asio/ip/address.hpp"
#include "boost/asio/ip/udp.hpp"
#include "boost/date_time/posix_time/posix_time_duration.hpp"
#include "boost/signals2/connection.hpp"

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/rsa.h"

#include "maidsafe/rudp/nat_type.h"


namespace maidsafe {

namespace rudp {

namespace detail { class Transport; }

  NodeId ManagedConnections::Bootstrap(
      const std::vector<boost::asio::ip::udp::endpoint> &bootstrap_endpoints,
      bool start_resilience_transport,
      MessageReceivedFunctor message_received_functor,
      ConnectionLostFunctor connection_lost_functor,
      NodeId this_node_id,
      std::shared_ptr<asymm::PrivateKey> private_key,
      std::shared_ptr<asymm::PublicKey> public_key,
      NatType& nat_type,
      boost::asio::ip::udp::endpoint local_endpoint = boost::asio::ip::udp::endpoint()) {}

  int ManagedConnections::GetAvailableEndpoint(NodeId &peer_id,
                           const EndpointPair& peer_endpoint_pair,
                           EndpointPair& this_endpoint_pair,
                           NatType& this_nat_type) {}

  int ManagedConnections::Add(NodeId peer_id, const std::string& validation_data) {}

  int ManagedConnections::MarkConnectionAsValid(NodeId peer_id) {}

  void ManagedConnections::Remove(NodeId peer_id) {}

  void ManagedConnections::Send(NodeId peer_id,
            const std::string& message,
            MessageSentFunctor message_sent_functor) {}

  void ManagedConnections::Ping(const boost::asio::ip::udp::endpoint& peer_endpoint,
                                PingFunctor ping_functor) {}


}  // namespace rudp

}  // namespace maidsafe

