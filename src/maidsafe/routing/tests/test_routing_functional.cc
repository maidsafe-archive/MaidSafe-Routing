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
#include <future>
#include "maidsafe/common/test.h"

#include "maidsafe/common/utils.h"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/bootstrap_file_handler.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/routing_api_impl.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace args = std::placeholders;

namespace maidsafe {

namespace routing {

namespace test {

namespace {

NodeInfo MakeNodeInfo() {
  NodeInfo node;
  node.node_id = NodeId(RandomString(64));
  asymm::Keys keys;
  asymm::GenerateKeyPair(&keys);
  node.public_key = keys.public_key;
  node.endpoint.address(boost::asio::ip::address::from_string("192.168.1.1"));
  node.endpoint.port(GetRandomPort());
  return node;
}

asymm::Keys MakeKeys() {
  NodeInfo node(MakeNodeInfo());
  asymm::Keys keys;
  keys.identity = node.node_id.String();
  keys.public_key = node.public_key;
  return keys;
}

}  // unamed namespace

class RoutingFunctionalTest;

class Node {
 public:
  explicit Node(bool client_mode = false)
      : id_(0),
        key_(MakeKeys()),
        endpoint_(GetLocalIp(), GetRandomPort()),
        routing_(),
        functors_(),
        mutex_(),
        messages_() {
    functors_.close_node_replaced = nullptr;
    functors_.message_received = std::bind(&Node::MessageReceived, this, args::_1, args::_2,
                                           args::_3, args::_4);
    functors_.network_status = nullptr;
    functors_.node_validation = nullptr;
    routing_.reset(new Routing(key_, client_mode));
    std::lock_guard<std::mutex> lock(mutex_);
    id_ = next_id_++;
  }

  void MessageReceived(const int32_t &mesasge_type,
                       const std::string &message,
                       const NodeId &/*group_id*/,
                       ReplyFunctor reply_functor) {
    LOG(kInfo) << id_ << " -- Received: type <" << mesasge_type
               << "> message : " << message.substr(0, 10);
    std::lock_guard<std::mutex> guard(mutex_);
    messages_.push_back(std::make_pair(mesasge_type, message));
    reply_functor("Response to " + message);
 }

  int GetStatus() { return routing_->GetStatus(); }

  NodeId node_id() { return NodeId(key_.identity); }

  int JoinZero(Functors functors, const Endpoint &peer_endpoint, const Endpoint &local_endpoint) {
    return routing_->Join(functors, peer_endpoint, local_endpoint);
  }

  int Join(Functors functors, const Endpoint &peer_endpoint) {
    return routing_->Join(functors, peer_endpoint);
  }

  Endpoint endpoint() const { return endpoint_; }

  std::shared_ptr<Routing> routing() const { return routing_; }

  size_t Id() const { return id_; }

  friend class RoutingFunctionalTest;

  static size_t next_id_;
 protected:
  size_t id_;
  asymm::Keys key_;
  Endpoint endpoint_;
  std::shared_ptr<Routing> routing_;
  Functors functors_;
  std::mutex mutex_;
  std::vector<std::pair<int32_t, std::string>>  messages_;
};

size_t Node::next_id_(0);

typedef std::shared_ptr<Node> NodePtr;

class RoutingFunctionalTest : public testing::Test {
 public:
  RoutingFunctionalTest() : nodes_(),
                            bootstrap_endpoints_(),
                            bootstrap_path_(GetSystemAppDir() / "bootstrap") {}
  ~RoutingFunctionalTest() {}

  void ResponseHandler(const int32_t& /*result*/,
                       const std::string& /*message*/,
                       size_t *message_count,
                       const size_t &total_messages,
                       std::mutex *mutex,
                       std::condition_variable *cond_var) {
    std::lock_guard<std::mutex> lock(*mutex);
    if (++(*message_count) == total_messages)
      cond_var->notify_one();
  }

 protected:

   virtual void SetUp() {
     NodePtr node1(new Node(false)), node2(new Node(false));
     nodes_.push_back(node1);
     nodes_.push_back(node2);
     SetNodeValidationFunctor(node1);
     SetNodeValidationFunctor(node2);
     auto f1 = std::async(std::launch::async, &Node::JoinZero, node1, node1->functors_,
                          node2->endpoint(), node1->endpoint());
     auto f2 = std::async(std::launch::async, &Node::JoinZero, node2, node2->functors_,
                          node1->endpoint(), node2->endpoint());
     EXPECT_EQ(kSuccess, f2.get());
     EXPECT_EQ(kSuccess, f1.get());
   }

   void SetUpNetwork(const size_t &size) {
     std::vector<std::future<int>> results;
     for (size_t index = 2; index < size; ++index) {
       NodePtr node(new Node(false));
       SetNodeValidationFunctor(node);
       results.push_back(std::async(&Node::Join, node, node->functors_, nodes_[1]->endpoint()));
       nodes_.push_back(node);
     }
     for (size_t index = 0; index < nodes_.size() - 2; ++index)
         EXPECT_EQ(kSuccess, results[index].get());
   }

  /** Send messages from randomly chosen sources to randomly chosen destinations */
  testing::AssertionResult RandomSend(const size_t &sources,
                                      const size_t &destinations,
                                      const size_t &messages) {
    size_t messages_count(0), source_id(0), dest_id(0), network_size(nodes_.size());
    NodeId dest_node_id, group_id;
    std::mutex mutex;
    std::condition_variable cond_var;
    if (sources > network_size || sources < 1)
      return testing::AssertionFailure() << "The max and min number of source nodes is "
                                         << nodes_.size() << " and " << 1;
    if (destinations < network_size)
      return testing::AssertionFailure() << "The max and min number of destination nodes is "
                                         << nodes_.size() << " and " << 1;
    std::vector<NodePtr> source_nodes, dest_nodes;
    while(source_nodes.size() < sources) {
      source_id = RandomUint32() % nodes_.size();
      if (std::find(source_nodes.begin(), source_nodes.end(),
                    nodes_[source_id]) == source_nodes.end())
        source_nodes.push_back(nodes_[source_id]);
    }
    // make sure that source and destination nodes are not the same if only one source and one
    // destination is to choose
    if ((sources == 1) && (destinations == 1))
      dest_nodes.push_back(nodes_[(source_nodes[0]->Id() +
          RandomUint32() % (network_size - 1) + 1) % network_size]);
    while(dest_nodes.size() < destinations) {
      dest_id = RandomUint32() % network_size;
      if (std::find(dest_nodes.begin(), dest_nodes.end(),
                    nodes_[dest_id]) == dest_nodes.end())
        dest_nodes.push_back(nodes_[dest_id]);
    }
    for (size_t index = 0; index < messages; ++index) {
      std::string data(RandomAlphaNumericString(256));
      source_id = RandomUint32() % source_nodes.size();
      // chooses a destination different from source
      do {
        dest_id = RandomUint32() % dest_nodes.size();
      } while (source_nodes[source_id]->Id() == dest_nodes[dest_id]->Id());
      dest_node_id = NodeId(dest_nodes[dest_id]->key_.identity);
      source_nodes[source_id]->routing_->Send(dest_node_id, group_id, data, 101,
          std::bind(&RoutingFunctionalTest::ResponseHandler, this, args::_1, args::_2,
                    &messages_count, messages, &mutex, &cond_var),
          10, ConnectType::kSingle);
    }
    std::unique_lock<std::mutex> lock(mutex);
    bool result = cond_var.wait_for(lock, std::chrono::seconds(10),
                                    [&](){ return messages_count == messages; });
    EXPECT_TRUE(result);
    if (!result) {
      return testing::AssertionFailure() << "Send operarion timed out: "
                                         << messages - messages_count << " failed to reply.";
    }
    return testing::AssertionSuccess();
  }

  /** Send messages from each source to each destination */
  testing::AssertionResult Send(const size_t &messages) {
    NodeId  group_id;
    size_t messages_count(0), network_size(nodes_.size());
    std::mutex mutex;
    std::condition_variable cond_var;
    for (size_t index = 0; index < messages; ++index) {
      for (auto source_node : nodes_) {
        for (auto dest_node : nodes_) {
          if (source_node->Id() != dest_node->Id()) {
            std::string data(RandomAlphaNumericString(256));
            source_node->routing_->Send(NodeId(dest_node->key_.identity), group_id, data, 101,
                std::bind(&RoutingFunctionalTest::ResponseHandler, this, args::_1, args::_2,
                          &messages_count, messages, &mutex, &cond_var),
                10, ConnectType::kSingle);
          }
        }
      }
    }

    std::unique_lock<std::mutex> lock(mutex);
    bool result = cond_var.wait_for(lock, std::chrono::seconds(10),
        [&](){ return messages_count == messages * (network_size - 1); });
    EXPECT_TRUE(result);
    if (!result) {
      return testing::AssertionFailure() << "Send operarion timed out: "
                                         << messages * (network_size - 1) - messages_count
                                         << " failed to reply.";
    }
    return testing::AssertionSuccess();
  }

  void Validate(const NodeId& node_id,
                const rudp::EndpointPair& their_endpoint,
                const rudp::EndpointPair& our_endpoint,
                const bool& client,
                std::shared_ptr<Routing> routhing) {
    if (node_id == NodeId()) {
      auto iter = std::find_if(nodes_.begin(), nodes_.end(),
          [&their_endpoint](const NodePtr &node) {
              return node->endpoint().port() == their_endpoint.external.port(); });
      EXPECT_NE(iter, nodes_.end());
      routhing->ValidateThisNode(NodeId((*iter)->key_.identity), (*iter)->key_.public_key,
                                 their_endpoint, our_endpoint, client);
    } else {
      auto iter = std::find_if(nodes_.begin(), nodes_.end(),
          [&node_id](const NodePtr &node) { return node->key_.identity == node_id.String(); });
      EXPECT_NE(iter, nodes_.end());
      routhing->ValidateThisNode(NodeId((*iter)->key_.identity), (*iter)->key_.public_key,
                                 their_endpoint, our_endpoint, client);
    }
  }

  void SetNodeValidationFunctor(NodePtr node) {
    node->functors_.node_validation = std::bind(&RoutingFunctionalTest::Validate, this, args::_1,
                                                args::_2, args::_3, args::_4, node->routing());
  }

  std::vector<NodePtr> nodes_;
  std::vector<Endpoint> bootstrap_endpoints_;
  fs::path bootstrap_path_;
};

TEST_F(RoutingFunctionalTest, FUNC_Send) {
  SetUpNetwork(9);
  EXPECT_TRUE(Send(1));
}

TEST_F(RoutingFunctionalTest, FUNC_RandomSend) {
  SetUpNetwork(9);
  EXPECT_TRUE(RandomSend(9, 9, 10));
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
