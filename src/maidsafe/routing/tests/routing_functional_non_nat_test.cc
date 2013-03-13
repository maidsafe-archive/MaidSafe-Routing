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

  void SetUp() {
    EXPECT_TRUE(env_->RestoreComposition());
    EXPECT_TRUE(env_->WaitForHealthToStabilise());
  }

  void TearDown() {
    EXPECT_LE(kServerSize, env_->ClientIndex());
    EXPECT_LE(kNetworkSize, env_->nodes_.size());
    EXPECT_TRUE(env_->RestoreComposition());
  }

 protected:
  std::shared_ptr<GenericNetwork> env_;
};

TEST_F(RoutingNetworkNonNatTest, FUNC_GroupUpdateSubscription) {
  // TODO(Alison) - test currently only passes when run before all other functional tests. Needs
  // update to pass when run after other functional tests.
  std::vector<NodeInfo> closest_nodes_info;
  for (const auto& node : env_->nodes_) {  // NOLINT (Alison)
    if ((node->node_id() == env_->nodes_[kServerSize - 1]->node_id()) ||
        (node->node_id() == env_->nodes_[kNetworkSize - 1]->node_id()))
      continue;
    closest_nodes_info = env_->GetClosestNodes(node->node_id(),
                                               Parameters::closest_nodes_size - 1);
    LOG(kVerbose) << "size of closest_nodes: " << closest_nodes_info.size();

    for (const auto& node_info : closest_nodes_info) {  // NOLINT (Alison)
      int index(env_->NodeIndex(node_info.node_id));
      if ((index == kServerSize - 1) || env_->nodes_[index]->IsClient())
        continue;
      EXPECT_TRUE(env_->nodes_[index]->NodeSubscriedForGroupUpdate(node->node_id()))
          << DebugId(node_info.node_id) << " does not have " << DebugId(node->node_id());
    }
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
