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
#include "maidsafe/routing/routing_private.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/tests/routing_network.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace args = std::placeholders;

namespace maidsafe {

namespace routing {

namespace test {

class FindNodeNetwork : public GenericNetwork {
 public:
  FindNodeNetwork(void) : GenericNetwork() {}

 protected:
  testing::AssertionResult Find(std::shared_ptr<GenericNode> source, const NodeId& node_id) {
    protobuf::Message find_node_rpc(rpcs::FindNodes(node_id, source->node_id(), 8));
    source->SendToClosestNode(find_node_rpc);
    return testing::AssertionSuccess();
  }

  testing::AssertionResult DropNode(const NodeId& node_id) {
    for (auto node : this->nodes_)
      node->DropNode(node_id);
    return testing::AssertionSuccess();
  }

  void PrintAllRoutingTables() {
    for (size_t index = 0; index < this->nodes_.size(); ++index) {
      LOG(kInfo) << "Routing table of node # " << index;
      this->nodes_[index]->PrintRoutingTable();
    }
  }
};

TEST_F(FindNodeNetwork, FUNC_FindExistingNode) {
  this->SetUpNetwork(kNetworkSize);
  for (auto source : this->nodes_) {
    EXPECT_TRUE(this->Find(source, source->node_id()));
    Sleep(boost::posix_time::seconds(1));
  }
  EXPECT_TRUE(this->ValidateRoutingTables());
}

TEST_F(FindNodeNetwork, FUNC_FindExistingNodeInHybridNetwork) {
  this->SetUpNetwork(kServerSize, kClientSize);
  for (auto source : this->nodes_) {
    EXPECT_TRUE(this->Find(source, source->node_id()));
    Sleep(boost::posix_time::seconds(1));
  }
  EXPECT_TRUE(this->ValidateRoutingTables());
}


TEST_F(FindNodeNetwork, FUNC_FindNonExistingNode) {
  this->SetUpNetwork(kServerSize, kClientSize);
  uint32_t source(RandomUint32() % (this->nodes_.size() - 2) + 2);
  NodeId node_id(GenerateUniqueRandomId(this->nodes_[source]->node_id(), 6));
  EXPECT_TRUE(this->Find(this->nodes_[source], node_id));
  Sleep(boost::posix_time::seconds(1));
  EXPECT_FALSE(this->nodes_[source]->RoutingTableHasNode(node_id));
}

TEST_F(FindNodeNetwork, FUNC_FindNodeAfterDrop) {
  this->SetUpNetwork(kNetworkSize);
  uint32_t source(RandomUint32() % (this->nodes_.size() - 2) + 2);
  NodeId node_id(GenerateUniqueRandomId(this->nodes_[source]->node_id(), 6));
  EXPECT_FALSE(this->nodes_[source]->RoutingTableHasNode(node_id));
  this->AddNode(false, node_id);
  Sleep(boost::posix_time::seconds(1));
  EXPECT_TRUE(this->nodes_[source]->RoutingTableHasNode(node_id));
  EXPECT_TRUE(this->nodes_[source]->DropNode(node_id));
  Sleep(boost::posix_time::seconds(20));
  EXPECT_TRUE(this->nodes_[source]->RoutingTableHasNode(node_id));
}

TEST_F(FindNodeNetwork, FUNC_VaultFindVaultNode) {
  this->SetUpNetwork(kNetworkSize);
  uint32_t source(
      RandomUint32() % (static_cast<uint32_t>(this->nodes_.size()) - 2) + 2),
      dest(static_cast<uint32_t>(this->nodes_.size()));

  this->AddNode(false, GenerateUniqueRandomId(this->nodes_[source]->node_id(), 20));
  EXPECT_TRUE(this->nodes_[source]->RoutingTableHasNode(this->nodes_[dest]->node_id()));

  EXPECT_TRUE(this->nodes_[source]->DropNode(this->nodes_[dest]->node_id()));

  Sleep(boost::posix_time::seconds(15));

  LOG(kVerbose) << "after find " << HexSubstr(this->nodes_[dest]->node_id().string());
  EXPECT_TRUE(this->nodes_[source]->RoutingTableHasNode(this->nodes_[dest]->node_id()));
}

TEST_F(FindNodeNetwork, FUNC_VaultFindClientNode) {
  // Create a bootstrap network
  this->SetUpNetwork(kNetworkSize);
  uint32_t source(
      RandomUint32() % (static_cast<uint32_t>(this->nodes_.size()) - 2) + 2),
      dest(static_cast<uint32_t>(this->nodes_.size()));

  // Add one client node
  this->AddNode(true, GenerateUniqueRandomId(this->nodes_[source]->node_id(), 20));

  EXPECT_TRUE(this->nodes_[dest]->RoutingTableHasNode(this->nodes_[source]->node_id()));
  EXPECT_FALSE(this->nodes_[source]->RoutingTableHasNode(this->nodes_[dest]->node_id()));
  EXPECT_TRUE(this->nodes_[source]->NonRoutingTableHasNode(this->nodes_[dest]->node_id()));

  // clear up
  EXPECT_TRUE(this->nodes_[dest]->DropNode(this->nodes_[source]->node_id()));
  Sleep(boost::posix_time::seconds(1));
  EXPECT_TRUE(this->nodes_[dest]->RoutingTableHasNode(this->nodes_[source]->node_id()));
  EXPECT_TRUE(this->nodes_[source]->NonRoutingTableHasNode(this->nodes_[dest]->node_id()));
}

TEST_F(FindNodeNetwork, FUNC_ClientFindVaultNode) {
  // Create a bootstrap network
  this->SetUpNetwork(kNetworkSize);
  uint32_t source(
      RandomUint32() % (static_cast<uint32_t>(this->nodes_.size()) - 2) + 2),
      client(static_cast<uint32_t>(this->nodes_.size())),
      vault(client + 1);

  // Add one client node
  this->AddNode(true, GenerateUniqueRandomId(this->nodes_[source]->node_id(), 8));
  // Add one vault node
  this->AddNode(false, GenerateUniqueRandomId(this->nodes_[source]->node_id(), 24));

  Sleep(boost::posix_time::seconds(1));

  EXPECT_TRUE(this->nodes_[client]->RoutingTableHasNode(this->nodes_[source]->node_id()));
  EXPECT_FALSE(this->nodes_[source]->RoutingTableHasNode(this->nodes_[client]->node_id()));
  EXPECT_TRUE(this->nodes_[source]->NonRoutingTableHasNode(this->nodes_[client]->node_id()));

  EXPECT_FALSE(this->nodes_[client]->RoutingTableHasNode(this->nodes_[vault]->node_id()));
  EXPECT_FALSE(this->nodes_[vault]->RoutingTableHasNode(this->nodes_[client]->node_id()));
  EXPECT_FALSE(this->nodes_[vault]->NonRoutingTableHasNode(this->nodes_[client]->node_id()));

  // trying to find
  EXPECT_TRUE(this->Find(this->nodes_[vault], this->nodes_[vault]->node_id()));
  Sleep(boost::posix_time::seconds(1));
  EXPECT_FALSE(this->nodes_[vault]->RoutingTableHasNode(this->nodes_[client]->node_id()));
  EXPECT_FALSE(this->nodes_[vault]->NonRoutingTableHasNode(this->nodes_[client]->node_id()));
}

TEST_F(FindNodeNetwork, FUNC_ClientFindClientNode) {
  // Create a bootstrap network
  this->SetUpNetwork(kNetworkSize);
  uint32_t source(
      RandomUint32() % (static_cast<uint32_t>(this->nodes_.size()) - 2) + 2),
      client1(static_cast<uint32_t>(this->nodes_.size())),
      client2(client1 + 1);

  // Add two client nodes
  this->AddNode(true, GenerateUniqueRandomId(this->nodes_[source]->node_id(), 8));
  this->AddNode(true, GenerateUniqueRandomId(this->nodes_[source]->node_id(), 12));
  Sleep(boost::posix_time::seconds(1));

  EXPECT_TRUE(this->nodes_[client1]->RoutingTableHasNode(this->nodes_[source]->node_id()));
  EXPECT_TRUE(this->nodes_[client2]->RoutingTableHasNode(this->nodes_[source]->node_id()));
  EXPECT_FALSE(this->nodes_[source]->RoutingTableHasNode(this->nodes_[client1]->node_id()));
  EXPECT_TRUE(this->nodes_[source]->NonRoutingTableHasNode(this->nodes_[client1]->node_id()));
  EXPECT_FALSE(this->nodes_[source]->RoutingTableHasNode(this->nodes_[client2]->node_id()));
  EXPECT_TRUE(this->nodes_[source]->NonRoutingTableHasNode(this->nodes_[client2]->node_id()));

  EXPECT_FALSE(this->nodes_[client1]->RoutingTableHasNode(this->nodes_[client2]->node_id()));
  EXPECT_FALSE(this->nodes_[client1]->NonRoutingTableHasNode(this->nodes_[client2]->node_id()));
  EXPECT_FALSE(this->nodes_[client2]->RoutingTableHasNode(this->nodes_[client1]->node_id()));
  EXPECT_FALSE(this->nodes_[client2]->NonRoutingTableHasNode(this->nodes_[client1]->node_id()));

  // trying to find
  EXPECT_TRUE(this->Find(this->nodes_[client1], this->nodes_[client1]->node_id()));
  Sleep(boost::posix_time::seconds(5));
  EXPECT_FALSE(this->nodes_[client1]->RoutingTableHasNode(this->nodes_[client2]->node_id()));
  EXPECT_FALSE(this->nodes_[client1]->NonRoutingTableHasNode(this->nodes_[client2]->node_id()));
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
