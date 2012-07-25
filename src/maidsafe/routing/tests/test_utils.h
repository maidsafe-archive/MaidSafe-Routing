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

#ifndef MAIDSAFE_ROUTING_TESTS_TEST_UTILS_H_
#define MAIDSAFE_ROUTING_TESTS_TEST_UTILS_H_

#include "maidsafe/routing/routing_table.h"

#include "boost/asio/ip/address.hpp"
#include "boost/asio/ip/udp.hpp"

namespace maidsafe {

namespace routing {

namespace test {

struct NodeInfoAndPrivateKey {
  NodeInfoAndPrivateKey()
      : node_info(),
        private_key() {}
  NodeInfo node_info;
  asymm::PrivateKey private_key;
};

NodeInfoAndPrivateKey MakeNodeInfoAndKeys();

asymm::Keys MakeKeys();

asymm::Keys GetKeys(const NodeInfoAndPrivateKey &node);

uint16_t GetRandomPort();

NodeInfo MakeNode();

// TODO(Prakash): Copying from rudp utils for test purpose. need to expose it if needed.
// Makes a udp socket connection to peer_endpoint.  Note, no data is sent, so
// no information about the validity or availability of the peer is deduced.
// If the retrieved local endpoint is unspecified or is the loopback address,
// the function returns a default-constructed (invalid) address.
boost::asio::ip::address GetLocalIp(
    boost::asio::ip::udp::endpoint peer_endpoint =
        Endpoint(boost::asio::ip::address_v4::from_string("8.8.8.8"), 0));

NodeId GenerateUniqueRandomId(const NodeId &holder, const uint16_t &pos);

int NetworkStatus(const bool &client, const int &status);

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TESTS_TEST_UTILS_H_
