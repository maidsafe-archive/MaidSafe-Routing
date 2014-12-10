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

#include "maidsafe/passport/passport.h"
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
    assert(env_->ClientIndex() == kServerSize && "No server should have been added in test");
    EXPECT_TRUE(env_->RestoreComposition());
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
    EXPECT_TRUE(env_->SendDirect(1));
    env_->ClearMessages();
  }
  {
    //  SendGroup
    unsigned int random_node(static_cast<unsigned int>(env_->RandomVaultIndex()));
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
    unsigned int random_client(static_cast<unsigned int>(env_->RandomClientIndex()));
    EXPECT_TRUE(env_->SendGroup(target_id, 1, random_client));
    for (const auto& group_id : group_Ids)
      EXPECT_EQ(1, env_->nodes_.at(env_->NodeIndex(group_id))->MessagesSize());
    env_->ClearMessages();

    // SendGroup RandomId
    target_id = NodeId(RandomString(NodeId::kSize));
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
  EXPECT_TRUE(env_->SendDirect(2));

  // Signature 2
  EXPECT_TRUE(env_->SendDirect(env_->RandomVaultNode()->node_id()));

  EXPECT_TRUE(env_->SendDirect(env_->RandomClientNode()->node_id(), kExpectClient));

  EXPECT_FALSE(env_->SendDirect(NodeId(RandomString(NodeId::kSize)), kExpectDoesNotExist));

  // Signature 3
  EXPECT_TRUE(env_->SendDirect(env_->RandomVaultNode(), env_->RandomVaultNode()->node_id()));

  unsigned int random_vault(env_->RandomVaultIndex());
  unsigned int random_client(env_->RandomClientIndex());
  EXPECT_TRUE(env_->SendDirect(env_->nodes_[random_vault], env_->nodes_[random_client]->node_id(),
                               kExpectClient));

  EXPECT_TRUE(env_->SendDirect(env_->RandomClientNode(), env_->RandomVaultNode()->node_id()));

  unsigned int another_random_client(env_->RandomClientIndex());
  if (random_client == another_random_client)
    EXPECT_TRUE(env_->SendDirect(env_->nodes_[random_client],
                                 env_->nodes_[another_random_client]->node_id(), kExpectClient));
  else
    EXPECT_FALSE(env_->SendDirect(env_->nodes_[random_client],
                                  env_->nodes_[another_random_client]->node_id(), kExpectClient));
}

TEST_F(RoutingNetworkTest, FUNC_SanityCheckSendGroup) {
  EXPECT_TRUE(env_->SendGroup(NodeId(RandomString(NodeId::kSize)), 1 + RandomUint32() % 5,
                              env_->RandomVaultIndex()));

  EXPECT_TRUE(env_->SendGroup(NodeId(RandomString(NodeId::kSize)), 1 + RandomUint32() % 5,
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
  EXPECT_FALSE(env_->SendDirect(NodeId(RandomString(NodeId::kSize)), kExpectDoesNotExist));
  EXPECT_TRUE(env_->SendDirect(env_->nodes_[env_->RandomVaultIndex()]->node_id()));
}

TEST_F(RoutingNetworkTest, FUNC_ClientSend) { EXPECT_TRUE(env_->SendDirect(1)); }

TEST_F(RoutingNetworkTest, FUNC_SendMulti) {
  boost::progress_timer t;

#if defined(MAIDSAFE_WIN32) && !defined(NDEBUG)
  EXPECT_TRUE(env_->SendDirect(3));
#else
  EXPECT_TRUE(env_->SendDirect(5));
#endif
  std::cout << "Time taken for test : " << t.elapsed();
}

TEST_F(RoutingNetworkTest, FUNC_ClientSendMulti) { EXPECT_TRUE(env_->SendDirect(3)); }

TEST_F(RoutingNetworkTest, FUNC_SendToGroup) {
  unsigned int message_count(10), receivers_message_count(0);
  size_t last_index(kServerSize - 1);
  NodeId dest_id(env_->nodes_[last_index]->node_id());

  env_->ClearMessages();
  boost::progress_timer t;
  EXPECT_TRUE(env_->SendGroup(dest_id, message_count));
  std::cout << "Time taken for test : " << t.elapsed();
  for (size_t index = 0; index != (last_index); ++index)
    receivers_message_count += static_cast<unsigned int>(env_->nodes_.at(index)->MessagesSize());

  EXPECT_EQ(0, env_->nodes_[last_index]->MessagesSize())
      << "Not expected message at Node : "
      << HexSubstr(env_->nodes_[last_index]->node_id().string());
  EXPECT_EQ(message_count * (Parameters::group_size), receivers_message_count);
}

TEST_F(RoutingNetworkTest, FUNC_SendToGroupSelfId) {
  unsigned int message_count(10), receivers_message_count(0);
  env_->ClearMessages();
  std::vector<std::future<std::unique_ptr<testing::AssertionResult>>> futures;
  auto timeout(Parameters::default_response_timeout);
  Parameters::default_response_timeout *= message_count;
  for (unsigned int dest_index(0); dest_index < kServerSize; ++dest_index) {
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
            futures.begin(), futures.end(),
            [](std::future<std::unique_ptr<testing::AssertionResult>>& future_bool)->bool {
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
    receivers_message_count += static_cast<unsigned int>(node->MessagesSize());
    node->ClearMessages();
  }
  EXPECT_EQ(message_count * (Parameters::group_size) * kServerSize, receivers_message_count);
  LOG(kVerbose) << "Total message received count : " << receivers_message_count;
  Parameters::default_response_timeout = timeout;
}

TEST_F(RoutingNetworkTest, FUNC_SendToGroupClientSelfId) {
  size_t message_count(100), receivers_message_count(0);
#if defined(MAIDSAFE_WIN32) && !defined(NDEBUG)
    message_count = 50;
#endif

  size_t client_index(env_->RandomClientIndex());

  size_t last_index(env_->nodes_.size());
  NodeId dest_id(env_->nodes_[client_index]->node_id());

  auto timeout(Parameters::default_response_timeout);
  Parameters::default_response_timeout *= Parameters::group_size * message_count;

  env_->ClearMessages();
  EXPECT_TRUE(env_->SendGroup(dest_id, message_count,
                              static_cast<unsigned int>(client_index)));  // from client
  for (size_t index = 0; index != (last_index); ++index)
    receivers_message_count += static_cast<unsigned int>(env_->nodes_.at(index)->MessagesSize());

  EXPECT_EQ(0, env_->nodes_[client_index]->MessagesSize())
      << "Not expected message at Node : "
      << HexSubstr(env_->nodes_[client_index]->node_id().string());
  EXPECT_EQ(message_count * (Parameters::group_size), receivers_message_count);
  Parameters::default_response_timeout = timeout;
}

TEST_F(RoutingNetworkTest, FUNC_SendToGroupInHybridNetwork) {
  unsigned int message_count(1), receivers_message_count(0);
  LOG(kVerbose) << "Network created";
  size_t last_index(env_->nodes_.size() - 1);
  NodeId dest_id(env_->nodes_[last_index]->node_id());

  env_->ClearMessages();
  EXPECT_TRUE(env_->SendGroup(dest_id, message_count));
  for (size_t index = 0; index != (last_index); ++index)
    receivers_message_count += static_cast<unsigned int>(env_->nodes_.at(index)->MessagesSize());

  EXPECT_EQ(0, env_->nodes_[last_index]->MessagesSize())
      << "Not expected message at Node : "
      << HexSubstr(env_->nodes_[last_index]->node_id().string());
  EXPECT_EQ(message_count * (Parameters::group_size), receivers_message_count);
}

TEST_F(RoutingNetworkTest, FUNC_SendToGroupRandomId) {
  unsigned int message_count(200), receivers_message_count(0);
#if defined(MAIDSAFE_WIN32) && !defined(NDEBUG)
  message_count = 50;
#endif

  env_->ClearMessages();
  std::vector<std::future<std::unique_ptr<testing::AssertionResult>>> futures;

  auto timeout(Parameters::default_response_timeout);
  Parameters::default_response_timeout *= message_count * Parameters::group_size;
  for (unsigned int index = 0; index < message_count; ++index) {
    futures.emplace_back(std::async(std::launch::async, [this]() {
      return std::move(std::unique_ptr<testing::AssertionResult>(
          new testing::AssertionResult(env_->SendGroup(NodeId(RandomString(NodeId::kSize)), 1))));
    }));
    Sleep(std::chrono::milliseconds(100));
  }
  while (!futures.empty()) {
    futures.erase(
        std::remove_if(
            futures.begin(), futures.end(),
            [](std::future<std::unique_ptr<testing::AssertionResult>>& future_bool)->bool {
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
    receivers_message_count += static_cast<unsigned int>(node->MessagesSize());
    node->ClearMessages();
  }
  EXPECT_EQ(message_count * (Parameters::group_size), receivers_message_count);
  LOG(kVerbose) << "Total message received count : " << message_count * (Parameters::group_size);
  Parameters::default_response_timeout = timeout;
}

TEST_F(RoutingNetworkTest, FUNC_NonMutatingClientSendToGroupRandomId) {
  unsigned int message_count(100), receivers_message_count(0);
#if defined(MAIDSAFE_WIN32) && !defined(NDEBUG)
    message_count = 50;
#endif

  env_->ClearMessages();
  std::vector<std::future<std::unique_ptr<testing::AssertionResult>>> futures;

  auto timeout(Parameters::default_response_timeout);
  Parameters::default_response_timeout *= message_count;
  env_->AddMutatingClient(false);
  assert(env_->nodes_.size() - 1 < std::numeric_limits<unsigned int>::max());

  for (unsigned int index = 0; index < message_count; ++index) {
    futures.emplace_back(std::async(std::launch::async, [this]() {
      return std::move(std::unique_ptr<testing::AssertionResult>(new testing::AssertionResult(
          env_->SendGroup(NodeId(RandomString(NodeId::kSize)), 1,
                          static_cast<unsigned int>(env_->nodes_.size() - 1)))));
    }));
    Sleep(std::chrono::milliseconds(10));
  }
  while (!futures.empty()) {
    futures.erase(
        std::remove_if(
            futures.begin(), futures.end(),
            [](std::future<std::unique_ptr<testing::AssertionResult>>& future_bool)->bool {
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
    receivers_message_count += static_cast<unsigned int>(node->MessagesSize());
    node->ClearMessages();
  }

  EXPECT_EQ(message_count * (Parameters::group_size), receivers_message_count);
  LOG(kVerbose) << "Total message received count : " << message_count * (Parameters::group_size);
  Parameters::default_response_timeout = timeout;
}

TEST_F(RoutingNetworkTest, FUNC_NonMutatingClientSendToGroupExistingId) {
  unsigned int message_count(100), receivers_message_count(0);
#if defined(MAIDSAFE_WIN32) && !defined(NDEBUG)
    message_count = 50;
#endif
  env_->ClearMessages();
  std::vector<std::future<std::unique_ptr<testing::AssertionResult>>> futures;
  auto timeout(Parameters::default_response_timeout);
  Parameters::default_response_timeout *= message_count;

  size_t initial_network_size(env_->nodes_.size());
  env_->AddMutatingClient(false);
  assert(env_->nodes_.size() - 1 < std::numeric_limits<unsigned int>::max());

  for (unsigned int index(0); index < message_count; ++index) {
    int group_id_index = index % initial_network_size;  // all other nodes
    NodeId group_id(env_->nodes_[group_id_index]->node_id());
    futures.emplace_back(std::async(std::launch::async, [this, group_id]() {
      return std::move(std::unique_ptr<testing::AssertionResult>(new testing::AssertionResult(
          env_->SendGroup(group_id, 1, static_cast<unsigned int>(env_->nodes_.size() - 1)))));
    }));
    Sleep(std::chrono::milliseconds(10));
  }
  while (!futures.empty()) {
    futures.erase(
        std::remove_if(
            futures.begin(), futures.end(),
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
    receivers_message_count += static_cast<unsigned int>(node->MessagesSize());
    node->ClearMessages();
  }

  EXPECT_EQ(message_count * (Parameters::group_size), receivers_message_count);
  LOG(kVerbose) << "Total message received count : " << message_count * (Parameters::group_size);
  Parameters::default_response_timeout = timeout;
}

TEST_F(RoutingNetworkTest, FUNC_JoinWithSameId) {
  auto maid(passport::CreateMaidAndSigner().first);
  env_->AddNode(maid);
  env_->AddNode(maid);
  env_->AddNode(maid);
  env_->AddNode(maid);
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

TEST_F(RoutingNetworkTest, FUNC_IsConnectedVault) {
  ASSERT_LE(env_->ClientIndex(), static_cast<size_t>(Parameters::max_routing_table_size + 1));

  // Vault checks vault id - expect true
  for (unsigned int i(0); i < env_->ClientIndex(); ++i) {
    for (unsigned int j(0); j < env_->ClientIndex(); ++j) {
      if (i != j) {
        EXPECT_TRUE(env_->nodes_.at(i)->IsConnectedVault(env_->nodes_.at(j)->node_id()));
      }
    }
  }

  // Vault or Client checks client id - expect false
  for (unsigned int i(0); i < env_->nodes_.size(); ++i) {
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
      EXPECT_TRUE(env_->nodes_.at(i)->IsConnectedVault(vault.id));
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
      int node_index(env_->NodeIndex(node_info.id));
      ASSERT_GE(node_index, 0);
      EXPECT_TRUE(env_->nodes_.at(node_index)->IsConnectedClient(client_id));
    }
  }

  // Vault checks vault id - expect false
  for (unsigned int i(0); i < env_->ClientIndex(); ++i) {
    for (unsigned int j(0); j < env_->ClientIndex(); ++j) {
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
    non_existing_id = NodeId(RandomString(NodeId::kSize));
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

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
