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

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <mutex>
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

struct Node;

// singleton
class FakeNetwork {
public:
  FakeNetwork();
  static FakeNetwork& instance() {
    static FakeNetwork FakeNetwork;
    return FakeNetwork;
  }
  Endpoint GetEndpoint();
  bool BootStrap(Node &node, Endpoint &connect_to_endpoint);
  bool RemoveMyNode(Endpoint endpoint);
  bool SendMessageToNode(Endpoint endpoint, std::string message);
  bool AddConnection(const Endpoint &my_endpoint, const Endpoint &peer_endpoint);
  std::vector<Node>::iterator FindNode(Endpoint endpoint);
  std::vector<Node>::iterator GetEndIterator() { return nodes_.end(); }
  void AddEmptyNode(Node node);
 private:
  std::vector<Node> nodes_;
  int32_t next_port_;
  boost::asio::ip::address local_ip_;
  std::mutex mutex_;
};

struct Node {
  Node(ConnectionLostFunctor lost, MessageReceivedFunctor message_rec);
  Node();
  Endpoint endpoint;
  ConnectionLostFunctor connection_lost;
  MessageReceivedFunctor message_received;
  std::vector<Endpoint> connected_to_endpoints;
};


}  // namespace rudp

}  // namespace maidsafe
