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

#include <vector>

#include "boost/progress.hpp"

#include "maidsafe/rudp/nat_type.h"

#include "maidsafe/routing/tests/routing_network.h"
#include "maidsafe/routing/tests/test_utils.h"

// TODO(Alison) - IsNodeIdInGroupRange - test kInProximalRange and kOutwithRange more thoroughly

namespace maidsafe {

namespace routing {

namespace test {

class RoutingNetworkNonNatTest : public testing::Test {
 public:
  RoutingNetworkNonNatTest(void) : env_(NodesEnvironment::g_environment()) {}

  void SetUp() override {
    EXPECT_TRUE(env_->RestoreComposition());
    EXPECT_TRUE(env_->WaitForHealthToStabilise());
  }

  void TearDown() override {
    EXPECT_LE(kServerSize, env_->ClientIndex());
    EXPECT_LE(kNetworkSize, env_->nodes_.size());
    EXPECT_TRUE(env_->RestoreComposition());
  }

 protected:
  std::shared_ptr<GenericNetwork> env_;
};

TEST_F(RoutingNetworkNonNatTest, FUNC_GroupUpdateSubscription) {
  std::vector<NodeInfo> closest_nodes_info;
  for (const auto& node : env_->nodes_) {
    if ((node->node_id() == env_->nodes_[kServerSize - 1]->node_id()) ||
        (node->node_id() == env_->nodes_[kNetworkSize - 1]->node_id()))
      continue;
    closest_nodes_info = env_->GetClosestNodes(node->node_id(), Parameters::closest_nodes_size - 1);
    LOG(kVerbose) << "size of closest_nodes: " << closest_nodes_info.size();

    int my_index(env_->NodeIndex(node->node_id()));
    for (const auto& node_info : closest_nodes_info) {
      int index(env_->NodeIndex(node_info.node_id));
      if ((index == kServerSize - 1) || env_->nodes_[index]->IsClient())
        continue;
      if (!node->IsClient()) {
        EXPECT_TRUE(env_->nodes_[index]->NodeSubscribedForGroupUpdate(node->node_id()))
            << DebugId(node_info.node_id) << " does not have " << DebugId(node->node_id());
        EXPECT_TRUE(env_->nodes_[my_index]->NodeSubscribedForGroupUpdate(node_info.node_id))
            << DebugId(node->node_id()) << " does not have " << DebugId(node_info.node_id);
      } else {
        EXPECT_GE(node->GetGroupMatrixConnectedPeers().size(), 8);
      }
    }
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
