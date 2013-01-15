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

#include "maidsafe/rudp/nat_type.h"

#include "maidsafe/routing/tests/routing_network.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

class RoutingStandAloneTest : public GenericNetwork, public testing::Test {
 public:
  RoutingStandAloneTest(void) : GenericNetwork() {}

  virtual void SetUp() {
    GenericNetwork::SetUp();
  }

  virtual void TearDown() {
    Sleep(boost::posix_time::microseconds(100));
  }
};

// TODO(Mahmoud): This test should be moved to TESTrouting_func as it doesn't affect network.
TEST_F(RoutingStandAloneTest, FUNC_GetGroup) {
  this->SetUpNetwork(kServerSize);
  int counter(100);
  while (counter-- > 0) {
    uint16_t random_node(static_cast<uint16_t>(RandomInt32() % kServerSize));
    NodeId node_id(NodeId::kRandomId);
    std::future<std::vector<NodeId>> future(this->nodes_[random_node]->GetGroup(node_id));
    auto nodes_id(future.get());
    auto group_ids(this->GroupIds(node_id));
    EXPECT_EQ(nodes_id.size(), group_ids.size());
    for (auto id : group_ids)
      EXPECT_NE(std::find(nodes_id.begin(), nodes_id.end(), id), nodes_id.end());
  }
}

// TODO(Mahmoud): This test should be moved to TESTrouting_func as it doesn't affect network.
TEST_F(RoutingStandAloneTest, FUNC_GroupUpdateSubscription) {
  std::vector<NodeInfo> closest_nodes_info;
  this->SetUpNetwork(kServerSize);
  for (auto node : this->nodes_) {
    if (node->node_id() == this->nodes_[kServerSize - 1]->node_id())
      continue;
    closest_nodes_info = this->GetClosestNodes(node->node_id(), Parameters::closest_nodes_size - 1);
    LOG(kVerbose) << "size of closest_nodes: " << closest_nodes_info.size();
    for (auto node_info : closest_nodes_info) {
      int index(this->NodeIndex(node_info.node_id));
      if (index == kServerSize - 1)
        continue;
      EXPECT_TRUE(this->nodes_[index]->NodeSubscriedForGroupUpdate(node->node_id()))
          << DebugId(node_info.node_id) << " does not have " << DebugId(node->node_id());
    }
  }
}

TEST_F(RoutingStandAloneTest, FUNC_SetupNetwork) {
  this->SetUpNetwork(kServerSize);
}

TEST_F(RoutingStandAloneTest, FUNC_SetupSingleClientHybridNetwork) {
  this->SetUpNetwork(kServerSize, 1);
}

TEST_F(RoutingStandAloneTest, FUNC_SetupHybridNetwork) {
  this->SetUpNetwork(kServerSize, kClientSize);
}

TEST_F(RoutingStandAloneTest, DISABLED_FUNC_ExtendedSendMulti) {
  // N.B. This test takes approx. 1hr to run, hence it is disabled.
  this->SetUpNetwork(kServerSize);
  uint16_t loop(100);
  while (loop-- > 0) {
    EXPECT_TRUE(Send(40));
    this->ClearMessages();
  }
}

TEST_F(RoutingStandAloneTest, FUNC_ExtendedSendToGroup) {
  uint16_t message_count(10), receivers_message_count(0);
  this->SetUpNetwork(kServerSize);
  size_t last_index(this->nodes_.size() - 1);
  NodeId dest_id(this->nodes_[last_index]->node_id());

  uint16_t loop(100);
  while (loop-- > 0) {
    EXPECT_TRUE(GroupSend(dest_id, message_count));
    for (size_t index = 0; index != (last_index); ++index)
      receivers_message_count += static_cast<uint16_t>(this->nodes_.at(index)->MessagesSize());

    EXPECT_EQ(0, this->nodes_[last_index]->MessagesSize())
          << "Not expected message at Node : "
          << HexSubstr(this->nodes_[last_index]->node_id().string());
    EXPECT_EQ(message_count * (Parameters::node_group_size), receivers_message_count);
    receivers_message_count = 0;
    this->ClearMessages();
  }
}

TEST_F(RoutingStandAloneTest, FUNC_ExtendedSendToGroupRandomId) {
  uint16_t message_count(200), receivers_message_count(0);
  this->SetUpNetwork(kServerSize);
  uint16_t loop(10);
  while (loop-- > 0) {
    for (int index = 0; index < message_count; ++index) {
      NodeId random_id(NodeId::kRandomId);
      std::vector<NodeId> groupd_ids(this->GroupIds(random_id));
      EXPECT_TRUE(GroupSend(random_id, 1));
      for (auto node : this->nodes_) {
        if (std::find(groupd_ids.begin(), groupd_ids.end(), node->node_id()) !=
            groupd_ids.end()) {
          receivers_message_count += static_cast<uint16_t>(node->MessagesSize());
          node->ClearMessages();
        }
      }
    }
    EXPECT_EQ(message_count * (Parameters::node_group_size), receivers_message_count);
    LOG(kVerbose) << "Total message received count : "
                  << message_count * (Parameters::node_group_size);
    receivers_message_count = 0;
    this->ClearMessages();
  }
}

TEST_F(RoutingStandAloneTest, FUNC_CheckUnsubscription) {
  this->SetUpNetwork(kServerSize);
  size_t size(kServerSize);
  size_t random_index(this->nodes_.size() % size);
  NodePtr node(nodes_[random_index]);
  NodeInfo furthest_closest(node->GetNthClosestNode(node->node_id(),
                                                    Parameters::closest_nodes_size));
  LOG(kVerbose) << "Furthest close node: " << DebugId(furthest_closest.node_id);
  this->AddNode(false, GenerateUniqueRandomId(node->node_id(), 30));
  int index(this->NodeIndex(furthest_closest.node_id));
  EXPECT_FALSE(this->nodes_[index]->NodeSubscriedForGroupUpdate(
      node->node_id())) << DebugId(furthest_closest.node_id) << " hase "
                        << DebugId(node->node_id());
  EXPECT_TRUE(this->nodes_[size]->NodeSubscriedForGroupUpdate(
      node->node_id())) << DebugId(this->nodes_[size]->node_id())
                        << " does not have " << DebugId(node->node_id());
}


TEST_F(RoutingStandAloneTest, FUNC_NodeRemoved) {
  this->SetUpNetwork(kServerSize);
  size_t random_index(this->RandomNodeIndex());
  NodeInfo removed_node_info(this->nodes_[random_index]->GetRemovableNode());
  EXPECT_GE(removed_node_info.bucket, 510);
}

// This test produces the recursive call.
TEST_F(RoutingStandAloneTest, FUNC_RecursiveCall) {
  this->SetUpNetwork(kServerSize);
  for (int index(0); index < 8; ++index)
    this->AddNode(false, GenerateUniqueRandomId(20));
  this->AddNode(true, GenerateUniqueRandomId(40));
  this->AddNode(false, GenerateUniqueRandomId(35));
  this->AddNode(false, GenerateUniqueRandomId(30));
  this->AddNode(false, GenerateUniqueRandomId(25));
  this->AddNode(false, GenerateUniqueRandomId(20));
  this->AddNode(false, GenerateUniqueRandomId(10));
  this->AddNode(true, GenerateUniqueRandomId(10));
}

TEST_F(RoutingStandAloneTest, FUNC_JoinAfterBootstrapLeaves) {
  this->SetUpNetwork(kServerSize);
  Sleep(boost::posix_time::seconds(10));
  this->AddNode(false, NodeId());
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
