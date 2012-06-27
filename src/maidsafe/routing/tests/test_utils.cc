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

#include "maidsafe/routing/tests/test_utils.h"
#include <set>

#include "maidsafe/common/utils.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/log.h"

namespace asio = boost::asio;
namespace ip = asio::ip;

namespace maidsafe {

namespace routing {

namespace test {

uint16_t GetRandomPort() {
  static std::set<uint16_t> already_used_ports;
  bool unique(false);
  uint16_t port(0);
  uint16_t failed_attempts(0);
  do {
    assert((1000 >= failed_attempts++) && "Unable to generate unique ports");
    port = (RandomUint32() % 48126) + 1025;
    unique = (already_used_ports.insert(port)).second;
  } while (!unique);
  return port;
}

NodeInfo MakeNode() {
  NodeInfo node;
  node.node_id = NodeId(RandomString(64));
  asymm::Keys keys;
  asymm::GenerateKeyPair(&keys);
  node.public_key = keys.public_key;
  node.endpoint.address(GetLocalIp());
  node.endpoint.port(GetRandomPort());
  return node;
}

ip::address GetLocalIp(ip::udp::endpoint peer_endpoint) {
  asio::io_service io_service;
  ip::udp::socket socket(io_service);
  try {
    socket.connect(peer_endpoint);
    if (socket.local_endpoint().address().is_unspecified() ||
        socket.local_endpoint().address().is_loopback())
      return ip::address();
    return socket.local_endpoint().address();
  }
  catch(const std::exception &e) {
    LOG(kError) << "Failed trying to connect to " << peer_endpoint << " - "
                << e.what();
    return ip::address();
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
