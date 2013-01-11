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

#ifndef MAIDSAFE_ROUTING_TESTS_MOCK_ROUTING_TABLE_H_
#define MAIDSAFE_ROUTING_TESTS_MOCK_ROUTING_TABLE_H_

#include <string>

#include "gmock/gmock.h"

#include "maidsafe/routing/routing_table.h"


namespace maidsafe {

namespace routing {

namespace test {

class MockRoutingTable : public RoutingTable {
 public:
  MockRoutingTable(bool client_mode, const NodeId& node_id, const asymm::Keys& keys,
                   NetworkStatistics& network_statistics);
  virtual ~MockRoutingTable();

  MOCK_METHOD2(IsNodeIdInGroupRange, bool(const NodeId& node_id, bool& is_group_leader));

 private:
  MockRoutingTable &operator=(const MockRoutingTable&);
  MockRoutingTable(const MockRoutingTable&);
};

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TESTS_MOCK_ROUTING_TABLE_H_

