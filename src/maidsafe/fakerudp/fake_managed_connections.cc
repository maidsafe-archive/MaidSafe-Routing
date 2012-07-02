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
#include "maidsafe/common/log.h"

namespace maidsafe {

namespace rudp {

typedef boost::asio::ip::udp::endpoint Endpoint;

ManagedConnections::ManagedConnections()  {
  Node node;
  bootstrap_endpoints_.push_back(node.endpoint);
  //FIXME
//  FakeNetwork::instance().AddEmptyNode(node);
}

ManagedConnections::~ManagedConnections() {}

boost::asio::ip::udp::endpoint ManagedConnections::Bootstrap(
    const std::vector<boost::asio::ip::udp::endpoint> &bootstrap_endpoints,
    MessageReceivedFunctor message_received_functor,
    ConnectionLostFunctor connection_lost_functor,
    Endpoint local_endpoint) {

  LOG(kVerbose) << "in bootstrap";

//  auto mynode = FakeNetwork::instance().FindNode(bootstrap_endpoints_[0]);
//  if (mynode == FakeNetwork.instance().GetEnd())
//    return Endpoint();
//  Node node = (*mynode);
  Node node;
  //FIXME
//  if (local_endpoint.address().is_unspecified())
//    node.endpoint = local_endpoint;
//  if (connection_lost_functor)
//    node.connection_lost = connection_lost_functor;
//  if (message_received_functor)
//    node.message_received = message_received_functor;

  for (auto i : bootstrap_endpoints) {
    for (int j = 0; j < 200; ++j) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      if (local_endpoint.address().is_unspecified() && (FakeNetwork::instance().FindNode(i) != FakeNetwork::instance().GetEnd())) {
        LOG(kVerbose) << "found viable bootstrap node \n";
        if(FakeNetwork::instance().BootStrap(node, i)) {
          LOG(kVerbose) << "Bootstrap sucessfull !!\n";
          Add(node.endpoint, i, "");
          return i;
        }
      } else {
        Add(node.endpoint, i, "");
         return i;
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
    LOG(kInfo) << " endpoint ip address " << this_endpoint_pair.external.address().to_string() << "\n";
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
