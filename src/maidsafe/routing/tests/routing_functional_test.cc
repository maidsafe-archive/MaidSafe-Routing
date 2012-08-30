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

#include "maidsafe/routing/tests/routing_network.h"
#include "maidsafe/rudp/nat_type.h"

namespace args = std::placeholders;

namespace maidsafe {

namespace routing {

namespace test {

class TestNode : public GenericNode {
 public:
  explicit TestNode(bool client_mode = false)
      : GenericNode(client_mode),
        messages_() {
    functors_.message_received = [&](const std::string& message, const NodeId&,
                                     ReplyFunctor reply_functor) {
        LOG(kInfo) << id_ << " -- Received:  message : " << message.substr(0, 10);
        std::lock_guard<std::mutex> guard(mutex_);
        messages_.push_back(message);
        reply_functor("Response to >:<" + message);
    };
    LOG(kVerbose) << "RoutingNode constructor";
  }

  TestNode(bool client_mode, const NodeInfoAndPrivateKey& node_info)
      : GenericNode(client_mode, node_info),
      messages_() {
    functors_.message_received = [&](const std::string& message, const NodeId&,
                                     ReplyFunctor reply_functor) {
      LOG(kInfo) << id_ << " -- Received: message : " << message.substr(0, 10);
      std::lock_guard<std::mutex> guard(mutex_);
      messages_.push_back(message);
      reply_functor("Response to " + message);
    };
    LOG(kVerbose) << "RoutingNode constructor";
  }

  TestNode(bool client_mode, const rudp::NatType& nat_type)
      : GenericNode(client_mode, nat_type),
      messages_() {
    functors_.message_received = [&](const std::string& message, const NodeId&,
                                     ReplyFunctor reply_functor) {
      LOG(kInfo) << id_ << " -- Received: message : " << message.substr(0, 10);
      std::lock_guard<std::mutex> guard(mutex_);
      messages_.push_back(message);
      reply_functor("Response to " + message);
    };
    LOG(kVerbose) << "RoutingNode constructor";
  }


  virtual ~TestNode() {}
  size_t MessagesSize() const { return messages_.size(); }

  void ClearMessages() {
    std::lock_guard<std::mutex> lock(mutex_);
    messages_.clear();
  }

 protected:
  std::vector<std::string> messages_;
};

template <typename NodeType>
class RoutingNetworkTest : public GenericNetwork<NodeType> {
 public:
  RoutingNetworkTest(void) : GenericNetwork<NodeType>() {}

 protected:
  // Send messages from each source to each destination
  testing::AssertionResult Send(const size_t& messages) {
    NodeId  group_id;
    size_t message_id(0), client_size(0), non_client_size(0);
    std::set<size_t> received_ids;
    for (auto node : this->nodes_)
      (node->IsClient()) ? client_size++ : non_client_size++;

    LOG(kVerbose) << "Network node size: " << client_size << " : " << non_client_size;

    size_t messages_count(0),
        expected_messages(non_client_size * (non_client_size - 1 + client_size) * messages);
    std::mutex mutex;
    std::condition_variable cond_var;
    for (size_t index = 0; index < messages; ++index) {
      for (auto source_node : this->nodes_) {
        for (auto dest_node : this->nodes_) {
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
            std::string data(RandomAlphaNumericString((RandomUint32() % 255 + 1) * 2^10));
            {
              std::lock_guard<std::mutex> lock(mutex);
              data = boost::lexical_cast<std::string>(++message_id) + "<:>" + data;
            }
            Sleep(boost::posix_time::millisec(50));
            source_node->Send(NodeId(dest_node->node_id()), NodeId(), data, callable,
                boost::posix_time::seconds(12), true, false);
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

  testing::AssertionResult GroupSend(const NodeId& node_id, const size_t& messages) {
    NodeId  group_id;
    size_t messages_count(0), expected_messages(messages);
    std::string data(RandomAlphaNumericString(2 ^ 10));

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
      this->nodes_[0]->Send(node_id, NodeId(), data, callable, boost::posix_time::seconds(10),
                            false, false);
    }

    std::unique_lock<std::mutex> lock(mutex);
    bool result = cond_var.wait_for(lock, std::chrono::seconds(15),
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
  }

  testing::AssertionResult Send(const NodeId& node_id) {
    std::set<size_t> received_ids;
    std::mutex mutex;
    std::condition_variable cond_var;
    size_t messages_count(0), message_id(0), expected_messages(0);
    auto node(std::find_if(this->nodes_.begin(), this->nodes_.end(),
                           [&](const std::shared_ptr<TestNode> node) {
                             return node->node_id() == node_id;
                           }));
    if ((node != this->nodes_.end()) && !((*node)->IsClient()))
      expected_messages = this->nodes_.size() - 1;
    for (auto source_node : this->nodes_) {
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
              boost::posix_time::seconds(12), true, false);
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

  testing::AssertionResult Send(std::shared_ptr<TestNode> source_node, const NodeId& node_id) {
    std::mutex mutex;
    std::condition_variable cond_var;
    size_t messages_count(0), expected_messages(0);
    auto node(std::find_if(this->nodes_.begin(), this->nodes_.end(),
                           [&](const std::shared_ptr<TestNode> node) {
                              return node->node_id() == node_id;
                           }));
    if ((node != this->nodes_.end()) && !((*node)->IsClient()))
      expected_messages = 1;
    auto callable = [&](const std::vector<std::string> &message) {
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
    if (source_node->node_id() != node_id) {
        std::string data(RandomAlphaNumericString((RandomUint32() % 255 + 1) * 2^10));
        source_node->Send(node_id, NodeId(), data, callable,
            boost::posix_time::seconds(12), true, false);
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
      return testing::AssertionFailure() << "Send operarion timed out: "
                                         << expected_messages - messages_count
                                         << " failed to reply.";
    }
    return testing::AssertionSuccess();
  }
};

TYPED_TEST_CASE_P(RoutingNetworkTest);

TYPED_TEST_P(RoutingNetworkTest, FUNC_SetupNetwork) {
  this->SetUpNetwork(kNetworkSize);
}

TYPED_TEST_P(RoutingNetworkTest, FUNC_SetupHybridNetwork) {
  this->SetUpNetwork(kServerSize, kClientSize);
}

TYPED_TEST_P(RoutingNetworkTest, FUNC_Send) {
  this->SetUpNetwork(kNetworkSize);
  EXPECT_TRUE(this->Send(1));
}

TYPED_TEST_P(RoutingNetworkTest, FUNC_SendToNonExistingNode) {
  this->SetUpNetwork(kNetworkSize);
  EXPECT_TRUE(this->Send(NodeId(NodeId::kRandomId)));
  EXPECT_TRUE(this->Send(this->nodes_[RandomUint32() % kNetworkSize]->node_id()));
}

TYPED_TEST_P(RoutingNetworkTest, FUNC_ClientSend) {
  this->SetUpNetwork(kServerSize, kClientSize);
  EXPECT_TRUE(this->Send(1));
  Sleep(boost::posix_time::seconds(21));  // This sleep is required for un-responded requests
}

TYPED_TEST_P(RoutingNetworkTest, FUNC_SendMulti) {
  this->SetUpNetwork(kServerSize);
  EXPECT_TRUE(this->Send(40));
}

TYPED_TEST_P(RoutingNetworkTest, FUNC_ClientSendMulti) {
  this->SetUpNetwork(kServerSize, kClientSize);
  EXPECT_TRUE(this->Send(3));
  Sleep(boost::posix_time::seconds(21));  // This sleep is required for un-responded requests
}

// TODO(Prakash): Add more tests for special cases in group messages.
// TYPED_TEST_P(RoutingNetworkTest, FUNC_SendToGroup) {
//   uint8_t message_count(1);
//   this->SetUpNetwork(kServerSize);
//   size_t last_index(this->nodes_.size() - 1);
//   NodeId dest_id(this->nodes_[last_index]->node_id());
//   for (uint16_t index = 0; index < (Parameters::node_group_size); ++index)
//     this->AddNode(false, GenerateUniqueRandomId(dest_id, 10));
//   Sleep(boost::posix_time::seconds(1));
//   EXPECT_TRUE(this->GroupSend(dest_id, message_count));
//   for (size_t index = last_index + 1; index < this->nodes_.size(); ++index)
//     EXPECT_EQ(message_count, this->nodes_[index]->MessagesSize())
//         << "Expected message at Node : "
//         << HexSubstr(this->nodes_[index]->node_id().String());
// }

TYPED_TEST_P(RoutingNetworkTest, FUNC_SendToGroup) {
  uint16_t message_count(1), receivers_message_count(0);
  this->SetUpNetwork(kNetworkSize);
  size_t last_index(this->nodes_.size() - 1);
  NodeId dest_id(this->nodes_[last_index]->node_id());

  EXPECT_TRUE(this->GroupSend(dest_id, message_count));
  for (size_t index = 0; index != (last_index); ++index)
    receivers_message_count += static_cast<uint16_t>(this->nodes_.at(index)->MessagesSize());

  EXPECT_EQ(0, this->nodes_[last_index]->MessagesSize())
        << "Expected message at Node : "
        << HexSubstr(this->nodes_[last_index]->node_id().String());
  EXPECT_EQ(message_count * (Parameters::node_group_size), receivers_message_count);
}

TYPED_TEST_P(RoutingNetworkTest, FUNC_SendToGroupInHybridNetwork) {
  uint16_t message_count(1), receivers_message_count(0);
  this->SetUpNetwork(kServerSize, 2);
  LOG(kVerbose) << "Network created";
  size_t last_index(this->nodes_.size() - 1);
  NodeId dest_id(this->nodes_[last_index]->node_id());

  EXPECT_TRUE(this->GroupSend(dest_id, message_count));
  for (size_t index = 0; index != (last_index); ++index)
    receivers_message_count += static_cast<uint16_t>(this->nodes_.at(index)->MessagesSize());

  EXPECT_EQ(0, this->nodes_[last_index]->MessagesSize())
        << "Expected message at Node : "
        << HexSubstr(this->nodes_[last_index]->node_id().String());
  EXPECT_EQ(message_count * (Parameters::node_group_size), receivers_message_count);
}

TYPED_TEST_P(RoutingNetworkTest, FUNC_SendToGroupRandomId) {
  uint16_t message_count(200), receivers_message_count(0);
  this->SetUpNetwork(kServerSize);
  for (int index = 0; index < message_count; ++index) {
    EXPECT_TRUE(this->GroupSend(NodeId(NodeId::kRandomId), 1));
    for (auto node : this->nodes_) {
      receivers_message_count += static_cast<uint16_t>(node->MessagesSize());
      node->ClearMessages();
    }
  }
  EXPECT_EQ(message_count * (Parameters::node_group_size), receivers_message_count);
  LOG(kVerbose) << "Total message received count : "
                << message_count * (Parameters::node_group_size);
}

TYPED_TEST_P(RoutingNetworkTest, FUNC_JoinAfterBootstrapLeaves) {
  this->SetUpNetwork(kNetworkSize);
  this->nodes_.erase(this->nodes_.begin(), this->nodes_.begin() + 2);
  LOG(kVerbose) << "Network Size " << this->nodes_.size();
  Sleep(boost::posix_time::seconds(2));
  LOG(kVerbose) << "RIse ";
  this->AddNode(false, NodeId());
//  this->AddNode(true, NodeId());
}


// This test produces the recursive call.
TYPED_TEST_P(RoutingNetworkTest, FUNC_RecursiveCall) {
  this->SetUpNetwork(kNetworkSize);
  for (int index(0); index < 8; ++index)
    this->AddNode(false, GenerateUniqueRandomId(20));
  this->AddNode(false, GenerateUniqueRandomId(40));
  this->AddNode(false, GenerateUniqueRandomId(35));
  this->AddNode(false, GenerateUniqueRandomId(30));
  this->AddNode(false, GenerateUniqueRandomId(25));
  this->AddNode(false, GenerateUniqueRandomId(20));
  this->AddNode(false, GenerateUniqueRandomId(10));
  this->AddNode(false, GenerateUniqueRandomId(10));
  this->PrintRoutingTables();
}

TYPED_TEST_P(RoutingNetworkTest, FUNC_SymmetricRouter) {
  this->SetUpNetwork(kNetworkSize);
  this->AddNode(false, rudp::NatType::kSymmetric);
  this->AddNode(false, rudp::NatType::kSymmetric);

  // non-symmetric to non-symmetric
  EXPECT_TRUE(this->Send(this->nodes_[0],
                         this->nodes_[(RandomUint32() % (kNetworkSize - 1)) + 1]->node_id()));

  // symmetric to non-symmetric
  EXPECT_TRUE(this->Send(this->nodes_[kNetworkSize],
                         this->nodes_[RandomUint32() % kNetworkSize]->node_id()));

  // non-symmetric to symmetric
  EXPECT_TRUE(this->Send(this->nodes_[RandomUint32() % kNetworkSize],
                         this->nodes_[kNetworkSize]->node_id()));

  // symmetric to symmetric
  EXPECT_TRUE(this->Send(this->nodes_[kNetworkSize],
                         this->nodes_[kNetworkSize + 1]->node_id()));
}

REGISTER_TYPED_TEST_CASE_P(RoutingNetworkTest, FUNC_SetupNetwork, FUNC_SetupHybridNetwork,
                           FUNC_Send, FUNC_SendToNonExistingNode, FUNC_ClientSend, FUNC_SendMulti,
                           FUNC_ClientSendMulti, FUNC_SendToGroup, FUNC_SendToGroupInHybridNetwork,
                           FUNC_SendToGroupRandomId, FUNC_JoinAfterBootstrapLeaves,
                           FUNC_RecursiveCall, FUNC_SymmetricRouter);
INSTANTIATE_TYPED_TEST_CASE_P(MAIDSAFE, RoutingNetworkTest, TestNode);

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
