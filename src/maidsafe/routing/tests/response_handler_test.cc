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

#include <algorithm>
#include <memory>
#include <vector>

#include "maidsafe/common/log.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/response_handler.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/service.h"
#include "maidsafe/routing/tests/test_utils.h"


namespace maidsafe {

namespace routing {

namespace test {


TEST(ResponseHandlerTest, BEH_ConnectAttempts) {
  Fob fob(MakeFob());
  RoutingTable routing_table(fob, false);
  NonRoutingTable non_routing_table(fob);
  AsioService asio_service(0);
  Timer timer(asio_service);
  NetworkUtils network(routing_table, non_routing_table, timer);
  ResponseHandler response_handler(routing_table, non_routing_table, network);

  NodeId node_id(NodeId::kRandomId);
  for (auto i(0); i != 100; ++i)
    response_handler.AddPendingConnect(node_id);
  EXPECT_EQ(1, response_handler.pending_connect_rpc_.size());
  for (auto i(0); i != 100; ++i)
    response_handler.ClearPendingConnect(node_id);
  EXPECT_EQ(0, response_handler.pending_connect_rpc_.size());

  std::vector<NodeId> nodes;
  for (auto i(0); i != 100; ++i)
    nodes.push_back(NodeId(NodeId::kRandomId));
  auto itr = unique(nodes.begin(), nodes.end());
  nodes.resize(itr - nodes.begin());

  for (auto i(0); i != 100; ++i) {
    std::random_shuffle(nodes.begin(), nodes.end());
    for (auto i : nodes)
      response_handler.AddPendingConnect(i);
  }
  EXPECT_EQ(nodes.size(), response_handler.pending_connect_rpc_.size());

  for (auto i : nodes)
    response_handler.ClearPendingConnect(i);
  EXPECT_EQ(nodes.size(), response_handler.pending_connect_rpc_.size());

  for (auto i(0); i != 100; ++i) {
    std::random_shuffle(nodes.begin(), nodes.end());
    for (auto i : nodes)
      response_handler.ClearPendingConnect(i);
  }
  EXPECT_EQ(0, response_handler.pending_connect_rpc_.size());

  for (auto i(0); i != 100; ++i) {
    std::random_shuffle(nodes.begin(), nodes.end());
    for (auto i : nodes)
      response_handler.AddPendingConnect(i);
  }
  EXPECT_EQ(nodes.size(), response_handler.pending_connect_rpc_.size());
  Parameters::connect_rpc_prune_timeout = boost::posix_time::seconds(1);
  Sleep(boost::posix_time::seconds(1));
  response_handler.PrunePendingConnect();
  EXPECT_EQ(0, response_handler.pending_connect_rpc_.size());
  // After pruning
  for (auto i(0); i != 100; ++i) {
    std::random_shuffle(nodes.begin(), nodes.end());
    for (auto i : nodes)
      response_handler.AddPendingConnect(i);
  }
  EXPECT_EQ(nodes.size(), response_handler.pending_connect_rpc_.size());
  Parameters::connect_rpc_prune_timeout = boost::posix_time::seconds(10);
}


}  // namespace test

}  // namespace routing

}  // namespace maidsafe
