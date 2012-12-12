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

#include <thread>
#include <vector>

#include "boost/thread/future.hpp"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/tests/routing_network.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace args = std::placeholders;

namespace maidsafe {

namespace routing {

namespace test {

class FindNodeNetwork : public testing::Test  {
 public:
  FindNodeNetwork(void) : env_(NodesEnvironment::g_environment()) {}

  void SetUp() {
    EXPECT_TRUE(env_->RestoreComposition());
    EXPECT_TRUE(env_->WaitForHealthToStabilise());
  }

  void TearDown() {
    EXPECT_TRUE(env_->RestoreComposition());
  }

 protected:
  testing::AssertionResult Find(std::shared_ptr<GenericNode> source, const NodeId& node_id) {
    protobuf::Message find_node_rpc(rpcs::FindNodes(node_id, source->node_id(), 8));
    source->SendToClosestNode(find_node_rpc);
    return testing::AssertionSuccess();
  }

  testing::AssertionResult DropNode(const NodeId& node_id) {
    for (auto node : env_->nodes_)
      node->DropNode(node_id);
    return testing::AssertionSuccess();
  }

  void PrintAllRoutingTables() {
    for (size_t index = 0; index < env_->nodes_.size(); ++index) {
      LOG(kInfo) << "Routing table of node # " << index;
      env_->nodes_[index]->PrintRoutingTable();
    }
  }

  std::shared_ptr<GenericNetwork> env_;
};

TEST_F(FindNodeNetwork, FUNC_FindExistingNode) {
  for (auto source : env_->nodes_) {
    EXPECT_TRUE(Find(source, source->node_id()));
    Sleep(boost::posix_time::seconds(1));
  }
  EXPECT_TRUE(env_->ValidateRoutingTables());
}

TEST_F(FindNodeNetwork, FUNC_FindNonExistingNode) {
  uint32_t source(RandomUint32() % (kServerSize - 2) + 2);
  NodeId node_id(GenerateUniqueRandomId(env_->nodes_[source]->node_id(), 6));
  EXPECT_TRUE(Find(env_->nodes_[source], node_id));
  Sleep(boost::posix_time::seconds(1));
  EXPECT_FALSE(env_->nodes_[source]->RoutingTableHasNode(node_id));
}

TEST_F(FindNodeNetwork, FUNC_FindNodeAfterDrop) {
  uint32_t source(RandomUint32() % (kServerSize - 2) + 2);
  NodeId node_id(GenerateUniqueRandomId(env_->nodes_[source]->node_id(), 6));
  EXPECT_FALSE(env_->nodes_[source]->RoutingTableHasNode(node_id));
  env_->AddNode(false, node_id);
  Sleep(boost::posix_time::seconds(1));
  EXPECT_TRUE(env_->nodes_[source]->RoutingTableHasNode(node_id));
  EXPECT_TRUE(env_->nodes_[source]->DropNode(node_id));
  Sleep(Parameters::recovery_time_lag + Parameters::find_node_interval +
          boost::posix_time::seconds(5));
  EXPECT_TRUE(env_->nodes_[source]->RoutingTableHasNode(node_id));
}

TEST_F(FindNodeNetwork, FUNC_VaultFindVaultNode) {
  uint32_t source(RandomUint32() % (kServerSize - 2) + 2),
           dest(static_cast<uint32_t>(env_->ClientIndex()));

  env_->AddNode(false, GenerateUniqueRandomId(env_->nodes_[source]->node_id(), 20));
  EXPECT_TRUE(env_->nodes_[source]->RoutingTableHasNode(env_->nodes_[dest]->node_id()));

  EXPECT_TRUE(env_->nodes_[source]->DropNode(env_->nodes_[dest]->node_id()));

  Sleep(Parameters::recovery_time_lag + Parameters::find_node_interval +
          boost::posix_time::seconds(5));

  LOG(kVerbose) << "after find " << HexSubstr(env_->nodes_[dest]->node_id().string());
  EXPECT_TRUE(env_->nodes_[source]->RoutingTableHasNode(env_->nodes_[dest]->node_id()));
}

TEST_F(FindNodeNetwork, FUNC_VaultFindClientNode) {
  // Create a bootstrap network
  uint32_t source(RandomUint32() % (kServerSize - 2) + 2),
           dest(static_cast<uint32_t>(env_->nodes_.size()));

  // Add one client node
  env_->AddNode(true, GenerateUniqueRandomId(env_->nodes_[source]->node_id(), 20));

  EXPECT_TRUE(env_->nodes_[dest]->RoutingTableHasNode(env_->nodes_[source]->node_id()));
  EXPECT_FALSE(env_->nodes_[source]->RoutingTableHasNode(env_->nodes_[dest]->node_id()));
  EXPECT_TRUE(env_->nodes_[source]->NonRoutingTableHasNode(env_->nodes_[dest]->node_id()));

  // clear up
  EXPECT_TRUE(env_->nodes_[dest]->DropNode(env_->nodes_[source]->node_id()));
  Sleep(Parameters::recovery_time_lag + Parameters::find_node_interval +
          boost::posix_time::seconds(5));
  EXPECT_TRUE(env_->nodes_[dest]->RoutingTableHasNode(env_->nodes_[source]->node_id()));
  EXPECT_TRUE(env_->nodes_[source]->NonRoutingTableHasNode(env_->nodes_[dest]->node_id()));
}

TEST_F(FindNodeNetwork, FUNC_ClientFindVaultNode) {
  // Create a bootstrap network
  uint32_t source(RandomUint32() % (kServerSize - 2) + 2);

  // Add one client node
  env_->AddNode(true, GenerateUniqueRandomId(env_->nodes_[source]->node_id(), 8));
  // Add one vault node
  env_->AddNode(false, GenerateUniqueRandomId(env_->nodes_[source]->node_id(), 24));

  size_t client(env_->nodes_.size() - 1);
  uint32_t vault(env_->ClientIndex());

  Sleep(boost::posix_time::seconds(1));

  EXPECT_TRUE(env_->nodes_[client]->RoutingTableHasNode(env_->nodes_[source]->node_id()));
  EXPECT_FALSE(env_->nodes_[source]->RoutingTableHasNode(env_->nodes_[client]->node_id()));
  EXPECT_TRUE(env_->nodes_[source]->NonRoutingTableHasNode(env_->nodes_[client]->node_id()));

  EXPECT_FALSE(env_->nodes_[client]->RoutingTableHasNode(env_->nodes_[vault]->node_id()));
  EXPECT_FALSE(env_->nodes_[vault]->RoutingTableHasNode(env_->nodes_[client]->node_id()));
  EXPECT_FALSE(env_->nodes_[vault]->NonRoutingTableHasNode(env_->nodes_[client]->node_id()));

  // trying to find
  EXPECT_TRUE(Find(env_->nodes_[vault], env_->nodes_[vault]->node_id()));
  Sleep(boost::posix_time::seconds(1));
  EXPECT_FALSE(env_->nodes_[vault]->RoutingTableHasNode(env_->nodes_[client]->node_id()));
  EXPECT_FALSE(env_->nodes_[vault]->NonRoutingTableHasNode(env_->nodes_[client]->node_id()));
}

TEST_F(FindNodeNetwork, FUNC_ClientFindClientNode) {
  // Create a bootstrap network
  uint32_t source(RandomUint32() % (kServerSize - 2) + 2),
           client1(static_cast<uint32_t>(env_->nodes_.size())),
           client2(client1 + 1);

  // Add two client nodes
  env_->AddNode(true, GenerateUniqueRandomId(env_->nodes_[source]->node_id(), 8));
  env_->AddNode(true, GenerateUniqueRandomId(env_->nodes_[source]->node_id(), 12));
  Sleep(boost::posix_time::seconds(1));

  EXPECT_TRUE(env_->nodes_[client1]->RoutingTableHasNode(env_->nodes_[source]->node_id()));
  EXPECT_TRUE(env_->nodes_[client2]->RoutingTableHasNode(env_->nodes_[source]->node_id()));
  EXPECT_FALSE(env_->nodes_[source]->RoutingTableHasNode(env_->nodes_[client1]->node_id()));
  EXPECT_TRUE(env_->nodes_[source]->NonRoutingTableHasNode(env_->nodes_[client1]->node_id()));
  EXPECT_FALSE(env_->nodes_[source]->RoutingTableHasNode(env_->nodes_[client2]->node_id()));
  EXPECT_TRUE(env_->nodes_[source]->NonRoutingTableHasNode(env_->nodes_[client2]->node_id()));

  EXPECT_FALSE(env_->nodes_[client1]->RoutingTableHasNode(env_->nodes_[client2]->node_id()));
  EXPECT_FALSE(env_->nodes_[client1]->NonRoutingTableHasNode(env_->nodes_[client2]->node_id()));
  EXPECT_FALSE(env_->nodes_[client2]->RoutingTableHasNode(env_->nodes_[client1]->node_id()));
  EXPECT_FALSE(env_->nodes_[client2]->NonRoutingTableHasNode(env_->nodes_[client1]->node_id()));

  // trying to find
  EXPECT_TRUE(Find(env_->nodes_[client1], env_->nodes_[client1]->node_id()));
  Sleep(boost::posix_time::seconds(5));
  EXPECT_FALSE(env_->nodes_[client1]->RoutingTableHasNode(env_->nodes_[client2]->node_id()));
  EXPECT_FALSE(env_->nodes_[client1]->NonRoutingTableHasNode(env_->nodes_[client2]->node_id()));
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
