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

namespace args = std::placeholders;

namespace maidsafe {

namespace routing {

namespace test {

template <typename T>
typename std::vector<T>::const_iterator Find(const T& t, const std::vector<T>& v) {
  return std::find_if(v.begin(), v.end(), [&t] (const T& element) {
                                              return element == t;
                                            });
}

class RoutingNetworkTest : public testing::Test {
 public:
  RoutingNetworkTest(void) : env_(NodesEnvironment::g_environment()) {}

  void SetUp() {
    EXPECT_TRUE(env_->RestoreComposition());
    EXPECT_TRUE(env_->WaitForHealthToStabilise());
  }

  void TearDown() {
    EXPECT_TRUE(env_->RestoreComposition());
  }

 protected:
  // Send messages from each source to each destination
  testing::AssertionResult Send(const size_t& messages) {
    NodeId  group_id;
    size_t message_id(0), client_size(0), non_client_size(0);
    std::set<size_t> received_ids;
    for (auto node : env_->nodes_)
      (node->IsClient()) ? client_size++ : non_client_size++;

    LOG(kVerbose) << "Network node size: " << client_size << " : " << non_client_size;

    size_t messages_count(0),
        expected_messages(non_client_size * (non_client_size - 1 + client_size) * messages);
    std::mutex mutex;
    std::condition_variable cond_var;
    for (size_t index = 0; index < messages; ++index) {
      for (auto source_node : env_->nodes_) {
        for (auto dest_node : env_->nodes_) {
          auto callable = [&](const std::vector<std::string> &message) {
              if (message.empty())
                return;
              std::lock_guard<std::mutex> lock(mutex);
              messages_count++;
              std::string data_id(message.at(0).substr(message.at(0).find(">:<") + 3,
                  message.at(0).find("<:>") - 3 - message.at(0).find(">:<")));
              received_ids.insert(boost::lexical_cast<size_t>(data_id));
              LOG(kVerbose) << "ResponseHandler .... " << messages_count << " msg_id: "
                            << data_id;
              if (messages_count == expected_messages) {
                cond_var.notify_one();
                LOG(kVerbose) << "ResponseHandler .... DONE " << messages_count;
              }
            };
          if (source_node->node_id() != dest_node->node_id()) {
            std::string data(RandomAlphaNumericString(512 * 2^10));
            {
              std::lock_guard<std::mutex> lock(mutex);
              data = boost::lexical_cast<std::string>(++message_id) + "<:>" + data;
            }
            assert(!data.empty() && "Send Data Empty !");
            source_node->Send(NodeId(dest_node->node_id()), NodeId(), data, callable,
                boost::posix_time::seconds(static_cast<long>(env_->nodes_.size())),  // NOLINT (Fraser)
                DestinationType::kDirect, false);
            Sleep(boost::posix_time::microseconds(21));
          }
        }
      }
    }

    std::unique_lock<std::mutex> lock(mutex);
    bool result = cond_var.wait_for(lock, std::chrono::seconds(20),
        [&]()->bool {
          LOG(kInfo) << " message count " << messages_count << " expected "
                     << expected_messages << "\n";
          return messages_count == expected_messages;
        });
    EXPECT_TRUE(result);
    if (!result) {
      for (size_t id(1); id <= expected_messages; ++id) {
        auto iter = received_ids.find(id);
        if (iter == received_ids.end())
          LOG(kVerbose) << "missing id: " << id;
      }
      return testing::AssertionFailure() << "Send operarion timed out: "
                                         << expected_messages - messages_count
                                         << " failed to reply.";
    }
    return testing::AssertionSuccess();
  }

  testing::AssertionResult GroupSend(const NodeId& node_id,
                                     const size_t& messages,
                                     uint16_t source_index = 0) {
    assert(static_cast<long>(10 * messages) > 0);  // NOLINT (Fraser)
    size_t messages_count(0), expected_messages(messages);
    std::string data(RandomAlphaNumericString((2 ^ 10) * 256));

    std::mutex mutex;
    std::condition_variable cond_var;
    for (size_t index = 0; index < messages; ++index) {
      auto callable = [&] (const std::vector<std::string> message) {
                        if (message.empty())
                          return;
                        std::lock_guard<std::mutex> lock(mutex);
                        messages_count++;
                        LOG(kVerbose) << "ResponseHandler .... " << messages_count;
                        if (messages_count == expected_messages) {
                          cond_var.notify_one();
                          LOG(kVerbose) << "ResponseHandler .... DONE " << messages_count;
                        }
                      };
      env_->nodes_[source_index]->Send(node_id,
                                       NodeId(),
                                       data,
                                       callable,
                                       boost::posix_time::seconds(static_cast<long>(10 * messages)),  // NOLINT (Fraser)
                                       DestinationType::kGroup,
                                       false);
    }

    std::unique_lock<std::mutex> lock(mutex);
    bool result = cond_var.wait_for(lock,
                                    std::chrono::seconds(10 * messages + 5),
                                    [&] ()->bool {
                                      LOG(kInfo) << " message count " << messages_count
                                                 << " expected " << expected_messages << "\n";
                                      return messages_count == expected_messages;
                                    });
    EXPECT_TRUE(result);
    if (!result) {
      return testing::AssertionFailure() << "Send operarion timed out: "
                                         << expected_messages - messages_count
                                         << " failed to reply.";
    }
    return testing::AssertionSuccess();
  }

  testing::AssertionResult Send(const NodeId& node_id) {
    std::set<size_t> received_ids;
    std::mutex mutex;
    std::condition_variable cond_var;
    size_t messages_count(0), message_id(0), expected_messages(0);
    auto node(std::find_if(env_->nodes_.begin(), env_->nodes_.end(),
                           [&](const std::shared_ptr<GenericNode> node) {
                             return node->node_id() == node_id;
                           }));
    if ((node != env_->nodes_.end()) && !((*node)->IsClient()))
      expected_messages = env_->nodes_.size() - 1;
    for (auto source_node : env_->nodes_) {
      auto callable = [&](const std::vector<std::string> &message) {
        if (message.empty())
          return;
        std::lock_guard<std::mutex> lock(mutex);
        messages_count++;
        std::string data_id(message.at(0).substr(message.at(0).find(">:<") + 3,
            message.at(0).find("<:>") - 3 - message.at(0).find(">:<")));
        received_ids.insert(boost::lexical_cast<size_t>(data_id));
        LOG(kVerbose) << "ResponseHandler .... " << messages_count << " msg_id: "
                      << data_id;
        if (messages_count == expected_messages) {
          cond_var.notify_one();
          LOG(kVerbose) << "ResponseHandler .... DONE " << messages_count;
        }
      };
      if (source_node->node_id() != node_id) {
          std::string data(RandomAlphaNumericString((RandomUint32() % 255 + 1) * 2^10));
          {
            std::lock_guard<std::mutex> lock(mutex);
            data = boost::lexical_cast<std::string>(++message_id) + "<:>" + data;
          }
          source_node->Send(node_id, NodeId(), data, callable,
              boost::posix_time::seconds(12), DestinationType::kDirect, false);
      }
    }

    std::unique_lock<std::mutex> lock(mutex);
    bool result = cond_var.wait_for(lock, std::chrono::seconds(20),
        [&]()->bool {
          LOG(kInfo) << " message count " << messages_count << " expected "
                     << expected_messages << "\n";
          return messages_count == expected_messages;
        });
    EXPECT_TRUE(result);
    if (!result) {
      for (size_t id(1); id <= expected_messages; ++id) {
        auto iter = received_ids.find(id);
        if (iter == received_ids.end())
          LOG(kVerbose) << "missing id: " << id;
      }
      return testing::AssertionFailure() << "Send operarion timed out: "
                                         << expected_messages - messages_count
                                         << " failed to reply.";
    }
    return testing::AssertionSuccess();
  }

  testing::AssertionResult Send(std::shared_ptr<GenericNode> source_node,
                                const NodeId& node_id,
                                bool no_response_expected = false) {
    std::mutex mutex;
    std::condition_variable cond_var;
    size_t messages_count(0), expected_messages(0);
    ResponseFunctor callable;
    if (!no_response_expected) {
      expected_messages = std::count_if(env_->nodes_.begin(), env_->nodes_.end(),
          [=](const std::shared_ptr<GenericNode> node)->bool {
            return node_id ==  node->node_id();
          });

      callable = [&](const std::vector<std::string> &message) {
        if (message.empty())
          return;
        std::lock_guard<std::mutex> lock(mutex);
        messages_count++;
        LOG(kVerbose) << "ResponseHandler .... " << messages_count;
        if (messages_count == expected_messages) {
          cond_var.notify_one();
          LOG(kVerbose) << "ResponseHandler .... DONE " << messages_count;
        }
      };
    }
    std::string data(RandomAlphaNumericString(512 * 2^10));
    assert(!data.empty() && "Send Data Empty !");
    source_node->Send(node_id, NodeId(), data, callable,
                      boost::posix_time::seconds(12), DestinationType::kDirect, false);

    if (!no_response_expected) {
      std::unique_lock<std::mutex> lock(mutex);
      bool result = cond_var.wait_for(lock, std::chrono::seconds(20),
          [&]()->bool {
            LOG(kInfo) << " message count " << messages_count << " expected "
                      << expected_messages << "\n";
            return messages_count == expected_messages;
          });
      EXPECT_TRUE(result);
      if (!result) {
        return testing::AssertionFailure() << "Send operarion timed out: "
                                           << expected_messages - messages_count
                                           << " failed to reply.";
      }
      return testing::AssertionSuccess();
    } else {
      return testing::AssertionSuccess();
    }
  }

  std::shared_ptr<GenericNetwork> env_;
};

TEST_F(RoutingNetworkTest, FUNC_SanityCheck) {
  {
    EXPECT_TRUE(Send(3));
    // This sleep is required for un-responded requests
    Sleep(boost::posix_time::seconds(static_cast<long>(env_->nodes_.size() + 1)));  // NOLINT (Fraser)
    env_->ClearMessages();
  }
  {
    //  GroupSend
    uint16_t random_node(RandomUint32() % kServerSize);
    NodeId target_id(env_->nodes_[random_node]->node_id());
    std::vector<NodeId> group_Ids(env_->GetGroupForId(target_id));
    EXPECT_TRUE(GroupSend(target_id, 1));
    for (auto& group_id : group_Ids)
      EXPECT_EQ(1, env_->nodes_.at(env_->NodeIndex(group_id))->MessagesSize());
    env_->ClearMessages();

    // GroupSend SelfId
    EXPECT_TRUE(GroupSend(target_id, 1, random_node));
    for (auto& group_id : group_Ids)
      EXPECT_EQ(1, env_->nodes_.at(env_->NodeIndex(group_id))->MessagesSize());
    env_->ClearMessages();

    // Client groupsend
    EXPECT_TRUE(GroupSend(target_id, 1, kNetworkSize - 1));
    for (auto& group_id : group_Ids)
      EXPECT_EQ(1, env_->nodes_.at(env_->NodeIndex(group_id))->MessagesSize());
    env_->ClearMessages();

    // GroupSend RandomId
    target_id = NodeId(NodeId::kRandomId);
    group_Ids = env_->GetGroupForId(target_id);
    EXPECT_TRUE(GroupSend(target_id, 1));
    for (auto& group_id : group_Ids)
      EXPECT_EQ(1, env_->nodes_.at(env_->NodeIndex(group_id))->MessagesSize());
    env_->ClearMessages();
  }
  {
    // Join client with same Id
    env_->AddNode(true, env_->nodes_[env_->RandomClientIndex()]->node_id());

    // Send to client with same Id
    EXPECT_TRUE(Send(env_->nodes_[kNetworkSize],
                     env_->nodes_[kNetworkSize]->node_id(),
                     true));
    env_->ClearMessages();
  }
  {
    // Anonymous join
    env_->AddNode(true, NodeId(), true);

    // Anonymous group send
    NodeId target_id(NodeId::kRandomId);
    std::vector<NodeId> group_Ids(env_->GetGroupForId(target_id));
    EXPECT_TRUE(GroupSend(target_id, 1, static_cast<uint16_t>(env_->nodes_.size() - 1)));
    for (auto& group_id : group_Ids)
      EXPECT_EQ(1, env_->nodes_.at(env_->NodeIndex(group_id))->MessagesSize());
    env_->ClearMessages();
  }
}

// Test disabled as already get covered by global environment setup
TEST_F(RoutingNetworkTest, DISABLED_FUNC_SetupNetwork) {
  env_->SetUpNetwork(kServerSize);
}

// Test disabled as already get covered by global environment setup
TEST_F(RoutingNetworkTest, DISABLED_FUNC_SetupSingleClientHybridNetwork) {
  env_->SetUpNetwork(kServerSize, 1);
}

// Test disabled as already get covered by global environment setup
TEST_F(RoutingNetworkTest, DISABLED_FUNC_SetupHybridNetwork) {
  env_->SetUpNetwork(kServerSize, kClientSize);
}

TEST_F(RoutingNetworkTest, FUNC_Send) {
  EXPECT_TRUE(Send(1));
}

TEST_F(RoutingNetworkTest, FUNC_SendToNonExistingNode) {
  EXPECT_TRUE(Send(NodeId(NodeId::kRandomId)));
  EXPECT_TRUE(Send(env_->nodes_[RandomUint32() % kNetworkSize]->node_id()));
}

TEST_F(RoutingNetworkTest, FUNC_ClientSend) {
  EXPECT_TRUE(Send(1));
  Sleep(boost::posix_time::seconds(21));  // This sleep is required for un-responded requests
}

TEST_F(RoutingNetworkTest, FUNC_SendMulti) {
  EXPECT_TRUE(Send(5));
}

TEST_F(RoutingNetworkTest, DISABLED_FUNC_ExtendedSendMulti) {
  uint16_t loop(100);
  while (loop-- > 0) {
    EXPECT_TRUE(Send(40));
    env_->ClearMessages();
  }
}

TEST_F(RoutingNetworkTest, FUNC_ClientSendMulti) {
  EXPECT_TRUE(Send(3));
// This sleep is required for un-responded requests
  Sleep(boost::posix_time::seconds(static_cast<long>(env_->nodes_.size() + 1)));  // NOLINT (Fraser)
}

TEST_F(RoutingNetworkTest, FUNC_SendToGroup) {
  uint16_t message_count(10), receivers_message_count(0);
  size_t last_index(kServerSize - 1);
  NodeId dest_id(env_->nodes_[last_index]->node_id());

  env_->ClearMessages();
  EXPECT_TRUE(GroupSend(dest_id, message_count));
  for (size_t index = 0; index != (last_index); ++index)
    receivers_message_count += static_cast<uint16_t>(env_->nodes_.at(index)->MessagesSize());

  EXPECT_EQ(0, env_->nodes_[last_index]->MessagesSize())
      << "Not expected message at Node : "
      << HexSubstr(env_->nodes_[last_index]->node_id().string());
  EXPECT_EQ(message_count * (Parameters::node_group_size), receivers_message_count);
}

TEST_F(RoutingNetworkTest, DISABLED_FUNC_ExtendedSendToGroup) {
  uint16_t message_count(10), receivers_message_count(0);
  size_t last_index(env_->nodes_.size() - 1);
  NodeId dest_id(env_->nodes_[last_index]->node_id());

  uint16_t loop(100);
  while (loop-- > 0) {
    EXPECT_TRUE(GroupSend(dest_id, message_count));
    for (size_t index = 0; index != (last_index); ++index)
      receivers_message_count += static_cast<uint16_t>(env_->nodes_.at(index)->MessagesSize());

    EXPECT_EQ(0, env_->nodes_[last_index]->MessagesSize())
          << "Not expected message at Node : "
          << HexSubstr(env_->nodes_[last_index]->node_id().string());
    EXPECT_EQ(message_count * (Parameters::node_group_size), receivers_message_count);
    receivers_message_count = 0;
    env_->ClearMessages();
  }
}

TEST_F(RoutingNetworkTest, FUNC_SendToGroupSelfId) {
  uint16_t message_count(10), receivers_message_count(0);
  size_t last_index(kServerSize - 1);
  NodeId dest_id(env_->nodes_[0]->node_id());

  env_->ClearMessages();
  EXPECT_TRUE(GroupSend(dest_id, message_count));
  for (size_t index = 0; index != (last_index); ++index)
    receivers_message_count += static_cast<uint16_t>(env_->nodes_.at(index)->MessagesSize());

  EXPECT_EQ(0, env_->nodes_[0]->MessagesSize())
        << "Not expected message at Node : "
        << HexSubstr(env_->nodes_[0]->node_id().string());
  EXPECT_EQ(message_count * (Parameters::node_group_size), receivers_message_count);
}

TEST_F(RoutingNetworkTest, FUNC_SendToGroupClientSelfId) {
  uint16_t message_count(100), receivers_message_count(0);

  size_t client_index(env_->RandomClientIndex());

  size_t last_index(env_->nodes_.size());
  NodeId dest_id(env_->nodes_[client_index]->node_id());

  env_->ClearMessages();
  EXPECT_TRUE(GroupSend(dest_id, message_count, client_index));  // from client
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
  EXPECT_TRUE(GroupSend(dest_id, message_count));
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
    EXPECT_TRUE(GroupSend(NodeId(NodeId::kRandomId), 1));
    for (auto node : env_->nodes_) {
      receivers_message_count += static_cast<uint16_t>(node->MessagesSize());
      node->ClearMessages();
    }
  }
  EXPECT_EQ(message_count * (Parameters::node_group_size), receivers_message_count);
  LOG(kVerbose) << "Total message received count : "
                << message_count * (Parameters::node_group_size);
}

TEST_F(RoutingNetworkTest, DISABLED_FUNC_ExtendedSendToGroupRandomId) {
  uint16_t message_count(200), receivers_message_count(0);
  uint16_t loop(10);
  while (loop-- > 0) {
    for (int index = 0; index < message_count; ++index) {
      NodeId random_id(NodeId::kRandomId);
      std::vector<NodeId> groupd_ids(env_->GroupIds(random_id));
      EXPECT_TRUE(GroupSend(random_id, 1));
      for (auto node : env_->nodes_) {
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
    env_->ClearMessages();
  }
}

TEST_F(RoutingNetworkTest, FUNC_AnonymousSendToGroupRandomId) {
  uint16_t message_count(200), receivers_message_count(0);
  env_->AddNode(true, NodeId(), true);
  assert(env_->nodes_.size() - 1 < std::numeric_limits<uint16_t>::max());
  for (int index = 0; index < message_count; ++index) {
    EXPECT_TRUE(GroupSend(NodeId(NodeId::kRandomId), 1,
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
    EXPECT_TRUE(GroupSend(group_id, 1, static_cast<uint16_t>(env_->nodes_.size() - 1)));
    for (auto node : env_->nodes_) {
      receivers_message_count += static_cast<uint16_t>(node->MessagesSize());
      node->ClearMessages();
    }
  }
  EXPECT_EQ(message_count * (Parameters::node_group_size), receivers_message_count);
  LOG(kVerbose) << "Total message received count : "
                << message_count * (Parameters::node_group_size);
}

TEST_F(RoutingNetworkTest, FUNC_JoinAfterBootstrapLeaves) {
  LOG(kVerbose) << "Network Size " << env_->nodes_.size();
  Sleep(boost::posix_time::seconds(10));
  LOG(kVerbose) << "RIse ";
  env_->AddNode(false, NodeId());
//  env_->AddNode(true, NodeId());
}

// This test produces the recursive call.
TEST_F(RoutingNetworkTest, FUNC_RecursiveCall) {
  for (int index(0); index < 8; ++index)
    env_->AddNode(false, GenerateUniqueRandomId(20));
  env_->AddNode(true, GenerateUniqueRandomId(40));
  env_->AddNode(false, GenerateUniqueRandomId(35));
  env_->AddNode(false, GenerateUniqueRandomId(30));
  env_->AddNode(false, GenerateUniqueRandomId(25));
  env_->AddNode(false, GenerateUniqueRandomId(20));
  env_->AddNode(false, GenerateUniqueRandomId(10));
  env_->AddNode(true, GenerateUniqueRandomId(10));
  env_->PrintRoutingTables();
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
    EXPECT_TRUE(Send(env_->nodes_[kNetworkSize],
                     env_->nodes_[kNetworkSize]->node_id(),
                     true));
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
  uint32_t new_index(env_->nodes_.size());
  env_->AddNode(true, node_id);
  size_t size(0);

  env_->ClearMessages();
  EXPECT_TRUE(Send(env_->nodes_[new_index],
                   node_id,
                   true));
  Sleep(boost::posix_time::seconds(1));
  for (auto node : env_->nodes_) {
    size += node->MessagesSize();
  }
  EXPECT_EQ(2, size);
}

TEST_F(RoutingNetworkTest, FUNC_NodeRemoved) {
  size_t random_index(RandomUint32() % env_->nodes_.size());
  NodeInfo removed_node_info(env_->nodes_[random_index]->GetRemovableNode());
  EXPECT_GE(removed_node_info.bucket, 510);
}

TEST_F(RoutingNetworkTest, FUNC_GetRandomExistingNode) {
  uint32_t collisions(0);
  uint32_t kChoseIndex((RandomUint32() % kNetworkSize - 2) + 2);
  EXPECT_TRUE(Send(1));
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

// TODO(Alison) - move churn tests to separate file/target, and run each with its own network.
// TEST_F(RoutingNetworkTest, FUNC_BasicNetworkChurn) {
//  // Existing vault node ids
//  std::vector<NodeId> existing_client_node_ids, existing_vault_node_ids;
//  for (size_t i(1); i < env_->nodes_.size(); ++i) {
//    if (env_->nodes_[i]->IsClient())
//      existing_client_node_ids.push_back(env_->nodes_[i]->node_id());
//    else
//      existing_vault_node_ids.push_back(env_->nodes_[i]->node_id());
//  }

//  for (int n(1); n < 51; ++n) {
//    if (n % 2 == 0) {
//      NodeId new_node(NodeId::kRandomId);
//      while (std::find_if(existing_vault_node_ids.begin(),
//                          existing_vault_node_ids.end(),
//                          [&new_node] (const NodeId& element) { return element == new_node; }) !=
//             existing_vault_node_ids.end()) {
//        new_node = NodeId(NodeId::kRandomId);
//      }
//      env_->AddNode(false, new_node);
//      existing_vault_node_ids.push_back(new_node);
//      Sleep(boost::posix_time::milliseconds(500 + RandomUint32() % 200));
//    }

//    if (n % 3 == 0) {
//      std::random_shuffle(existing_vault_node_ids.begin(), existing_vault_node_ids.end());
//      env_->RemoveNode(existing_vault_node_ids.back());
//      existing_vault_node_ids.pop_back();
//      Sleep(boost::posix_time::milliseconds(500 + RandomUint32() % 200));
//    }
//  }
// }

// TEST_F(RoutingNetworkTest, FUNC_MessagingNetworkChurn) {
//  const size_t vault_network_size(env_->ClientIndex());
//  const size_t clients_in_network(env_->nodes_.size() - env_->ClientIndex());

//  std::vector<NodeId> existing_node_ids;
//  for (auto& node : env_->nodes_)
//    existing_node_ids.push_back(node->node_id());
//  LOG(kInfo) << "After harvesting node ids\n\n\n\n";

//  std::vector<NodeId> new_node_ids;
//  const size_t up_count(vault_network_size / 3), down_count(vault_network_size / 5);
//  size_t downed(0);
//  while (new_node_ids.size() < up_count) {
//    NodeId new_id(NodeId::kRandomId);
//    auto itr(Find(new_id, existing_node_ids));
//    if (itr == existing_node_ids.end())
//      new_node_ids.push_back(new_id);
//  }
//  LOG(kInfo) << "After generating new ids\n\n\n\n";

//  // Start thread for messaging between clients and clients to groups
//  std::string message(RandomString(4096));
//  volatile bool run(true);
//  auto messaging_handle = std::async(std::launch::async,
//                                     [=, &run] {
//                                       LOG(kInfo) << "Before messaging loop";
//                                       while (run) {
//                                         GenericNetwork::NodePtr sender_client(
//                                            env_->RandomClientNode());
//                                         GenericNetwork::NodePtr receiver_client(
//                                            env_->RandomClientNode());
//                                         GenericNetwork::NodePtr vault_node(
//                                            env_->RandomVaultNode());
//                                         // Choose random client nodes for direct message
//                                         sender_client->Send(receiver_client->node_id(), NodeId(),
//                                                             message, nullptr,
//                                                             boost::posix_time::seconds(2),
//                                                             DestinationType::kDirect,
//                                                             false);
//                                         // Choose random client for group message to random env
//                                         sender_client->Send(NodeId(NodeId::kRandomId), NodeId(),
//                                                             message, nullptr,
//                                                             boost::posix_time::seconds(2),
//                                                             DestinationType::kGroup,
//                                                             false);


//                                         // Choose random vault for group message to random env
//                                         vault_node->Send(NodeId(NodeId::kRandomId), NodeId(),
//                                                          message, nullptr,
//                                                          boost::posix_time::seconds(2),
//                                                          DestinationType::kGroup,
//                                                          false);
//                                         // Wait before going again
//                                         Sleep(boost::posix_time::milliseconds(900 +
//                                                                               RandomUint32() %
//                                                                               200));
//                                         LOG(kInfo) << "Ran messaging iteration";
//                                       }
//                                       LOG(kInfo) << "After messaging loop";
//                                     });
//  LOG(kInfo) << "Started messaging thread\n\n\n\n";

//  // Start thread to bring down nodes
//  auto down_handle = std::async(std::launch::async,
//                                [=, &run, &down_count, &downed] {
//                                  while (run && downed < down_count) {
////                                    if (RandomUint32() % 5 == 0)
////                                      env_->RemoveRandomClient();
////                                    else
//                                      env_->RemoveRandomVault();
//                                      ++downed;
//                                    Sleep(boost::posix_time::seconds(10));
//                                  }
//                                });

//  // Start thread to bring up nodes
//  auto up_handle = std::async(std::launch::async,
//                              [=, &run, &new_node_ids] {
//                                while (run) {
//                                  if (new_node_ids.empty())
//                                    return;
////                                  if (RandomUint32() % 5 == 0)
////                                    env_->AddNode(true, new_node_ids.back());
////                                  else
//                                    env_->AddNode(false, new_node_ids.back());
//                                  new_node_ids.pop_back();
//                                  Sleep(boost::posix_time::seconds(3));
//                                }
//                              });

//  // Let stuff run for a while
//  down_handle.get();
//  up_handle.get();

//  // Stop all threads
//  run = false;
//  messaging_handle.get();

//  LOG(kInfo) << "\n\t Initial count of Vault nodes : " << vault_network_size
//             << "\n\t Initial count of client nodes : " << clients_in_network
//             << "\n\t Current count of nodes : " << env_->nodes_.size()
//             << "\n\t Up count of nodes : " << up_count
//             << "\n\t down_count count of nodes : " << down_count;
//  auto expected_current_size = vault_network_size + clients_in_network + up_count - down_count;
//  EXPECT_EQ(expected_current_size, env_->nodes_.size());
// }

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
