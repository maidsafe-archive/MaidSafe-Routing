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
#include "maidsafe/routing/network.h"
#include "maidsafe/routing/routing.pb.h"
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
  testing::AssertionResult Find(std::shared_ptr<GenericNode> source, const Address& Address) {
    protobuf::Message find_node_rpc(
        rpcs::FindNodes(Address, source->node_id(), Parameters::closest_nodes_size));
    source->SendToClosestNode(find_node_rpc);
    return testing::AssertionSuccess();
  }

  testing::AssertionResult DropNode(const Address& Address) {
    for (const auto& node : nodes_)
      node->DropNode(Address);
    return testing::AssertionSuccess();
  }

  void PrintAllRoutingTables() {
    for (size_t index = 0; index < nodes_.size(); ++index) {
     
      nodes_[index]->PrintRoutingTable();
    }
  }

  size_t FindClosestIndex(const Address& Address) {
    size_t source(0);
    for (size_t index(1); index < ClientIndex(); ++index)
      if (Address::CloserToTarget(nodes_[index]->Address(), nodes_[source]->node_id(), node_id))
        source = index;
    return source;
  }
};

TEST_F(FindNodeNetwork, FUNC_FindExistingNode) {
  SetUpNetwork(kServerSize);
  for (const auto& source : nodes_) {
    EXPECT_TRUE(Find(source, source->Address()));
    Sleep(std::chrono::seconds(1));
  }
  EXPECT_TRUE(ValidateRoutingTables());
}

TEST_F(FindNodeNetwork, FUNC_FindNonExistingNode) {
  SetUpNetwork(kServerSize);
  size_t source(RandomVaultIndex());
  Address Address(GenerateUniqueRandomId(nodes_[source]->node_id(), 6));
  EXPECT_TRUE(Find(nodes_[source], Address));
  Sleep(std::chrono::seconds(1));
  EXPECT_FALSE(nodes_[source]->RoutingTableHasNode(Address));
}

TEST_F(FindNodeNetwork, FUNC_FindNodeAfterDrop) {
  SetUpNetwork(kServerSize);
  auto pmid(passport::CreatePmidAndSigner().first);
  Address Address(pmid.name());
  size_t source(FindClosestIndex(Address));
  EXPECT_FALSE(nodes_[source]->RoutingTableHasNode(Address));
  AddNode(pmid);
  EXPECT_TRUE(nodes_[source]->RoutingTableHasNode(Address));
  EXPECT_TRUE(nodes_[source]->DropNode(Address));
  Sleep(Parameters::recovery_time_lag);
  EXPECT_TRUE(nodes_[source]->RoutingTableHasNode(Address));
}

TEST_F(FindNodeNetwork, FUNC_VaultFindVaultNode) {
  SetUpNetwork(kServerSize, kClientSize);
  size_t source(0), dest(ClientIndex());
  auto pmid(passport::CreatePmidAndSigner().first);
  Address Address(pmid.name());
  source = FindClosestIndex(Address);
  AddNode(pmid);
  EXPECT_FALSE(nodes_.at(dest)->IsClient());
  EXPECT_TRUE(nodes_[source]->RoutingTableHasNode(nodes_[dest]->Address()));
  EXPECT_TRUE(nodes_[source]->DropNode(nodes_[dest]->Address()));
 
  Sleep(Parameters::recovery_time_lag);
 
  EXPECT_TRUE(nodes_[source]->RoutingTableHasNode(nodes_[dest]->Address()));
}

TEST_F(FindNodeNetwork, FUNC_VaultFindClientNode) {
  SetUpNetwork(kServerSize, kClientSize);
  size_t source(0), dest(nodes_.size());
  auto maid(passport::CreateMaidAndSigner().first);
  Address Address(maid.name());
  source = FindClosestIndex(Address);
  AddNode(maid);
  // Add one client node
  EXPECT_TRUE(nodes_.at(dest)->IsClient());
  EXPECT_TRUE(nodes_[dest]->RoutingTableHasNode(nodes_[source]->Address()));
  EXPECT_FALSE(nodes_[source]->RoutingTableHasNode(nodes_[dest]->Address()));
  EXPECT_TRUE(nodes_[source]->ClientRoutingTableHasNode(nodes_[dest]->Address()));
  // clear up
  EXPECT_TRUE(nodes_[dest]->DropNode(nodes_[source]->Address()));
  Sleep(Parameters::recovery_time_lag);
  EXPECT_TRUE(nodes_[dest]->RoutingTableHasNode(nodes_[source]->Address()))
      << nodes_[dest]->Address() << " does not have " << nodes_[source]->node_id();
  EXPECT_TRUE(nodes_[source]->ClientRoutingTableHasNode(nodes_[dest]->Address()))
      << nodes_[source]->Address() << " client table misses " << nodes_[dest]->node_id();
}

// The test is commented/disabled due to difficulties in creating close Pmid and Maid nodes.
TEST_F(FindNodeNetwork, DISABLED_FUNC_ClientFindVaultNode) {
  //  SetUpNetwork(kServerSize, kClientSize);
  //  size_t source(RandomVaultIndex());

  //  // Add one client node
  //  AddNode(true, GenerateUniqueRandomId(nodes_[source]->Address(), 8));
  //  // Add one vault node
  //  AddNode(false, GenerateUniqueRandomId(nodes_[source]->Address(), 24));

  //  size_t client(nodes_.size() - 1);
  //  size_t vault(ClientIndex() - 1);
  //  EXPECT_TRUE(nodes_.at(client)->IsClient());
  //  EXPECT_FALSE(nodes_.at(vault)->IsClient());

  //  Sleep(std::chrono::seconds(1));

  //  EXPECT_TRUE(nodes_[client]->RoutingTableHasNode(nodes_[source]->Address()));
  //  EXPECT_FALSE(nodes_[source]->RoutingTableHasNode(nodes_[client]->Address()));
  //  EXPECT_TRUE(nodes_[source]->ClientRoutingTableHasNode(nodes_[client]->Address()));

  //  EXPECT_TRUE(nodes_[client]->RoutingTableHasNode(nodes_[vault]->Address()));
  //  EXPECT_FALSE(nodes_[vault]->RoutingTableHasNode(nodes_[client]->Address()));
  //  EXPECT_TRUE(nodes_[vault]->ClientRoutingTableHasNode(nodes_[client]->Address()));

  //  // trying to find
  //  EXPECT_TRUE(Find(nodes_[vault], nodes_[vault]->Address()));
  //  Sleep(std::chrono::seconds(1));
  //  EXPECT_FALSE(nodes_[vault]->RoutingTableHasNode(nodes_[client]->Address()));
  //  EXPECT_TRUE(nodes_[vault]->ClientRoutingTableHasNode(nodes_[client]->Address()));
}

// The test is commented/disabled due to difficulties in creating two close Maid nodes.
TEST_F(FindNodeNetwork, DISABLED_FUNC_ClientFindClientNode) {
  /*  SetUpNetwork(kServerSize, kClientSize);
    size_t source(RandomVaultIndex()), client1(nodes_.size()), client2(client1 + 1);

    // Add two client nodes
    AddNode(true, GenerateUniqueRandomId(nodes_[source]->Address(), 8));
    AddNode(true, GenerateUniqueRandomId(nodes_[source]->Address(), 12));
    Sleep(std::chrono::seconds(1));
    EXPECT_TRUE(nodes_.at(client1)->IsClient());
    EXPECT_TRUE(nodes_.at(client2)->IsClient());

    EXPECT_TRUE(nodes_[client1]->RoutingTableHasNode(nodes_[source]->Address()));
    EXPECT_TRUE(nodes_[client2]->RoutingTableHasNode(nodes_[source]->Address()));
    EXPECT_FALSE(nodes_[source]->RoutingTableHasNode(nodes_[client1]->Address()));
    EXPECT_TRUE(nodes_[source]->ClientRoutingTableHasNode(nodes_[client1]->Address()));
    EXPECT_FALSE(nodes_[source]->RoutingTableHasNode(nodes_[client2]->Address()));
    EXPECT_TRUE(nodes_[source]->ClientRoutingTableHasNode(nodes_[client2]->Address()));

    EXPECT_FALSE(nodes_[client1]->RoutingTableHasNode(nodes_[client2]->Address()));
    EXPECT_FALSE(nodes_[client1]->ClientRoutingTableHasNode(nodes_[client2]->Address()));
    EXPECT_FALSE(nodes_[client2]->RoutingTableHasNode(nodes_[client1]->Address()));
    EXPECT_FALSE(nodes_[client2]->ClientRoutingTableHasNode(nodes_[client1]->Address()));

    // trying to find
    EXPECT_TRUE(Find(nodes_[client1], nodes_[client1]->Address()));
    Sleep(std::chrono::seconds(5));
    EXPECT_FALSE(nodes_[client1]->RoutingTableHasNode(nodes_[client2]->Address()));
    EXPECT_FALSE(nodes_[client1]->ClientRoutingTableHasNode(nodes_[client2]->Address()));
    */
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
