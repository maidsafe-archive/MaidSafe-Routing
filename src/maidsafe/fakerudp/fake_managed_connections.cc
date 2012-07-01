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
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"
#include "maidsafe/fakerudp/fake_network.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>
#include "boost/asio/ip/address.hpp"
#include "boost/asio/ip/udp.hpp"
#include "boost/date_time/posix_time/posix_time_duration.hpp"
#include "boost/signals2/connection.hpp"
#include "boost/thread/shared_mutex.hpp"

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/utils.h"

namespace maidsafe {

namespace rudp {

typedef boost::asio::ip::udp::endpoint Endpoint;

ManagedConnections::ManagedConnections()  {
    // make bootstrap_endpoints_[0] == our endpoint;
    bootstrap_endpoints_.push_back(FakeNetwork::instance().GetEndpoint());
}
ManagedConnections::~ManagedConnections()  {}

boost::asio::ip::udp::endpoint ManagedConnections::Bootstrap(
    const std::vector<boost::asio::ip::udp::endpoint> &bootstrap_endpoints,
    MessageReceivedFunctor message_received_functor,
    ConnectionLostFunctor connection_lost_functor,
    Endpoint local_endpoint) {

  Node node(connection_lost_functor, message_received_functor);
  if(!local_endpoint.address().is_unspecified())
    node.endpoint = local_endpoint;
  if (bootstrap_endpoints_.empty())
  bootstrap_endpoints_.push_back(node.endpoint);

  for (auto i : bootstrap_endpoints) {
    for (int j = 0; j < 200; ++j) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      if (FakeNetwork::instance().FindNode(i) != FakeNetwork::instance().GetEnd()) {
        if(FakeNetwork::instance().BootStrap(node, i)) {
          std::cout << "BOOTSTRAPPING ZERO STATE !!";
          return node.endpoint;
        }
      }
    }
  }
  return Endpoint();
}

int ManagedConnections::GetAvailableEndpoint(const boost::asio::ip::udp::endpoint& /*peer_endpoint*/,
                                             EndpointPair &this_endpoint_pair) {
  if (bootstrap_endpoints_.size() > 0) {
    this_endpoint_pair.external = bootstrap_endpoints_[0];
    this_endpoint_pair.local = bootstrap_endpoints_[0]; 
    return kSuccess;
  } else {
    return kError;
  }
}

int ManagedConnections::Add(const boost::asio::ip::udp::endpoint &this_endpoint,
                            const boost::asio::ip::udp::endpoint &peer_endpoint,
                            const std::string &/*validation_data*/) {
  FakeNetwork::instance().AddConnection(this_endpoint, peer_endpoint);
  return kSuccess;
}

void ManagedConnections::Send(const boost::asio::ip::udp::endpoint &peer_endpoint,
          const std::string &message,
          MessageSentFunctor message_sent_functor) const {
  message_sent_functor(FakeNetwork::instance().SendMessageToNode(peer_endpoint, message));
}

void ManagedConnections::Remove(const boost::asio::ip::udp::endpoint &peer_endpoint) {
 // find in network remove and fire connection losts's
}

}  // namespace rudp

}  // namespace maidsafe
