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
#include "maidsafe/fakerudp/fake_network.h"

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

#include "maidsafe/common/log.h"
#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/utils.h"

namespace maidsafe {

namespace rudp {

typedef boost::asio::ip::udp::endpoint Endpoint;


Node::Node()
    : endpoint(FakeNetwork::instance().GetEndpoint()),
      connection_lost(),
      message_received(),
      connected_endpoints() {}

FakeNetwork::FakeNetwork()
    : nodes_(),
      next_port_(1500),
      local_ip_(),
      mutex_() {}

Endpoint FakeNetwork::GetEndpoint() {
  return Endpoint(boost::asio::ip::address::from_string("8.8.8.8"), ++next_port_);
}

std::vector<Node>::iterator FakeNetwork::FindNode(Endpoint endpoint) {
  // std::lock_guard<std::mutex> lock(mutex_);
  return  std::find_if(nodes_.begin(),
                       nodes_.end(),
                       [=] (Node& element) {
                          return (element.endpoint == endpoint);
                       });
}

bool FakeNetwork::BootStrap(Node &node, Endpoint &connect_to_endpoint) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (int i = 0; i < 200; ++i) {
    Sleep(boost::posix_time::milliseconds(10));
//    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto iter2 = FindNode(connect_to_endpoint);
    if (iter2 != nodes_.end()) {
      iter2->connected_endpoints.push_back(node.endpoint);
      return true;
    }
  }
  return false;
}

bool FakeNetwork::AddConnection(const Endpoint &my_endpoint, const Endpoint &peer_endpoint) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto iter = std::find_if(nodes_.begin(),
                           nodes_.end(),
                           [=] (Node& element) {
                              return (element.endpoint == my_endpoint);
                           });
  if (iter == nodes_.end()) {
    LOG(kError) << "Failed to find " << my_endpoint << " on network.";
    return false;
  }
  auto itr = std::find_if(iter->connected_endpoints.begin(),
                          iter->connected_endpoints.end(),
                          [&](Endpoint &endpoint) {
                              return (endpoint == peer_endpoint);
                          });
  if (itr == iter->connected_endpoints.end()) {
    iter->connected_endpoints.push_back(peer_endpoint);
  } else {
    LOG(kInfo) << "Connection already exists between " << my_endpoint << " and " << peer_endpoint;
  }
  return true;
}

bool FakeNetwork::RemoveConnection(const Endpoint &my_endpoint, const Endpoint &peer_endpoint) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto my_iter = FindNode(my_endpoint);
  if (my_iter != nodes_.end()) {
    auto itr = std::find_if(my_iter->connected_endpoints.begin(),
                            my_iter->connected_endpoints.end(),
                            [&](Endpoint &endpoint) {
                                return (endpoint == peer_endpoint);
                            });
    if (itr != my_iter->connected_endpoints.end()) {
      my_iter->connection_lost(peer_endpoint);
      my_iter->connected_endpoints.erase(itr);
    } else {
      LOG(kWarning) << "Failed to find connection form " << my_endpoint << " to " << peer_endpoint;
    }
    auto peer_iter = FindNode(peer_endpoint);
    if (peer_iter != nodes_.end()) {
      auto itr = std::find_if(peer_iter->connected_endpoints.begin(),
                              peer_iter->connected_endpoints.end(),
                              [&](Endpoint &endpoint) {
                                  return (endpoint == my_endpoint);
                              });
      if (itr != peer_iter->connected_endpoints.end()) {
        peer_iter->connection_lost(my_endpoint);
        peer_iter->connected_endpoints.erase(itr);
      } else {
        LOG(kWarning) << "Failed to find connection from " << peer_endpoint << " to "
                      << my_endpoint;
      }
    } else {
      LOG(kError) << "Failed to find " << peer_endpoint << " on network.";
      return false;
    }
  } else {
    LOG(kError) << "Failed to find " << my_endpoint << " on network.";
    return false;
  }
  return true;
}

bool FakeNetwork::RemoveMyNode(Endpoint endpoint) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto iter = FindNode(endpoint);
  if (iter != nodes_.end()) {
    for (auto i : iter->connected_endpoints) {
      auto it = FindNode(i);
      if (it != nodes_.end()) {
        // it->connection_lost(endpoint);
        it->connected_endpoints.erase(
            std::remove_if(it->connected_endpoints.begin(),
                           it->connected_endpoints.end(),
                           [&](Endpoint &element) {
                              return element == endpoint;
                           }));
      }
    }
    nodes_.erase(iter);
    return true;
  }
  return false;
}

bool FakeNetwork::SendMessageToNode(Endpoint endpoint, std::string message) {
  MessageReceivedFunctor message_received;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto iter = FindNode(endpoint);
    if (iter == nodes_.end())
      return false;
    if (iter->message_received)
      message_received = iter->message_received;
  }
  if (message_received) {
    message_received(message);
    return true;
  } else {
    return false;
  }
}

void FakeNetwork::AddEmptyNode(Node node) {
  nodes_.push_back(node);
}

}  // namespace fakerudp

}  // namespace maidsafe
