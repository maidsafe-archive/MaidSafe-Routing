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

class RoutingNetworkTest : public testing::Test {
 public:
  RoutingNetworkTest(void) : env_(NodesEnvironment::g_environment()) {}

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

TEST_F(RoutingNetworkTest, FUNC_SanityCheck) {
  {
    EXPECT_TRUE(env_->SendDirect(3));
    env_->ClearMessages();
  }
  {
    //  SendGroup
    uint16_t random_node(static_cast<uint16_t>(env_->RandomVaultIndex()));
    NodeId target_id(env_->nodes_[random_node]->node_id());
    std::vector<NodeId> group_Ids(env_->GetGroupForId(target_id));
    EXPECT_TRUE(env_->SendGroup(target_id, 1));
    for (const auto& group_id : group_Ids)
      EXPECT_EQ(1, env_->nodes_.at(env_->NodeIndex(group_id))->MessagesSize());
    env_->ClearMessages();

    // SendGroup SelfId
    EXPECT_TRUE(env_->SendGroup(target_id, 1, random_node));
    for (const auto& group_id : group_Ids)
      EXPECT_EQ(1, env_->nodes_.at(env_->NodeIndex(group_id))->MessagesSize());
    env_->ClearMessages();

    // Client SendGroup
    uint16_t random_client(static_cast<uint16_t>(env_->RandomClientIndex()));
    EXPECT_TRUE(env_->SendGroup(target_id, 1, random_client));
    for (const auto& group_id : group_Ids)
      EXPECT_EQ(1, env_->nodes_.at(env_->NodeIndex(group_id))->MessagesSize());
    env_->ClearMessages();

    // SendGroup RandomId
    target_id = NodeId(NodeId::kRandomId);
    group_Ids = env_->GetGroupForId(target_id);
    EXPECT_TRUE(env_->SendGroup(target_id, 1));
    for (const auto& group_id : group_Ids)
      EXPECT_EQ(1, env_->nodes_.at(env_->NodeIndex(group_id))->MessagesSize());
    env_->ClearMessages();
  }
  {
    // Join client with same Id
    env_->AddNode(env_->nodes_[env_->RandomClientIndex()]->GetMaid());

    // Send to client with same Id
    EXPECT_TRUE(env_->SendDirect(env_->nodes_[kNetworkSize], env_->nodes_[kNetworkSize]->node_id(),
                                 kExpectClient));
    env_->ClearMessages();
  }
}

TEST_F(RoutingNetworkTest, FUNC_SanityCheckSend) {
  // Signature 1
  EXPECT_TRUE(env_->SendDirect(1 + RandomUint32() % 5));

  // Signature 2
  EXPECT_TRUE(env_->SendDirect(env_->RandomVaultNode()->node_id()));

  EXPECT_TRUE(env_->SendDirect(env_->RandomClientNode()->node_id(), kExpectClient));

  EXPECT_FALSE(env_->SendDirect(NodeId(NodeId::kRandomId), kExpectDoesNotExist));

  // Signature 3
  EXPECT_TRUE(env_->SendDirect(env_->RandomVaultNode(), env_->RandomVaultNode()->node_id()));

  uint16_t random_vault(env_->RandomVaultIndex());
  uint16_t random_client(env_->RandomClientIndex());
  EXPECT_TRUE(env_->SendDirect(env_->nodes_[random_vault], env_->nodes_[random_client]->node_id(),
                               kExpectClient));

  EXPECT_TRUE(env_->SendDirect(env_->RandomClientNode(), env_->RandomVaultNode()->node_id()));

  uint16_t another_random_client(env_->RandomClientIndex());
  EXPECT_EQ((random_client == another_random_client),
            env_->SendDirect(env_->nodes_[random_client],
                             env_->nodes_[another_random_client]->node_id(), kExpectClient));
}

TEST_F(RoutingNetworkTest, FUNC_SanityCheckSendGroup) {
  EXPECT_TRUE(
      env_->SendGroup(NodeId(NodeId::kRandomId), 1 + RandomUint32() % 5, env_->RandomVaultIndex()));

  EXPECT_TRUE(env_->SendGroup(NodeId(NodeId::kRandomId), 1 + RandomUint32() % 5,
                              env_->RandomClientIndex()));

  EXPECT_TRUE(env_->SendGroup(env_->RandomVaultNode()->node_id(), 1 + RandomUint32() % 5,
                              env_->RandomVaultIndex()));

  EXPECT_TRUE(env_->SendGroup(env_->RandomVaultNode()->node_id(), 1 + RandomUint32() % 5,
                              env_->RandomClientIndex()));

  EXPECT_TRUE(env_->SendGroup(env_->RandomClientNode()->node_id(), 1 + RandomUint32() % 5,
                              env_->RandomVaultIndex()));

  EXPECT_TRUE(env_->SendGroup(env_->RandomClientNode()->node_id(), 1 + RandomUint32() % 5,
                              env_->RandomClientIndex()));
}

TEST_F(RoutingNetworkTest, FUNC_Send) {
  boost::progress_timer t;
  EXPECT_TRUE(env_->SendDirect(1));
  std::cout << "Time taken for test : " << t.elapsed();
}

TEST_F(RoutingNetworkTest, FUNC_SendToNonExistingNode) {
  EXPECT_FALSE(env_->SendDirect(NodeId(NodeId::kRandomId), kExpectDoesNotExist));
  EXPECT_TRUE(env_->SendDirect(env_->nodes_[env_->RandomVaultIndex()]->node_id()));
}

TEST_F(RoutingNetworkTest, FUNC_ClientSend) { EXPECT_TRUE(env_->SendDirect(1)); }

TEST_F(RoutingNetworkTest, FUNC_SendMulti) {
  boost::progress_timer t;
  EXPECT_TRUE(env_->SendDirect(5));
  std::cout << "Time taken for test : " << t.elapsed();
}

TEST_F(RoutingNetworkTest, FUNC_ClientSendMulti) { EXPECT_TRUE(env_->SendDirect(3)); }

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
  EXPECT_EQ(message_count * (Parameters::group_size), receivers_message_count);
}

TEST_F(RoutingNetworkTest, FUNC_SendToGroupSelfId) {
  uint16_t message_count(10), receivers_message_count(0);
  env_->ClearMessages();
  std::vector<std::future<std::unique_ptr<testing::AssertionResult>>> futures;

  for (uint16_t dest_index(0); dest_index < kServerSize; ++dest_index) {
    NodeId dest_id(env_->nodes_.at(dest_index)->node_id());
    futures.emplace_back(
        std::async(std::launch::async, [this, dest_id, &message_count, dest_index]() {
          return std::move(std::unique_ptr<testing::AssertionResult>(
              new testing::AssertionResult(env_->SendGroup(dest_id, message_count, dest_index))));
        }));
    Sleep(std::chrono::milliseconds(10));
  }
  while (!futures.empty()) {
    futures.erase(
        std::remove_if(
            futures.begin(),
            futures.end(),
                [](std::future<std::unique_ptr<testing::AssertionResult>> & future_bool)->bool {
              if (IsReady(future_bool)) {
                EXPECT_TRUE(*future_bool.get());
                return true;
              } else {
                return false;
              }
            }),
        futures.end());
    std::this_thread::yield();
  }

  for (auto& node : env_->nodes_) {
    receivers_message_count += static_cast<uint16_t>(node->MessagesSize());
    node->ClearMessages();
  }
  EXPECT_EQ(message_count * (Parameters::group_size) * kServerSize, receivers_message_count);
  LOG(kVerbose) << "Total message received count : " << receivers_message_count;
}

TEST_F(RoutingNetworkTest, FUNC_SendToGroupClientSelfId) {
  size_t message_count(100), receivers_message_count(0);

  size_t client_index(env_->RandomClientIndex());

  size_t last_index(env_->nodes_.size());
  NodeId dest_id(env_->nodes_[client_index]->node_id());

  env_->ClearMessages();
  EXPECT_TRUE(
      env_->SendGroup(dest_id, message_count, static_cast<uint16_t>(client_index)));  // from client
  for (size_t index = 0; index != (last_index); ++index)
    receivers_message_count += static_cast<uint16_t>(env_->nodes_.at(index)->MessagesSize());

  EXPECT_EQ(0, env_->nodes_[client_index]->MessagesSize())
      << "Not expected message at Node : "
      << HexSubstr(env_->nodes_[client_index]->node_id().string());
  EXPECT_EQ(message_count * (Parameters::group_size), receivers_message_count);
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
  EXPECT_EQ(message_count * (Parameters::group_size), receivers_message_count);
}

TEST_F(RoutingNetworkTest, FUNC_SendToGroupRandomId) {
  uint16_t message_count(200), receivers_message_count(0);
  env_->ClearMessages();
  std::vector<std::future<std::unique_ptr<testing::AssertionResult>>> futures;

  for (int index = 0; index < message_count; ++index) {
    futures.emplace_back(std::async(std::launch::async, [this]() {
      return std::move(std::unique_ptr<testing::AssertionResult>(
          new testing::AssertionResult(env_->SendGroup(NodeId(NodeId::kRandomId), 1))));
    }));
    Sleep(std::chrono::milliseconds(100));
  }
  while (!futures.empty()) {
    futures.erase(
        std::remove_if(
            futures.begin(),
            futures.end(),
                [](std::future<std::unique_ptr<testing::AssertionResult>> & future_bool)->bool {
              if (IsReady(future_bool)) {
                EXPECT_TRUE(*future_bool.get());
                return true;
              } else {
                return false;
              }
            }),
        futures.end()); // NOLINT
    std::this_thread::yield();
  }
  for (auto& node : env_->nodes_) {
    receivers_message_count += static_cast<uint16_t>(node->MessagesSize());
    node->ClearMessages();
  }
  EXPECT_EQ(message_count * (Parameters::group_size), receivers_message_count);
  LOG(kVerbose) << "Total message received count : " << message_count *
                                                            (Parameters::group_size);
}

TEST_F(RoutingNetworkTest, FUNC_NonMutatingClientSendToGroupRandomId) {
  uint16_t message_count(100), receivers_message_count(0);
  env_->ClearMessages();
  std::vector<std::future<std::unique_ptr<testing::AssertionResult>>> futures;

  env_->AddMutatingClient(false);
  assert(env_->nodes_.size() - 1 < std::numeric_limits<uint16_t>::max());

  for (int index = 0; index < message_count; ++index) {
    futures.emplace_back(std::async(std::launch::async, [this]() {
      return std::move(
          std::unique_ptr<testing::AssertionResult>(new testing::AssertionResult(env_->SendGroup(
              NodeId(NodeId::kRandomId), 1, static_cast<uint16_t>(env_->nodes_.size() - 1)))));
    }));
    Sleep(std::chrono::milliseconds(10));
  }
  while (!futures.empty()) {
    futures.erase(
        std::remove_if(
            futures.begin(),
            futures.end(),
                [](std::future<std::unique_ptr<testing::AssertionResult>> & future_bool)->bool {
              if (IsReady(future_bool)) {
                EXPECT_TRUE(*future_bool.get());
                return true;
              } else {
                return false;
              }
            }),
        futures.end());  // NOLINT
    std::this_thread::yield();
  }

  for (auto& node : env_->nodes_) {
    receivers_message_count += static_cast<uint16_t>(node->MessagesSize());
    node->ClearMessages();
  }

  EXPECT_EQ(message_count * (Parameters::group_size), receivers_message_count);
  LOG(kVerbose) << "Total message received count : " << message_count *
                                                            (Parameters::group_size);
}

TEST_F(RoutingNetworkTest, FUNC_NonMutatingClientSendToGroupExistingId) {
  uint16_t message_count(100), receivers_message_count(0);
  env_->ClearMessages();
  std::vector<std::future<std::unique_ptr<testing::AssertionResult>>> futures;

  size_t initial_network_size(env_->nodes_.size());
  env_->AddMutatingClient(false);
  assert(env_->nodes_.size() - 1 < std::numeric_limits<uint16_t>::max());

  for (int index = 0; index < message_count; ++index) {
    int group_id_index = index % initial_network_size;  // all other nodes
    NodeId group_id(env_->nodes_[group_id_index]->node_id());
    futures.emplace_back(std::async(std::launch::async, [this, group_id]() {
      return std::move(std::unique_ptr<testing::AssertionResult>(new testing::AssertionResult(
          env_->SendGroup(group_id, 1, static_cast<uint16_t>(env_->nodes_.size() - 1)))));
    }));
    Sleep(std::chrono::milliseconds(10));
  }
  while (!futures.empty()) {
    futures.erase(
        std::remove_if(
            futures.begin(),
            futures.end(),
                [](std::future<std::unique_ptr<testing::AssertionResult>> & future_bool)->bool {
              if (IsReady(future_bool)) {
                EXPECT_TRUE(*future_bool.get());
                return true;
              } else {
                return false;
              }
            }),
        futures.end());
    std::this_thread::yield();
  }

  for (auto& node : env_->nodes_) {
    receivers_message_count += static_cast<uint16_t>(node->MessagesSize());
    node->ClearMessages();
  }

  EXPECT_EQ(message_count * (Parameters::group_size), receivers_message_count);
  LOG(kVerbose) << "Total message received count : " << message_count *
                                                            (Parameters::group_size);
}

TEST_F(RoutingNetworkTest, FUNC_JoinWithSameId) {
  auto maid(MakeMaid());
  env_->AddNode(maid);
  env_->AddNode(maid);
  env_->AddNode(maid);
  env_->AddNode(maid);
}

TEST_F(RoutingNetworkTest, FUNC_SendToClientsWithSameId) {
  // TODO(Prakash) - send messages in parallel so test duration is reduced.
  // TODO(Prakash) - revert kMessageCount to 50 when test duration fixed.
  const uint16_t kMessageCount(5);
  auto maid(MakeMaid());
  for (uint16_t index(0); index < 4; ++index)
    env_->AddNode(maid);

  for (uint16_t index(0); index < kMessageCount; ++index)
    EXPECT_TRUE(env_->SendDirect(env_->nodes_[kNetworkSize], env_->nodes_[kNetworkSize]->node_id(),
                                 kExpectClient));
  uint16_t num_of_tries(0);
  bool done(false);
  do {
    //    Sleep(std::chrono::seconds(1));
    size_t size(0);
    for (const auto& node : env_->nodes_) {
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
  auto maid(env_->nodes_.at(env_->RandomClientIndex())->GetMaid());
  size_t new_index(env_->nodes_.size());
  env_->AddNode(maid);
  size_t size(0);

  env_->ClearMessages();
  NodeId node_id(maid.name());
  EXPECT_TRUE(env_->SendDirect(env_->nodes_[new_index], node_id, kExpectClient));
  for (const auto& node : env_->nodes_) {
    size += node->MessagesSize();
  }
  EXPECT_EQ(2, size);
}

TEST_F(RoutingNetworkTest, FUNC_IsNodeIdInGroupRange) {
  std::vector<NodeId> vault_ids;
  for (const auto& node : env_->nodes_)
    if (!node->IsClient())
      vault_ids.push_back(node->node_id());
  EXPECT_GE(vault_ids.size(), static_cast<size_t>(Parameters::group_size));

  for (const auto& node : env_->nodes_) {
    if (!node->IsClient()) {
      // Check vault IDs from network
      LOG(kVerbose) << "current:" << DebugId(node->node_id());
      for (uint16_t i(0); i < vault_ids.size(); ++i) {
        LOG(kVerbose) << "current vault id:" << DebugId(vault_ids.at(i));
        std::vector<NodeId> sorted_ids(routing::Parameters::group_size + 1);
        std::partial_sort_copy(std::begin(vault_ids), std::end(vault_ids), std::begin(sorted_ids),
                               std::end(sorted_ids),
                               [&](const NodeId& lhs, const NodeId& rhs)->bool {
                                 return NodeId::CloserToTarget(lhs, rhs, vault_ids.at(i));
                               });
        for (const auto& id : sorted_ids)
          LOG(kVerbose) << DebugId(id);
        if (!NodeId::CloserToTarget(sorted_ids.at(routing::Parameters::group_size),
                                    node->node_id(), vault_ids.at(i)))
          EXPECT_EQ(GroupRangeStatus::kInRange, node->IsNodeIdInGroupRange(vault_ids.at(i)));
        else
          EXPECT_NE(GroupRangeStatus::kInRange, node->IsNodeIdInGroupRange(vault_ids.at(i)));
      }

//      // Check random IDs
      for (uint16_t i(0); i < 50; ++i) {
        NodeId random_id(NodeId::kRandomId);
        std::partial_sort(std::begin(vault_ids), std::begin(vault_ids) + Parameters::group_size,
                          std::end(vault_ids),
                          [random_id](const NodeId& lhs, const NodeId& rhs) {
                            return NodeId::CloserToTarget(lhs, rhs, random_id);
                          });
        if (!NodeId::CloserToTarget(vault_ids.at(routing::Parameters::group_size - 1),
                                    node->node_id(), random_id))
          EXPECT_EQ(GroupRangeStatus::kInRange, node->IsNodeIdInGroupRange(random_id));
        else
          EXPECT_NE(GroupRangeStatus::kInRange, node->IsNodeIdInGroupRange(random_id));
      }
    }
  }
}

TEST_F(RoutingNetworkTest, FUNC_IsConnectedVault) {
  ASSERT_LE(env_->ClientIndex(), static_cast<size_t>(Parameters::max_routing_table_size + 1));

  // Vault checks vault id - expect true
  for (uint16_t i(0); i < env_->ClientIndex(); ++i) {
    for (uint16_t j(0); j < env_->ClientIndex(); ++j) {
      if (i != j) {
        EXPECT_TRUE(env_->nodes_.at(i)->IsConnectedVault(env_->nodes_.at(j)->node_id()));
      }
    }
  }

  // Vault or Client checks client id - expect false
  for (uint16_t i(0); i < env_->nodes_.size(); ++i) {
    for (size_t j(env_->ClientIndex()); j < env_->nodes_.size(); ++j) {
      EXPECT_FALSE(env_->nodes_.at(i)->IsConnectedVault(env_->nodes_.at(j)->node_id()));
    }
  }

  // Client checks close vault id - expect true
  for (size_t i(env_->ClientIndex()); i < env_->nodes_.size(); ++i) {
    NodeId client_id(env_->nodes_.at(i)->node_id());
    std::vector<NodeInfo> closest_nodes(
        env_->GetClosestVaults(client_id, Parameters::max_routing_table_size_for_client));
    for (const auto& vault : closest_nodes) {
      EXPECT_TRUE(env_->nodes_.at(i)->IsConnectedVault(vault.node_id));
    }
  }
}

TEST_F(RoutingNetworkTest, FUNC_IsConnectedClient) {
  ASSERT_LE(env_->nodes_.size() - env_->ClientIndex(),
            static_cast<size_t>(Parameters::max_client_routing_table_size + 1));

  // Vault checks close client id - expect true
  for (size_t i(env_->ClientIndex()); i < env_->nodes_.size(); ++i) {
    NodeId client_id(env_->nodes_.at(i)->node_id());
    std::vector<NodeInfo> closest_nodes(
        env_->GetClosestVaults(client_id, Parameters::max_routing_table_size_for_client));
    for (const auto& node_info : closest_nodes) {
      int node_index(env_->NodeIndex(node_info.node_id));
      ASSERT_GE(node_index, 0);
      EXPECT_TRUE(env_->nodes_.at(node_index)->IsConnectedClient(client_id));
    }
  }

  // Vault checks vault id - expect false
  for (uint16_t i(0); i < env_->ClientIndex(); ++i) {
    for (uint16_t j(0); j < env_->ClientIndex(); ++j) {
      if (i != j) {
        EXPECT_FALSE(env_->nodes_.at(i)->IsConnectedClient(env_->nodes_.at(j)->node_id()));
      }
    }
  }
}

TEST_F(RoutingNetworkTest, FUNC_NonexistentIsConnectedVaultOrClient) {
  NodeId non_existing_id;
  bool exists(true);
  while (exists) {
    non_existing_id = NodeId(NodeId::kRandomId);
    exists = false;
    for (const auto& node : env_->nodes_) {
      if (node->node_id() == non_existing_id)
        exists = true;
    }
  }

  for (const auto& node : env_->nodes_) {
    EXPECT_FALSE(node->IsConnectedVault(non_existing_id));
    if (!node->IsClient())
      EXPECT_FALSE(node->IsConnectedClient(non_existing_id));
  }
}

TEST_F(RoutingNetworkTest, FUNC_CheckGroupMatrixUniqueNodes) {
  env_->CheckGroupMatrixUniqueNodes();
}

TEST_F(RoutingNetworkTest, FUNC_ClosestNodesClientBehindSymmetricNat) {
  auto sym_client(MakeMaid());
  NodeId sym_client_id(sym_client.name());
  env_->AddNode(sym_client, true);

  std::vector<NodeInfo> close_vaults(
      env_->GetClosestVaults(sym_client_id, Parameters::group_size));
  NodeId edge_id(close_vaults.back().node_id);

  std::vector<passport::Pmid> closer_vaults;
  while (closer_vaults.size() < 2) {
    auto pmid(MakePmid());
    NodeId new_id(pmid.name());
    if (NodeId::CloserToTarget(new_id, edge_id, sym_client_id))
      closer_vaults.push_back(pmid);
  }
  for (const auto& node : closer_vaults)
    env_->AddNode(node, true);

  ASSERT_TRUE(env_->WaitForHealthToStabilise());
  ASSERT_TRUE(env_->WaitForNodesToJoin());

  int index(env_->NodeIndex(sym_client_id));
  ASSERT_GE(index, 0);
  std::vector<NodeInfo> from_matrix(env_->nodes_.at(index)->ClosestNodes());
  std::vector<NodeInfo> from_network(env_->GetClosestVaults(sym_client_id, 8));
  EXPECT_LE(8U, from_matrix.size());

  for (uint16_t i(0); i < std::min(size_t(8), from_matrix.size()); ++i)
    EXPECT_EQ(from_matrix.at(i).node_id, from_network.at(i).node_id);
}

TEST_F(RoutingNetworkTest, FUNC_ClosestNodesVaultBehindSymmetricNat) {
  auto pmid(MakePmid());
  NodeId sym_vault_id(pmid.name());
  env_->AddNode(pmid, true);

  std::vector<NodeInfo> close_vaults(
      env_->GetClosestVaults(sym_vault_id, Parameters::group_size + 1));  // exclude self
  NodeId edge_id(close_vaults.back().node_id);

  std::vector<passport::Pmid> closer_vaults;
  while (closer_vaults.size() < 2) {
    auto new_pmid(MakePmid());
    NodeId new_id(new_pmid.name());
    if (NodeId::CloserToTarget(new_id, edge_id, sym_vault_id))
      closer_vaults.push_back(new_pmid);
  }
  for (const auto& pmid : closer_vaults)
    env_->AddNode(pmid, true);

  ASSERT_TRUE(env_->WaitForHealthToStabilise());
  ASSERT_TRUE(env_->WaitForNodesToJoin());

  int index(env_->NodeIndex(sym_vault_id));
  ASSERT_GE(index, 0);
  std::vector<NodeInfo> from_matrix(env_->nodes_.at(index)->ClosestNodes());
  std::vector<NodeInfo> from_network(env_->GetClosestVaults(sym_vault_id, 9));
  EXPECT_LE(9U, from_matrix.size());

  for (uint16_t i(0); i < std::min(size_t(9), from_matrix.size()); ++i) {
    EXPECT_EQ(from_matrix.at(i).node_id, from_network.at(i).node_id);
  }
}

TEST_F(RoutingNetworkTest, FUNC_VaultJoinWhenClosestVaultAlsoBehindSymmetricNat) {
  auto sym_node1(MakePmid());
  NodeId sym_node_id_1(sym_node1.name());
  env_->AddNode(sym_node1, true);

  ASSERT_TRUE(env_->WaitForHealthToStabilise());
  ASSERT_TRUE(env_->WaitForNodesToJoin());

  std::vector<NodeInfo> closest_vaults(env_->GetClosestVaults(sym_node_id_1, 2));

  auto sym_node2(MakePmid());
  NodeId sym_node_id_2(sym_node2.name());

  while (NodeId::CloserToTarget(closest_vaults.at(1).node_id, sym_node_id_2, sym_node_id_1)) {
    sym_node2 = MakePmid();
    sym_node_id_2 = NodeId(sym_node2.name());
  }

  env_->AddNode(sym_node2, true);
}

TEST_F(RoutingNetworkTest, FUNC_ClientJoinWhenClosestVaultAlsoBehindSymmetricNat) {
  auto sym_node1(MakePmid());
  NodeId sym_node_id_1(sym_node1.name());
  env_->AddNode(sym_node1, true);

  ASSERT_TRUE(env_->WaitForHealthToStabilise());
  ASSERT_TRUE(env_->WaitForNodesToJoin());

  std::vector<NodeInfo> closest_vaults(env_->GetClosestVaults(sym_node_id_1, 2));

  auto sym_node2(MakePmid());
  NodeId sym_node_id_2(sym_node2.name());

  while (NodeId::CloserToTarget(closest_vaults.at(1).node_id, sym_node_id_2, sym_node_id_1)) {
    sym_node2 = MakePmid();
    sym_node_id_2 = NodeId(sym_node2.name());
  }

  env_->AddNode(sym_node2, true);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
