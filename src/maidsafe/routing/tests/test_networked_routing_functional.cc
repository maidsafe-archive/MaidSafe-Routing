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

namespace args = std::placeholders;

namespace maidsafe {

namespace routing {

namespace test {

class TestNode : public GenericNode {
 public:
  explicit TestNode(bool client_mode = false)
      : GenericNode(client_mode),
        messages_() {
    functors_.message_received = [&](const int32_t &mesasge_type,
                                     const std::string &message,
                                     const NodeId &/*group_id*/,
                                     ReplyFunctor reply_functor) {
        LOG(kInfo) << id_ << " -- Received: type <" << mesasge_type
                   << "> message : " << message.substr(0, 10);
      std::lock_guard<std::mutex> guard(mutex_);
      messages_.push_back(std::make_pair(mesasge_type, message));
      reply_functor("Response to " + message);
  };

    LOG(kVerbose) << "RoutingNode constructor";
  }

  virtual ~TestNode() {}

 protected:
  std::vector<std::pair<int32_t, std::string> > messages_;
};

template <typename NodeType>
class RoutingNetworkTest : public GenericNetwork<NodeType> {
 public:
  RoutingNetworkTest(void) : GenericNetwork<NodeType>() {}

 protected:
  /** Send messages from each source to each destination */
  testing::AssertionResult Send(const size_t &messages) {
    NodeId  group_id;
    size_t messages_count(0),
        expected_messages(GenericNetwork<NodeType>::nodes_.size() *
                          (GenericNetwork<NodeType>::nodes_.size() - 1) *
                          messages);
    std::mutex mutex;
    std::condition_variable cond_var;
    for (size_t index = 0; index < messages; ++index) {
      for (auto source_node : GenericNetwork<NodeType>::nodes_) {
        for (auto dest_node : GenericNetwork<NodeType>::nodes_) {
            auto callable = [&] (const int32_t& /*result*/,
                                 const std::vector<std::string>& /*message*/) {
              std::lock_guard<std::mutex> lock(mutex);
              messages_count++;
              LOG(kVerbose) << "ResponseHandler .... " << messages_count;
              if (messages_count == expected_messages) {
                cond_var.notify_one();
                LOG(kVerbose) << "ResponseHandler .... DONE " << messages_count;
              }
            };
          if (source_node->Id() != dest_node->Id()) {
            std::string data(RandomAlphaNumericString(256 * 1024));
            source_node->Send(NodeId(dest_node->Id()), group_id, data, 101, callable,
                boost::posix_time::seconds(15), ConnectType::kSingle);
          }
        }
      }
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
};

TYPED_TEST_CASE_P(RoutingNetworkTest);

TYPED_TEST_P(RoutingNetworkTest, FUNC_Send) {
  this->SetUpNetwork(4);
  EXPECT_TRUE(this->Send(2));
  LOG(kVerbose) << "Func send is over";
  Sleep(boost::posix_time::seconds(5));
}

REGISTER_TYPED_TEST_CASE_P(RoutingNetworkTest, FUNC_Send);
INSTANTIATE_TYPED_TEST_CASE_P(MAIDSAFE, RoutingNetworkTest, TestNode);

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
