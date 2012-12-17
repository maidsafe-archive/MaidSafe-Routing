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

#include <cstdint>
#include <vector>

#include "boost/asio/ip/address.hpp"
#include "boost/asio/ip/udp.hpp"

#include "maidsafe/common/rsa.h"

#include "maidsafe/private/utils/fob.h"

#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/routing_table.h"


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

NodeInfoAndPrivateKey MakeNodeInfoAndKeysWithFob(Fob fob);

Fob MakeFob();

Fob GetFob(const NodeInfoAndPrivateKey& node);

NodeInfo MakeNode();

NodeId GenerateUniqueRandomId(const NodeId& holder, const uint16_t& pos);
NodeId GenerateUniqueRandomId(const uint16_t& pos);

int NetworkStatus(const bool& client, const int& status);

void SortFromTarget(const NodeId& target, std::vector<NodeInfo>& nodes);

void SortIdsFromTarget(const NodeId& target, std::vector<NodeInfo>& nodes);

void SortNodeInfosFromTarget(const NodeId& target, std::vector<NodeInfo>& nodes);

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TESTS_TEST_UTILS_H_
