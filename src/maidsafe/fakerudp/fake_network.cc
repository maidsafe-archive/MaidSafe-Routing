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

Node::Node(ConnectionLostFunctor lost, MessageReceivedFunctor message_rec) {
  endpoint = FakeNetwork::instance().GetEndpoint();
  if (lost)
    connection_lost = lost;
  if (message_rec)
    message_received = message_rec;
}

Node::Node() {
  endpoint = FakeNetwork::instance().GetEndpoint();
}

FakeNetwork::FakeNetwork() : next_port_(1500) {
      boost::asio::ip::address ip;
      ip.from_string("8.8.8.8");
      local_ip_ = ip;
}

Endpoint FakeNetwork::GetEndpoint() {
  return Endpoint(local_ip_, ++next_port_);
}

std::vector<Node>::iterator FakeNetwork::FindNode(Endpoint endpoint) {
    std::lock_guard<std::mutex> lock(mutex_);
  return  std::find_if(nodes_.begin(),
                     nodes_.end(),
                    [=] (Node& element)
                     {
                       return (element.endpoint == endpoint);
                     }
                     );
}

bool FakeNetwork::BootStrap(Node &node, Endpoint &connect_to_endpoint) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (int i = 0; i < 200; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      auto iter2 = FindNode(connect_to_endpoint);
      if (iter2 != nodes_.end())
       (*iter2).connected_to_endpoints.push_back(node.endpoint);
      return true;
    }
  return false;
}

bool FakeNetwork::AddConnection(const Endpoint &my_endpoint, const Endpoint &peer_endpoint) {
      std::lock_guard<std::mutex> lock(mutex_);
      auto iter = std::find_if(nodes_.begin(),
                           nodes_.end(),
                          [=] (Node& element)
                           {
                             return (element.endpoint == my_endpoint);
                           }
                           );
     (*iter).connected_to_endpoints.push_back(peer_endpoint);
  return true;
}


bool FakeNetwork::RemoveMyNode(Endpoint endpoint) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto iter = FindNode(endpoint);
  if (iter != nodes_.end()) {
    for (auto i :  (*iter).connected_to_endpoints) {
      auto getit = FindNode(i);
      if (getit != nodes_.end()) {
        (*getit).connection_lost(endpoint);
        (*getit).connected_to_endpoints.erase(std::remove_if((*getit).connected_to_endpoints.begin(),
                                                           (*getit).connected_to_endpoints.end(),[&]
                                                           (Endpoint &element)
                                                           {
                                                           return element == endpoint;
                                                           }
                                                           ));
      }
    }
    nodes_.erase(iter);
    return true;
  }
  return false;
}

bool FakeNetwork::SendMessageToNode(Endpoint endpoint, std::string message) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto iter = FindNode(endpoint);
  if (iter == nodes_.end())
    return false;
  if ((*iter).message_received) {
    (*iter).message_received(message);
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
