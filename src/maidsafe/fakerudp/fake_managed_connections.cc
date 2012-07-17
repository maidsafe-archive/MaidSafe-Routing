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

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>

#include "boost/asio/ip/address.hpp"
#include "boost/asio/ip/udp.hpp"
#include "boost/date_time/posix_time/posix_time_duration.hpp"
#include "boost/signals2/connection.hpp"
#include "boost/thread/shared_mutex.hpp"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"
#include "maidsafe/fakerudp/fake_network.h"
#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/log.h"

namespace maidsafe {

namespace rudp {

typedef boost::asio::ip::udp::endpoint Endpoint;

ManagedConnections::ManagedConnections()
    : asio_service_(2),
      message_received_functor_(),
      connection_lost_functor_(),
      transports_(),
      connection_map_(),
      shared_mutex_(),
      bootstrap_endpoints_() {
  Node node;
  bootstrap_endpoints_.push_back(node.endpoint);
  FakeNetwork::instance().AddEmptyNode(node);
  asio_service_.Start();
}

ManagedConnections::~ManagedConnections() {
  if (!FakeNetwork::instance().RemoveMyNode(bootstrap_endpoints_[0])) {
    LOG(kVerbose) << "Failed to remove my node in destructor.";
  }
}

Endpoint ManagedConnections::Bootstrap(const std::vector<Endpoint> &bootstrap_endpoints,
                                       MessageReceivedFunctor message_received_functor,
                                       ConnectionLostFunctor connection_lost_functor,
                                       Endpoint local_endpoint) {
  LOG(kVerbose) << "In Bootstrap";

  if (!message_received_functor) {
    LOG(kError) << "You must provide a valid MessageReceivedFunctor.";
    return Endpoint();
  }
  message_received_functor_ = message_received_functor;
  if (!connection_lost_functor) {
    LOG(kError) << "You must provide a valid ConnectionLostFunctor.";
    return Endpoint();
  }
  connection_lost_functor_ = connection_lost_functor;

  if (bootstrap_endpoints.empty()) {
    LOG(kError) << "You must provide at least one Bootstrap endpoint.";
    return Endpoint();
  }

  auto mynode = FakeNetwork::instance().FindNode(bootstrap_endpoints_[0]);  // me
  assert(mynode != FakeNetwork::instance().GetEndIterator() && "Apparently not in network.");

  Node &node = (*mynode);
  if (!local_endpoint.address().is_unspecified()) {
    node.endpoint = local_endpoint;
    bootstrap_endpoints_[0] = local_endpoint;
  }

  if (connection_lost_functor)
    node.connection_lost = connection_lost_functor;
  if (message_received_functor)
    node.message_received = message_received_functor;

  for (auto i : bootstrap_endpoints) {
    for (int j = 0; j < 200; ++j) {
//      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      Sleep(boost::posix_time::milliseconds(10));
      if (local_endpoint.address().is_unspecified() &&
          (FakeNetwork::instance().FindNode(i) != FakeNetwork::instance().GetEndIterator())) {
        LOG(kVerbose) << "Found viable bootstrap node.\n";
        if (FakeNetwork::instance().BootStrap(node, i)) {
          LOG(kVerbose) << "Bootstrap sucessfull!!\n";
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

int ManagedConnections::GetAvailableEndpoint(const Endpoint& /*peer_endpoint*/,
                                             EndpointPair &this_endpoint_pair) {
  assert((bootstrap_endpoints_.size() != 0) && "I do not know my own endpoint");
  this_endpoint_pair.external = bootstrap_endpoints_[0];
  this_endpoint_pair.local = bootstrap_endpoints_[0];
  LOG(kInfo) << " endpoint ip address "
             << this_endpoint_pair.external.address().to_string() << "\n";
  return kSuccess;
}

int ManagedConnections::Add(const Endpoint &this_endpoint,
                            const Endpoint &peer_endpoint,
                            const std::string &validation_data) {
  asio_service_.service().post([=]() {
    FakeNetwork::instance().AddConnection(this_endpoint, peer_endpoint);
    if (!validation_data.empty())
      FakeNetwork::instance().SendMessageToNode(peer_endpoint, validation_data);
  });
  return kSuccess;
}

void ManagedConnections::Send(const Endpoint &peer_endpoint,
                              const std::string &message,
                              MessageSentFunctor message_sent_functor) {
  asio_service_.service().post([=]() {
    bool message_sent(FakeNetwork::instance().SendMessageToNode(peer_endpoint, message));
    if (message_sent_functor)
      message_sent_functor(message_sent);
  });
}

void ManagedConnections::Remove(const Endpoint &peer_endpoint) {
  asio_service_.service().post([=]() {
    if (!FakeNetwork::instance().RemoveConnection(bootstrap_endpoints_[0], peer_endpoint))
      LOG(kVerbose) << "Failed to remove " << peer_endpoint;
  });
}
}  // namespace rudp

}  // namespace maidsafe
