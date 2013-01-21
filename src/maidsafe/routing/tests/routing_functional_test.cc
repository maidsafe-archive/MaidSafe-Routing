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

namespace maidsafe {

namespace routing {

namespace test {

class RoutingNetworkTest : public testing::Test {
 public:
  RoutingNetworkTest(void) : env_(NodesEnvironment::g_environment()) {}

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

TEST_F(RoutingNetworkTest, FUNC_GroupUpdateSubscription) {
  std::vector<NodeInfo> closest_nodes_info;
  for (auto node : env_->nodes_) {
    if ((node->node_id() == env_->nodes_[kServerSize - 1]->node_id()) ||
        (node->node_id() == env_->nodes_[kNetworkSize - 1]->node_id()))
      continue;
    closest_nodes_info = env_->GetClosestNodes(node->node_id(),
                                               Parameters::closest_nodes_size - 1);
    LOG(kVerbose) << "size of closest_nodes: " << closest_nodes_info.size();

    for (auto node_info : closest_nodes_info) {
      int index(env_->NodeIndex(node_info.node_id));
      if ((index == kServerSize - 1) || env_->nodes_[index]->IsClient())
        continue;
      EXPECT_TRUE(env_->nodes_[index]->NodeSubscriedForGroupUpdate(node->node_id()))
          << DebugId(node_info.node_id) << " does not have " << DebugId(node->node_id());
    }
  }
}

TEST_F(RoutingNetworkTest, FUNC_SanityCheck) {
  {
    EXPECT_TRUE(env_->Send(3));
    // This sleep is required for un-responded requests
    Sleep(boost::posix_time::seconds(static_cast<long>(env_->nodes_.size() + 1)));  // NOLINT (Fraser)
    env_->ClearMessages();
  }
  {
    //  SendGroup
    uint16_t random_node(static_cast<uint16_t>(env_->RandomVaultIndex()));
    NodeId target_id(env_->nodes_[random_node]->node_id());
    std::vector<NodeId> group_Ids(env_->GetGroupForId(target_id));
    EXPECT_TRUE(env_->SendGroup(target_id, 1));
    for (auto& group_id : group_Ids)
      EXPECT_EQ(1, env_->nodes_.at(env_->NodeIndex(group_id))->MessagesSize());
    env_->ClearMessages();

    // SendGroup SelfId
    EXPECT_TRUE(env_->SendGroup(target_id, 1, random_node));
    for (auto& group_id : group_Ids)
      EXPECT_EQ(1, env_->nodes_.at(env_->NodeIndex(group_id))->MessagesSize());
    env_->ClearMessages();

    // Client SendGroup
    EXPECT_TRUE(env_->SendGroup(target_id, 1, kNetworkSize - 1));
    for (auto& group_id : group_Ids)
      EXPECT_EQ(1, env_->nodes_.at(env_->NodeIndex(group_id))->MessagesSize());
    env_->ClearMessages();

    // SendGroup RandomId
    target_id = NodeId(NodeId::kRandomId);
    group_Ids = env_->GetGroupForId(target_id);
    EXPECT_TRUE(env_->SendGroup(target_id, 1));
    for (auto& group_id : group_Ids)
      EXPECT_EQ(1, env_->nodes_.at(env_->NodeIndex(group_id))->MessagesSize());
    env_->ClearMessages();
  }
  {
    // Join client with same Id
    env_->AddNode(true, env_->nodes_[env_->RandomClientIndex()]->node_id());

    // Send to client with same Id
    EXPECT_TRUE(env_->Send(env_->nodes_[kNetworkSize], env_->nodes_[kNetworkSize]->node_id()));
    env_->ClearMessages();
  }
  {
    // Anonymous join
    env_->AddNode(true, NodeId(), true);

    // Anonymous group send
    NodeId target_id(NodeId::kRandomId);
    std::vector<NodeId> group_Ids(env_->GetGroupForId(target_id));
    EXPECT_TRUE(env_->SendGroup(target_id, 1, static_cast<uint16_t>(env_->nodes_.size() - 1)));
    for (auto& group_id : group_Ids)
      EXPECT_EQ(1, env_->nodes_.at(env_->NodeIndex(group_id))->MessagesSize());
    env_->ClearMessages();
  }
}

TEST_F(RoutingNetworkTest, FUNC_SanityCheckSend) {
  // Signature 1
  EXPECT_TRUE(env_->Send(1 + RandomUint32() % 5));

  // Signature 2
  EXPECT_TRUE(env_->Send(env_->RandomVaultNode()->node_id()));

  EXPECT_TRUE(env_->Send(env_->RandomClientNode()->node_id()));

  EXPECT_TRUE(env_->Send(NodeId(NodeId::kRandomId)));

  // Signature 3
  EXPECT_TRUE(env_->Send(env_->RandomVaultNode(), env_->RandomVaultNode()->node_id()));

  EXPECT_TRUE(env_->Send(env_->RandomVaultNode(), env_->RandomClientNode()->node_id()));

  EXPECT_TRUE(env_->Send(env_->RandomClientNode(), env_->RandomVaultNode()->node_id()));

  EXPECT_TRUE(env_->Send(env_->RandomClientNode(), env_->RandomClientNode()->node_id()));
}

TEST_F(RoutingNetworkTest, FUNC_SanityCheckSendGroup) {
  EXPECT_TRUE(env_->SendGroup(NodeId(NodeId::kRandomId),
                              1 + RandomUint32() % 5,
                              env_->RandomVaultIndex()));

  EXPECT_TRUE(env_->SendGroup(NodeId(NodeId::kRandomId),
                              1 + RandomUint32() % 5,
                              env_->RandomClientIndex()));

  EXPECT_TRUE(env_->SendGroup(env_->RandomVaultNode()->node_id(),
                              1 + RandomUint32() % 5,
                              env_->RandomVaultIndex()));

  EXPECT_TRUE(env_->SendGroup(env_->RandomVaultNode()->node_id(),
                              1 + RandomUint32() % 5,
                              env_->RandomClientIndex()));

  EXPECT_TRUE(env_->SendGroup(env_->RandomClientNode()->node_id(),
                              1 + RandomUint32() % 5,
                              env_->RandomVaultIndex()));

  EXPECT_TRUE(env_->SendGroup(env_->RandomClientNode()->node_id(),
                              1 + RandomUint32() % 5,
                              env_->RandomClientIndex()));
}

TEST_F(RoutingNetworkTest, FUNC_Send) {
  boost::progress_timer t;
  EXPECT_TRUE(env_->Send(1));
  std::cout << "Time taken for test : " << t.elapsed();
}

TEST_F(RoutingNetworkTest, FUNC_SendToNonExistingNode) {
  EXPECT_TRUE(env_->Send(NodeId(NodeId::kRandomId)));
  EXPECT_TRUE(env_->Send(env_->nodes_[env_->RandomVaultIndex()]->node_id()));
}

TEST_F(RoutingNetworkTest, FUNC_ClientSend) {
  EXPECT_TRUE(env_->Send(1));
  Sleep(boost::posix_time::seconds(21));  // This sleep is required for un-responded requests
}

TEST_F(RoutingNetworkTest, FUNC_SendMulti) {
  boost::progress_timer t;
  EXPECT_TRUE(env_->Send(5));
  std::cout << "Time taken for test : " << t.elapsed();
}

TEST_F(RoutingNetworkTest, FUNC_ClientSendMulti) {
  EXPECT_TRUE(env_->Send(3));
// This sleep is required for un-responded requests
  Sleep(boost::posix_time::seconds(static_cast<long>(env_->nodes_.size() + 1)));  // NOLINT (Fraser)
}

TEST_F(RoutingNetworkTest, FUNC_SendToGroup) {
  uint16_t message_count(10), receivers_message_count(0);
  size_t last_index(kServerSize - 1);
  NodeId dest_id(env_->nodes_[last_index]->node_id());

  env_->ClearMessages();
  boost::progress_timer t;
  EXPECT_TRUE(env_->SendGroup(dest_id, message_count));
  std::cout << "Time taken for test : " << t.elapsed();
  for (size_t index = 0; index != (last_index); ++index)
    receivers_message_count += static_cast<uint16_t>(env_->nodes_.at(index)->MessagesSize());

  EXPECT_EQ(0, env_->nodes_[last_index]->MessagesSize())
      << "Not expected message at Node : "
      << HexSubstr(env_->nodes_[last_index]->node_id().string());
  EXPECT_EQ(message_count * (Parameters::node_group_size), receivers_message_count);
}

TEST_F(RoutingNetworkTest, FUNC_SendToGroupSelfId) {
  uint16_t message_count(10), receivers_message_count(0);
  size_t last_index(kServerSize - 1);
  NodeId dest_id(env_->nodes_[0]->node_id());

  env_->ClearMessages();
  EXPECT_TRUE(env_->SendGroup(dest_id, message_count));
  for (size_t index = 0; index != (last_index); ++index)
    receivers_message_count += static_cast<uint16_t>(env_->nodes_.at(index)->MessagesSize());

  EXPECT_EQ(0, env_->nodes_[0]->MessagesSize())
        << "Not expected message at Node : "
        << HexSubstr(env_->nodes_[0]->node_id().string());
  EXPECT_EQ(message_count * (Parameters::node_group_size), receivers_message_count);
}

TEST_F(RoutingNetworkTest, FUNC_SendToGroupClientSelfId) {
  size_t message_count(100), receivers_message_count(0);

  size_t client_index(env_->RandomClientIndex());

  size_t last_index(env_->nodes_.size());
  NodeId dest_id(env_->nodes_[client_index]->node_id());

  env_->ClearMessages();
  EXPECT_TRUE(env_->SendGroup(dest_id, message_count,
                              static_cast<uint16_t>(client_index)));  // from client
  for (size_t index = 0; index != (last_index); ++index)
    receivers_message_count += static_cast<uint16_t>(env_->nodes_.at(index)->MessagesSize());

  EXPECT_EQ(0, env_->nodes_[client_index]->MessagesSize())
        << "Not expected message at Node : "
        << HexSubstr(env_->nodes_[client_index]->node_id().string());
  EXPECT_EQ(message_count * (Parameters::node_group_size), receivers_message_count);
}

TEST_F(RoutingNetworkTest, FUNC_SendToGroupInHybridNetwork) {
  uint16_t message_count(1), receivers_message_count(0);
  LOG(kVerbose) << "Network created";
  size_t last_index(env_->nodes_.size() - 1);
  NodeId dest_id(env_->nodes_[last_index]->node_id());

  env_->ClearMessages();
  EXPECT_TRUE(env_->SendGroup(dest_id, message_count));
  for (size_t index = 0; index != (last_index); ++index)
    receivers_message_count += static_cast<uint16_t>(env_->nodes_.at(index)->MessagesSize());

  EXPECT_EQ(0, env_->nodes_[last_index]->MessagesSize())
        << "Not expected message at Node : "
        << HexSubstr(env_->nodes_[last_index]->node_id().string());
  EXPECT_EQ(message_count * (Parameters::node_group_size), receivers_message_count);
}

TEST_F(RoutingNetworkTest, FUNC_SendToGroupRandomId) {
  uint16_t message_count(200), receivers_message_count(0);
  env_->ClearMessages();
  for (int index = 0; index < message_count; ++index) {
    EXPECT_TRUE(env_->SendGroup(NodeId(NodeId::kRandomId), 1));
    for (auto node : env_->nodes_) {
      receivers_message_count += static_cast<uint16_t>(node->MessagesSize());
      node->ClearMessages();
    }
  }
  EXPECT_EQ(message_count * (Parameters::node_group_size), receivers_message_count);
  LOG(kVerbose) << "Total message received count : "
                << message_count * (Parameters::node_group_size);
}

TEST_F(RoutingNetworkTest, FUNC_AnonymousSendToGroupRandomId) {
  uint16_t message_count(200), receivers_message_count(0);
  env_->AddNode(true, NodeId(), true);
  assert(env_->nodes_.size() - 1 < std::numeric_limits<uint16_t>::max());
  for (int index = 0; index < message_count; ++index) {
    EXPECT_TRUE(env_->SendGroup(NodeId(NodeId::kRandomId), 1,
                                static_cast<uint16_t>(env_->nodes_.size() - 1)));
    for (auto node : env_->nodes_) {
      receivers_message_count += static_cast<uint16_t>(node->MessagesSize());
      node->ClearMessages();
    }
  }
  EXPECT_EQ(message_count * (Parameters::node_group_size), receivers_message_count);
  LOG(kVerbose) << "Total message received count : "
                << message_count * (Parameters::node_group_size);
}

TEST_F(RoutingNetworkTest, FUNC_AnonymousSendToGroupExistingId) {
  uint16_t message_count(200), receivers_message_count(0);
  size_t initial_network_size(env_->nodes_.size());
  env_->AddNode(true, NodeId(), true);
  assert(env_->nodes_.size() - 1 < std::numeric_limits<uint16_t>::max());
  for (int index = 0; index < message_count; ++index) {
    int group_id_index = index % initial_network_size;  // all other nodes
    NodeId group_id(env_->nodes_[group_id_index]->node_id());
    EXPECT_TRUE(env_->SendGroup(group_id, 1, static_cast<uint16_t>(env_->nodes_.size() - 1)));
    for (auto node : env_->nodes_) {
      receivers_message_count += static_cast<uint16_t>(node->MessagesSize());
      node->ClearMessages();
    }
  }
  EXPECT_EQ(message_count * (Parameters::node_group_size), receivers_message_count);
  LOG(kVerbose) << "Total message received count : "
                << message_count * (Parameters::node_group_size);
}

TEST_F(RoutingNetworkTest, FUNC_JoinWithSameId) {
  NodeId node_id(NodeId::kRandomId);
  env_->AddNode(true, node_id);
  env_->AddNode(true, node_id);
  env_->AddNode(true, node_id);
  env_->AddNode(true, node_id);
}

TEST_F(RoutingNetworkTest, FUNC_SendToClientsWithSameId) {
  const uint16_t kMessageCount(50);
  NodeId node_id(NodeId::kRandomId);
  for (uint16_t index(0); index < 4; ++index)
    env_->AddNode(true, node_id);

  for (uint16_t index(0); index < kMessageCount; ++index)
    EXPECT_TRUE(env_->Send(env_->nodes_[kNetworkSize], env_->nodes_[kNetworkSize]->node_id()));
  uint16_t num_of_tries(0);
  bool done(false);
  do {
    Sleep(boost::posix_time::seconds(1));
    size_t size(0);
    for (auto node : env_->nodes_) {
      size += node->MessagesSize();
    }
    if (4 * kMessageCount == size) {
      done = true;
      num_of_tries = 19;
    }
    ++num_of_tries;
  } while (num_of_tries < 20);
  EXPECT_TRUE(done);  // the number of 20 may need to be increased
}

TEST_F(RoutingNetworkTest, FUNC_SendToClientWithSameId) {
  NodeId node_id(env_->nodes_.at(env_->RandomClientIndex())->node_id());
  size_t new_index(env_->nodes_.size());
  env_->AddNode(true, node_id);
  size_t size(0);

  env_->ClearMessages();
  EXPECT_TRUE(env_->Send(env_->nodes_[new_index], node_id));
  Sleep(boost::posix_time::seconds(1));
  for (auto node : env_->nodes_) {
    size += node->MessagesSize();
  }
  EXPECT_EQ(2, size);
}

TEST_F(RoutingNetworkTest, FUNC_GetRandomExistingNode) {
  uint32_t collisions(0);
  size_t kChoseIndex(env_->RandomNodeIndex());
  EXPECT_TRUE(env_->Send(1));
//  EXPECT_LT(env_->nodes_[random_node]->RandomNodeVector().size(), 98);
//  for (auto node : env_->nodes_[random_node]->RandomNodeVector())
//    LOG(kVerbose) << HexSubstr(node.string());
  NodeId last_node(NodeId::kRandomId), last_random(NodeId::kRandomId);
  for (auto index(0); index < 100; ++index) {
    last_node = env_->nodes_[kChoseIndex]->GetRandomExistingNode();
    if (last_node == last_random) {
      LOG(kVerbose) << HexSubstr(last_random.string()) << ", " << HexSubstr(last_node.string());
      collisions++;
//      for (auto node : env_->nodes_[random_node]->RandomNodeVector())
//        LOG(kVerbose) << HexSubstr(node.string());
    }
    last_random = last_node;
  }
  ASSERT_LT(collisions, 50);
  for (int i(0); i < 120; ++i)
    env_->nodes_[kChoseIndex]->AddNodeToRandomNodeHelper(NodeId(NodeId::kRandomId));

  // Check there are 100 unique IDs in the RandomNodeHelper
  std::set<NodeId> random_node_ids;
  int attempts(0);
  while (attempts < 10000 && random_node_ids.size() < 100) {
    NodeId retrieved_id(env_->nodes_[kChoseIndex]->GetRandomExistingNode());
    env_->nodes_[kChoseIndex]->RemoveNodeFromRandomNodeHelper(retrieved_id);
    random_node_ids.insert(retrieved_id);
  }
  EXPECT_EQ(100, random_node_ids.size());
}

TEST_F(RoutingNetworkTest, FUNC_IsNodeIdInGroupRange) {
  std::vector<NodeId> vault_ids;
  for (auto node : env_->nodes_)
    if (!node->IsClient())
      vault_ids.push_back(node->node_id());
  EXPECT_GE(vault_ids.size(), Parameters::node_group_size);

  for (auto node : env_->nodes_) {
    if (!node->IsClient()) {
      // Check vault IDs from network
      std::partial_sort(vault_ids.begin(),
                        vault_ids.begin() + Parameters::node_group_size,
                        vault_ids.end(),
                        [=] (const NodeId& lhs, const NodeId& rhs)->bool {
                          return NodeId::CloserToTarget(lhs, rhs, node->node_id());
                        });
      for (uint16_t i(0); i < vault_ids.size(); ++i) {
        if (i < Parameters::node_group_size)
          EXPECT_TRUE(node->IsNodeIdInGroupRange(vault_ids.at(i)));
        else
          EXPECT_FALSE(node->IsNodeIdInGroupRange(vault_ids.at(i)));
      }

      // Check random IDs
      NodeId expected_threshold_id(vault_ids.at(Parameters::node_group_size - 1));
      for (uint16_t i(0); i < 50; ++i) {
        NodeId random_id(NodeId::kRandomId);
        if (NodeId::CloserToTarget(random_id, expected_threshold_id, node->node_id()))
          EXPECT_TRUE(node->IsNodeIdInGroupRange(random_id));
        else
          EXPECT_FALSE(node->IsNodeIdInGroupRange(random_id));
      }
    }
  }
}

/*
TEST_F(RoutingNetworkTest, FUNC_IsConnectedToVault) {
  ASSERT_LE(env_->ClientIndex(), Parameters::max_routing_table_size + 1);

  // Vault checks vault id - expect true
  for (uint16_t i(0); i < env_->ClientIndex(); ++i) {
    for (uint16_t j(0); j < env_->ClientIndex(); ++j) {
      if (i != j) {
        EXPECT_TRUE(env_->nodes_.at(i)->IsConnectedToVault(env_->nodes_.at(j)->node_id()));
      }
    }
  }

  // Vault or Client checks client id - expect false
  for (uint16_t i(0); i < env_->nodes_.size(); ++i) {
    for (uint16_t j(env_->ClientIndex()); j < env_->nodes_.size(); ++j) {
      EXPECT_FALSE(env_->nodes_.at(i)->IsConnectedToVault(env_->nodes_.at(j)->node_id()));
    }
  }

  // Client checks close vault id - expect true
  for (uint16_t i(env_->ClientIndex()); i < env_->nodes_.size(); ++i) {
    NodeId client_id(env_->nodes_.at(i)->node_id());
    std::vector<NodeInfo> closest_nodes(
          env_->GetClosestVaults(client_id, Parameters::max_client_routing_table_size));
    for (auto vault : closest_nodes) {
      EXPECT_TRUE(env_->nodes_.at(i)->IsConnectedToVault(vault.node_id));
    }
  }
}

TEST_F(RoutingNetworkTest, FUNC_IsConnectedToClient) {
  ASSERT_LE(env_->nodes_.size() - env_->ClientIndex(), Parameters::max_non_routing_table_size + 1);

  // Vault checks close client id - expect true
  for (uint16_t i(env_->ClientIndex()); i < env_->nodes_.size(); ++i) {
    NodeId client_id(env_->nodes_.at(i)->node_id());
    std::vector<NodeInfo> closest_nodes(
          env_->GetClosestVaults(client_id, Parameters::max_client_routing_table_size));
    for (auto node_info : closest_nodes) {
      int node_index(env_->NodeIndex(node_info.node_id));
      ASSERT_GE(node_index, 0);
      EXPECT_TRUE(env_->nodes_.at(node_index)->IsConnectedToClient(client_id));
    }
  }

  // Vault checks vault id - expect false
  for (uint16_t i(0); i < env_->ClientIndex(); ++i) {
    for (uint16_t j(0); j < env_->ClientIndex(); ++j) {
      if (i != j) {
        EXPECT_FALSE(env_->nodes_.at(i)->IsConnectedToClient(env_->nodes_.at(j)->node_id()));
      }
    }
  }
}

TEST_F(RoutingNetworkTest, FUNC_NonexistentIsConnectedToVaultOrClient) {
  NodeId non_existing_id;
  bool exists(true);
  while (exists) {
    non_existing_id = NodeId(NodeId::kRandomId);
    exists = false;
    for (auto node : env_->nodes_) {
      if (node->node_id() == non_existing_id)
        exists = true;
    }
  }

  for (auto node : env_->nodes_) {
    EXPECT_FALSE(node->IsConnectedToVault(non_existing_id));
    if (!node->IsClient())
      EXPECT_FALSE(node->IsConnectedToClient(non_existing_id));
  }
}
*/

TEST_F(RoutingNetworkTest, FUNC_ClosestNodes) {
  for (auto node : env_->nodes_) {
    // Note (15/01/13): Currently fails for clients. This is because GroupMatrix currently takes
    // the vault/client's own node ID upon creation and includes it in its contents. Instead it
    // should only take the ID for vaults.
    std::vector<NodeInfo> from_matrix(node->ClosestNodes());
    std::vector<NodeInfo> from_network(
          env_->GetClosestVaults(node->node_id(), static_cast<uint32_t>(from_matrix.size())));
    EXPECT_EQ(from_matrix.size(), from_network.size());
    auto min_size(std::min(from_matrix.size(), from_network.size()));
    EXPECT_LE(8U, min_size);

    std::string type("VAULT");
    if (node->IsClient())
      type = "CLIENT";
    for (uint16_t i(0); i < std::min(size_t(8), min_size); ++i)
      EXPECT_EQ(from_matrix.at(i).node_id, from_network.at(i).node_id) << "For node of type "
                                                                       << type;
  }
}

TEST_F(RoutingNetworkTest, FUNC_ClosestNodesClientBehindSymmetricNat) {
  NodeId sym_client_id(NodeId::kRandomId);
  env_->AddNode(true, sym_client_id, false, true);

  std::vector<NodeInfo> close_vaults(
        env_->GetClosestVaults(sym_client_id, Parameters::node_group_size));
  NodeId edge_id(close_vaults.back().node_id);

  std::vector<NodeId> closer_vaults;
  while (closer_vaults.size() < 2) {
    NodeId new_id(NodeId::kRandomId);
    if (NodeId::CloserToTarget(new_id, edge_id, sym_client_id))
      closer_vaults.push_back(new_id);
  }
  for (auto node_id : closer_vaults)
    env_->AddNode(false, node_id, false, true);

  EXPECT_TRUE(env_->WaitForHealthToStabilise());

  int index(env_->NodeIndex(sym_client_id));
  ASSERT_GE(index, 0);
  std::vector<NodeInfo> from_matrix(env_->nodes_.at(index)->ClosestNodes());
  std::vector<NodeInfo> from_network(
        env_->GetClosestVaults(sym_client_id, 8));
  EXPECT_LE(8U, from_matrix.size());

  for (uint16_t i(0); i < std::min(size_t(8), from_matrix.size()); ++i)
    EXPECT_EQ(from_matrix.at(i).node_id, from_network.at(i).node_id);
}

TEST_F(RoutingNetworkTest, FUNC_ClosestNodesVaultBehindSymmetricNat) {
  NodeId sym_vault_id(NodeId::kRandomId);
  env_->AddNode(false, sym_vault_id, false, true);

  std::vector<NodeInfo> close_vaults(
        env_->GetClosestVaults(sym_vault_id, Parameters::node_group_size + 1));  // exclude self
  NodeId edge_id(close_vaults.back().node_id);

  std::vector<NodeId> closer_vaults;
  while (closer_vaults.size() < 2) {
    NodeId new_id(NodeId::kRandomId);
    if (NodeId::CloserToTarget(new_id, edge_id, sym_vault_id))
      closer_vaults.push_back(new_id);
  }
  for (auto node_id : closer_vaults)
    env_->AddNode(false, node_id, false, true);

  EXPECT_TRUE(env_->WaitForHealthToStabilise());

  int index(env_->NodeIndex(sym_vault_id));
  ASSERT_GE(index, 0);
  std::vector<NodeInfo> from_matrix(env_->nodes_.at(index)->ClosestNodes());
  std::vector<NodeInfo> from_network(
        env_->GetClosestVaults(sym_vault_id, 9));
  EXPECT_LE(9U, from_matrix.size());

  for (uint16_t i(0); i < std::min(size_t(9), from_matrix.size()); ++i) {
    EXPECT_EQ(from_matrix.at(i).node_id, from_network.at(i).node_id);
  }
}

TEST_F(RoutingNetworkTest, FUNC_ClosestNodesBehindSymmetricNat) {
  uint16_t num_symmetric_vaults(kServerSize / 4);
  for (auto i(0); i < num_symmetric_vaults; ++i)
    env_->AddNode(false, true);
  uint16_t num_symmetric_clients(1 + RandomUint32() % (kClientSize / 2));
  for (auto i(0); i < num_symmetric_clients; ++i)
    env_->AddNode(true, true);

  EXPECT_TRUE(env_->WaitForHealthToStabilise());

  for (auto node : env_->nodes_) {
    // Note (17/01/13): Currently fails for clients. This is because GroupMatrix currently takes
    // the vault/client's own node ID upon creation and includes it in its contents. Instead it
    // should only take the ID for vaults.
    if (node->IsClient())
      continue;  // TODO(Alison) - remove this when FUNC_ClosestNodes (see above) is passing
    std::vector<NodeInfo> from_matrix(node->ClosestNodes());
    std::vector<NodeInfo> from_network(
          env_->GetClosestVaults(node->node_id(), 9));
    auto min_size(std::min(from_matrix.size(), from_network.size()));
    EXPECT_LE(9U, min_size);

    std::string type("VAULT");
    if (node->IsClient())
      type = "CLIENT";
    for (uint16_t i(0); i < std::min(size_t(9), min_size); ++i)
      EXPECT_EQ(from_matrix.at(i).node_id, from_network.at(i).node_id) << "For node of type "
                                                                       << type;
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
