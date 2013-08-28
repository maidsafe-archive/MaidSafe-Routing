/* Copyright 2012 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

#include <thread>
#include <vector>

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/network_utils.h"
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

  virtual void SetUp() {
    GenericNetwork::SetUp();
  }

  virtual void TearDown() {
    Sleep(std::chrono::microseconds(100));
  }

 protected:
  testing::AssertionResult Find(std::shared_ptr<GenericNode> source, const NodeId& node_id) {
    protobuf::Message find_node_rpc(rpcs::FindNodes(node_id, source->node_id(), 8));
    source->SendToClosestNode(find_node_rpc);
    return testing::AssertionSuccess();
  }

  testing::AssertionResult DropNode(const NodeId& node_id) {
    for (const auto& node : this->nodes_)
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
  this->SetUpNetwork(kServerSize);
  for (const auto& source : this->nodes_) {
    EXPECT_TRUE(Find(source, source->node_id()));
    Sleep(std::chrono::seconds(1));
  }
  EXPECT_TRUE(this->ValidateRoutingTables());
}

TEST_F(FindNodeNetwork, FUNC_FindNonExistingNode) {
  this->SetUpNetwork(kServerSize);
  size_t source(this->RandomVaultIndex());
  NodeId node_id(GenerateUniqueRandomId(this->nodes_[source]->node_id(), 6));
  EXPECT_TRUE(Find(this->nodes_[source], node_id));
  Sleep(std::chrono::seconds(1));
  EXPECT_FALSE(this->nodes_[source]->RoutingTableHasNode(node_id));
}

TEST_F(FindNodeNetwork, FUNC_FindNodeAfterDrop) {
  this->SetUpNetwork(kServerSize);
  size_t source(this->RandomVaultIndex());
  NodeId node_id(GenerateUniqueRandomId(this->nodes_[source]->node_id(), 6));
  EXPECT_FALSE(this->nodes_[source]->RoutingTableHasNode(node_id));
  this->AddNode(false, node_id);
  EXPECT_TRUE(this->nodes_[source]->RoutingTableHasNode(node_id));
  EXPECT_TRUE(this->nodes_[source]->DropNode(node_id));
  Sleep(Parameters::recovery_time_lag + Parameters::find_node_interval + std::chrono::seconds(5));
  EXPECT_TRUE(this->nodes_[source]->RoutingTableHasNode(node_id));
}

TEST_F(FindNodeNetwork, FUNC_VaultFindVaultNode) {
  this->SetUpNetwork(kServerSize, kClientSize);
  size_t source(this->RandomVaultIndex()),
         dest(this->ClientIndex());

  this->AddNode(false, GenerateUniqueRandomId(this->nodes_[source]->node_id(), 20));
  EXPECT_FALSE(this->nodes_.at(dest)->IsClient());

  EXPECT_TRUE(this->nodes_[source]->RoutingTableHasNode(this->nodes_[dest]->node_id()));

  EXPECT_TRUE(this->nodes_[source]->DropNode(this->nodes_[dest]->node_id()));

  Sleep(Parameters::recovery_time_lag + Parameters::find_node_interval + std::chrono::seconds(5));

  LOG(kVerbose) << "after find " << HexSubstr(this->nodes_[dest]->node_id().string());
  EXPECT_TRUE(this->nodes_[source]->RoutingTableHasNode(this->nodes_[dest]->node_id()));
}

TEST_F(FindNodeNetwork, FUNC_VaultFindClientNode) {
  this->SetUpNetwork(kServerSize, kClientSize);
  size_t source(this->RandomVaultIndex()),
         dest(this->nodes_.size());

  // Add one client node
  this->AddNode(true, GenerateUniqueRandomId(this->nodes_[source]->node_id(), 20));
  EXPECT_TRUE(this->nodes_.at(dest)->IsClient());

  EXPECT_TRUE(this->nodes_[dest]->RoutingTableHasNode(this->nodes_[source]->node_id()));
  EXPECT_FALSE(this->nodes_[source]->RoutingTableHasNode(this->nodes_[dest]->node_id()));
  EXPECT_TRUE(this->nodes_[source]->ClientRoutingTableHasNode(this->nodes_[dest]->node_id()));

  // clear up
  EXPECT_TRUE(this->nodes_[dest]->DropNode(this->nodes_[source]->node_id()));
  Sleep(Parameters::recovery_time_lag + Parameters::find_node_interval + std::chrono::seconds(5));
  EXPECT_TRUE(this->nodes_[dest]->RoutingTableHasNode(this->nodes_[source]->node_id()));
  EXPECT_TRUE(this->nodes_[source]->ClientRoutingTableHasNode(this->nodes_[dest]->node_id()));
}

TEST_F(FindNodeNetwork, FUNC_ClientFindVaultNode) {
  this->SetUpNetwork(kServerSize, kClientSize);
  size_t source(this->RandomVaultIndex());

  // Add one client node
  this->AddNode(true, GenerateUniqueRandomId(this->nodes_[source]->node_id(), 8));
  // Add one vault node
  this->AddNode(false, GenerateUniqueRandomId(this->nodes_[source]->node_id(), 24));

  size_t client(this->nodes_.size() - 1);
  size_t vault(this->ClientIndex() - 1);
  EXPECT_TRUE(this->nodes_.at(client)->IsClient());
  EXPECT_FALSE(this->nodes_.at(vault)->IsClient());

  Sleep(std::chrono::seconds(1));

  EXPECT_TRUE(this->nodes_[client]->RoutingTableHasNode(this->nodes_[source]->node_id()));
  EXPECT_FALSE(this->nodes_[source]->RoutingTableHasNode(this->nodes_[client]->node_id()));
  EXPECT_TRUE(this->nodes_[source]->ClientRoutingTableHasNode(this->nodes_[client]->node_id()));

  EXPECT_TRUE(this->nodes_[client]->RoutingTableHasNode(this->nodes_[vault]->node_id()));
  EXPECT_FALSE(this->nodes_[vault]->RoutingTableHasNode(this->nodes_[client]->node_id()));
  EXPECT_TRUE(this->nodes_[vault]->ClientRoutingTableHasNode(this->nodes_[client]->node_id()));

  // trying to find
  EXPECT_TRUE(Find(this->nodes_[vault], this->nodes_[vault]->node_id()));
  Sleep(std::chrono::seconds(1));
  EXPECT_FALSE(this->nodes_[vault]->RoutingTableHasNode(this->nodes_[client]->node_id()));
  EXPECT_TRUE(this->nodes_[vault]->ClientRoutingTableHasNode(this->nodes_[client]->node_id()));
}

TEST_F(FindNodeNetwork, FUNC_ClientFindClientNode) {
  this->SetUpNetwork(kServerSize, kClientSize);
  size_t source(this->RandomVaultIndex()),
         client1(this->nodes_.size()),
         client2(client1 + 1);

  // Add two client nodes
  this->AddNode(true, GenerateUniqueRandomId(this->nodes_[source]->node_id(), 8));
  this->AddNode(true, GenerateUniqueRandomId(this->nodes_[source]->node_id(), 12));
  Sleep(std::chrono::seconds(1));
  EXPECT_TRUE(this->nodes_.at(client1)->IsClient());
  EXPECT_TRUE(this->nodes_.at(client2)->IsClient());

  EXPECT_TRUE(this->nodes_[client1]->RoutingTableHasNode(this->nodes_[source]->node_id()));
  EXPECT_TRUE(this->nodes_[client2]->RoutingTableHasNode(this->nodes_[source]->node_id()));
  EXPECT_FALSE(this->nodes_[source]->RoutingTableHasNode(this->nodes_[client1]->node_id()));
  EXPECT_TRUE(this->nodes_[source]->ClientRoutingTableHasNode(this->nodes_[client1]->node_id()));
  EXPECT_FALSE(this->nodes_[source]->RoutingTableHasNode(this->nodes_[client2]->node_id()));
  EXPECT_TRUE(this->nodes_[source]->ClientRoutingTableHasNode(this->nodes_[client2]->node_id()));

  EXPECT_FALSE(this->nodes_[client1]->RoutingTableHasNode(this->nodes_[client2]->node_id()));
  EXPECT_FALSE(this->nodes_[client1]->ClientRoutingTableHasNode(this->nodes_[client2]->node_id()));
  EXPECT_FALSE(this->nodes_[client2]->RoutingTableHasNode(this->nodes_[client1]->node_id()));
  EXPECT_FALSE(this->nodes_[client2]->ClientRoutingTableHasNode(this->nodes_[client1]->node_id()));

  // trying to find
  EXPECT_TRUE(Find(this->nodes_[client1], this->nodes_[client1]->node_id()));
  Sleep(std::chrono::seconds(5));
  EXPECT_FALSE(this->nodes_[client1]->RoutingTableHasNode(this->nodes_[client2]->node_id()));
  EXPECT_FALSE(this->nodes_[client1]->ClientRoutingTableHasNode(this->nodes_[client2]->node_id()));
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
