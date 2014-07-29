/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#include <thread>
#include <vector>

#include "maidsafe/passport/passport.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/tests/routing_network.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace args = std::placeholders;

namespace maidsafe {

namespace routing {

namespace test {

class FindNodeNetwork : public GenericNetwork, public testing::Test {
 public:
  FindNodeNetwork(void) : GenericNetwork() {}

  virtual void SetUp() override { GenericNetwork::SetUp(); }

  virtual void TearDown() override { Sleep(std::chrono::microseconds(100)); }

 protected:
  testing::AssertionResult Find(std::shared_ptr<GenericNode> source, const NodeId& node_id) {
    protobuf::Message find_node_rpc(rpcs::FindNodes(node_id, source->node_id(),
                                    Parameters::closest_nodes_size));
    source->SendToClosestNode(find_node_rpc);
    return testing::AssertionSuccess();
  }

  testing::AssertionResult DropNode(const NodeId& node_id) {
    for (const auto& node : nodes_)
      node->DropNode(node_id);
    return testing::AssertionSuccess();
  }

  void PrintAllRoutingTables() {
    for (size_t index = 0; index < nodes_.size(); ++index) {
      LOG(kInfo) << "Routing table of node # " << index;
      nodes_[index]->PrintRoutingTable();
    }
  }

  size_t FindClosestIndex(const NodeId& node_id) {
    size_t source(0);
    for (size_t index(1); index < ClientIndex(); ++index)
      if (NodeId::CloserToTarget(nodes_[index]->node_id(), nodes_[source]->node_id(), node_id))
        source = index;
    return source;
  }
};

TEST_F(FindNodeNetwork, FUNC_FindExistingNode) {
  SetUpNetwork(kServerSize);
  for (const auto& source : nodes_) {
    EXPECT_TRUE(Find(source, source->node_id()));
    Sleep(std::chrono::seconds(1));
  }
  EXPECT_TRUE(ValidateRoutingTables());
}

TEST_F(FindNodeNetwork, FUNC_FindNonExistingNode) {
  SetUpNetwork(kServerSize);
  size_t source(RandomVaultIndex());
  NodeId node_id(GenerateUniqueRandomId(nodes_[source]->node_id(), 6));
  EXPECT_TRUE(Find(nodes_[source], node_id));
  Sleep(std::chrono::seconds(1));
  EXPECT_FALSE(nodes_[source]->RoutingTableHasNode(node_id));
}

TEST_F(FindNodeNetwork, FUNC_FindNodeAfterDrop) {
  SetUpNetwork(kServerSize);
  auto pmid(passport::CreatePmidAndSigner().first);
  NodeId node_id(pmid.name());
  size_t source(FindClosestIndex(node_id));
  EXPECT_FALSE(nodes_[source]->RoutingTableHasNode(node_id));
  AddNode(pmid);
  EXPECT_TRUE(nodes_[source]->RoutingTableHasNode(node_id));
  EXPECT_TRUE(nodes_[source]->DropNode(node_id));
  Sleep(Parameters::recovery_time_lag);
  EXPECT_TRUE(nodes_[source]->RoutingTableHasNode(node_id));
}

TEST_F(FindNodeNetwork, FUNC_VaultFindVaultNode) {
  SetUpNetwork(kServerSize, kClientSize);
  size_t source(0), dest(ClientIndex());
  auto pmid(passport::CreatePmidAndSigner().first);
  NodeId node_id(pmid.name());
  source = FindClosestIndex(node_id);
  AddNode(pmid);
  EXPECT_FALSE(nodes_.at(dest)->IsClient());
  EXPECT_TRUE(nodes_[source]->RoutingTableHasNode(nodes_[dest]->node_id()));
  EXPECT_TRUE(nodes_[source]->DropNode(nodes_[dest]->node_id()));
  LOG(kVerbose) << "before find " << HexSubstr(nodes_[dest]->node_id().string());
  Sleep(Parameters::recovery_time_lag);
  LOG(kVerbose) << "after find " << HexSubstr(nodes_[dest]->node_id().string());
  EXPECT_TRUE(nodes_[source]->RoutingTableHasNode(nodes_[dest]->node_id()));
}

TEST_F(FindNodeNetwork, FUNC_VaultFindClientNode) {
  SetUpNetwork(kServerSize, kClientSize);
  size_t source(0), dest(nodes_.size());
  auto maid(passport::CreateMaidAndSigner().first);
  NodeId node_id(maid.name());
  source = FindClosestIndex(node_id);
  AddNode(maid);
  // Add one client node
  EXPECT_TRUE(nodes_.at(dest)->IsClient());
  EXPECT_TRUE(nodes_[dest]->RoutingTableHasNode(nodes_[source]->node_id()));
  EXPECT_FALSE(nodes_[source]->RoutingTableHasNode(nodes_[dest]->node_id()));
  EXPECT_TRUE(nodes_[source]->ClientRoutingTableHasNode(nodes_[dest]->node_id()));
  // clear up
  EXPECT_TRUE(nodes_[dest]->DropNode(nodes_[source]->node_id()));
  Sleep(Parameters::recovery_time_lag);
  EXPECT_TRUE(nodes_[dest]->RoutingTableHasNode(nodes_[source]->node_id()))
              << nodes_[dest]->node_id() << " does not have " << nodes_[source]->node_id();
  EXPECT_TRUE(nodes_[source]->ClientRoutingTableHasNode(nodes_[dest]->node_id()))
              << nodes_[source]->node_id() << " client table misses " << nodes_[dest]->node_id();
}

// The test is commented/disabled due to difficulties in creating close Pmid and Maid nodes.
TEST_F(FindNodeNetwork, DISABLED_FUNC_ClientFindVaultNode) {
//  SetUpNetwork(kServerSize, kClientSize);
//  size_t source(RandomVaultIndex());

//  // Add one client node
//  AddNode(true, GenerateUniqueRandomId(nodes_[source]->node_id(), 8));
//  // Add one vault node
//  AddNode(false, GenerateUniqueRandomId(nodes_[source]->node_id(), 24));

//  size_t client(nodes_.size() - 1);
//  size_t vault(ClientIndex() - 1);
//  EXPECT_TRUE(nodes_.at(client)->IsClient());
//  EXPECT_FALSE(nodes_.at(vault)->IsClient());

//  Sleep(std::chrono::seconds(1));

//  EXPECT_TRUE(nodes_[client]->RoutingTableHasNode(nodes_[source]->node_id()));
//  EXPECT_FALSE(nodes_[source]->RoutingTableHasNode(nodes_[client]->node_id()));
//  EXPECT_TRUE(nodes_[source]->ClientRoutingTableHasNode(nodes_[client]->node_id()));

//  EXPECT_TRUE(nodes_[client]->RoutingTableHasNode(nodes_[vault]->node_id()));
//  EXPECT_FALSE(nodes_[vault]->RoutingTableHasNode(nodes_[client]->node_id()));
//  EXPECT_TRUE(nodes_[vault]->ClientRoutingTableHasNode(nodes_[client]->node_id()));

//  // trying to find
//  EXPECT_TRUE(Find(nodes_[vault], nodes_[vault]->node_id()));
//  Sleep(std::chrono::seconds(1));
//  EXPECT_FALSE(nodes_[vault]->RoutingTableHasNode(nodes_[client]->node_id()));
//  EXPECT_TRUE(nodes_[vault]->ClientRoutingTableHasNode(nodes_[client]->node_id()));
}

// The test is commented/disabled due to difficulties in creating two close Maid nodes.
TEST_F(FindNodeNetwork, DISABLED_FUNC_ClientFindClientNode) {
/*  SetUpNetwork(kServerSize, kClientSize);
  size_t source(RandomVaultIndex()), client1(nodes_.size()), client2(client1 + 1);

  // Add two client nodes
  AddNode(true, GenerateUniqueRandomId(nodes_[source]->node_id(), 8));
  AddNode(true, GenerateUniqueRandomId(nodes_[source]->node_id(), 12));
  Sleep(std::chrono::seconds(1));
  EXPECT_TRUE(nodes_.at(client1)->IsClient());
  EXPECT_TRUE(nodes_.at(client2)->IsClient());

  EXPECT_TRUE(nodes_[client1]->RoutingTableHasNode(nodes_[source]->node_id()));
  EXPECT_TRUE(nodes_[client2]->RoutingTableHasNode(nodes_[source]->node_id()));
  EXPECT_FALSE(nodes_[source]->RoutingTableHasNode(nodes_[client1]->node_id()));
  EXPECT_TRUE(nodes_[source]->ClientRoutingTableHasNode(nodes_[client1]->node_id()));
  EXPECT_FALSE(nodes_[source]->RoutingTableHasNode(nodes_[client2]->node_id()));
  EXPECT_TRUE(nodes_[source]->ClientRoutingTableHasNode(nodes_[client2]->node_id()));

  EXPECT_FALSE(nodes_[client1]->RoutingTableHasNode(nodes_[client2]->node_id()));
  EXPECT_FALSE(nodes_[client1]->ClientRoutingTableHasNode(nodes_[client2]->node_id()));
  EXPECT_FALSE(nodes_[client2]->RoutingTableHasNode(nodes_[client1]->node_id()));
  EXPECT_FALSE(nodes_[client2]->ClientRoutingTableHasNode(nodes_[client1]->node_id()));

  // trying to find
  EXPECT_TRUE(Find(nodes_[client1], nodes_[client1]->node_id()));
  Sleep(std::chrono::seconds(5));
  EXPECT_FALSE(nodes_[client1]->RoutingTableHasNode(nodes_[client2]->node_id()));
  EXPECT_FALSE(nodes_[client1]->ClientRoutingTableHasNode(nodes_[client2]->node_id()));
  */
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
