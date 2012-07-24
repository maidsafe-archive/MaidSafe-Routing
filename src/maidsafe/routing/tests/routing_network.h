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

#ifndef MAIDSAFE_ROUTING_TESTS_ROUTING_NETWORK_H_
#define MAIDSAFE_ROUTING_TESTS_ROUTING_NETWORK_H_

#include <chrono>
#include <future>
#include <string>
#include <vector>
#include <algorithm>

#include "boost/thread/future.hpp"

#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/tests/test_utils.h"
#include "maidsafe/routing/routing_pb.h"


namespace args = std::placeholders;

namespace maidsafe {

namespace routing {

class Routing;

namespace test {

namespace {

#ifdef FAKE_RUDP
  const uint32_t kClientSize(8);
  const uint32_t kServerSize(8);
#else
  const uint32_t kClientSize(6);
  const uint32_t kServerSize(6);
#endif

const uint32_t kNetworkSize = kClientSize + kServerSize;

}  // anonymous namespace

template <typename NodeType>
class GenericNetwork;

class GenericNode {
 public:
  explicit GenericNode(bool client_mode = false);
  GenericNode(bool client_mode, const NodeInfoAndPrivateKey &node_info);
  virtual ~GenericNode();
  int GetStatus() const;
  NodeId node_id() const;
  size_t id() const;
  Endpoint endpoint() const;
  std::shared_ptr<Routing> routing() const;
  NodeInfo node_info() const;
  void set_joined(const bool node_joined);
  bool joined() const;
  bool client_mode() const;
  void set_client_mode(const bool &client_mode);
  int expected();
  void set_expected(const int &expected);
  int ZeroStateJoin(const NodeInfo &peer_node_info);
  void Join(const Endpoint &peer_endpoint);
  void Send(const NodeId &destination_id,
            const NodeId &group_id,
            const std::string &data,
            const int32_t &type,
            const ResponseFunctor response_functor,
            const boost::posix_time::time_duration &timeout,
            const ConnectType &connect_type);
  void PrintRoutingTable();
  void RudpSend(const Endpoint &peer_endpoint, const protobuf::Message &message,
                rudp::MessageSentFunctor message_sent_functor);
  bool RoutingTableHasNode(const NodeId &node_id);
  bool NonRoutingTableHasNode(const NodeId &node_id);
  testing::AssertionResult DropNode(const NodeId &node_id);

  static size_t next_node_id_;

  template <typename NodeType>
  friend class GenericNetwork;

 protected:
  size_t id_;
  NodeInfoAndPrivateKey node_info_plus_;
  std::shared_ptr<Routing> routing_;
  Functors functors_;
  std::mutex mutex_;
  bool client_mode_;
  bool joined_;
  int expected_;
};

template <typename NodeType>
class GenericNetwork : public testing::Test {
 public:
  typedef std::shared_ptr<NodeType> NodePtr;
  GenericNetwork() : nodes_(),
      bootstrap_endpoints_(),
      bootstrap_path_("bootstrap"),
      mutex_() {
    LOG(kVerbose) << "RoutingNetwork Constructor";
  }

  ~GenericNetwork() {}

  std::vector<NodePtr> nodes_;

 protected:
  virtual void SetUp() {
    NodePtr node1(new NodeType(false)), node2(new NodeType(false));
    nodes_.push_back(node1);
    nodes_.push_back(node2);
    SetNodeValidationFunctor(node1);
    SetNodeValidationFunctor(node2);
    LOG(kVerbose) << "Setup started";
    auto f1 = std::async(std::launch::async, [=, &node2] ()->int {
      return node1->ZeroStateJoin(node2->node_info());
    });
    auto f2 = std::async(std::launch::async, [=, &node1] ()->int {
      return node2->ZeroStateJoin(node1->node_info());
    });
    EXPECT_EQ(kSuccess, f2.get());
    EXPECT_EQ(kSuccess, f1.get());
    LOG(kVerbose) << "Setup succeeded";
  }

  virtual void TearDown() {
    nodes_.clear();
  }

  virtual void SetUpNetwork(const size_t &non_client_size, const size_t &client_size = 0) {
    for (size_t index = 2; index < non_client_size; ++index) {
      NodePtr node(new NodeType(false));
      AddNodeDetails(node);
      LOG(kVerbose) << "Node # " << nodes_.size() << " added to network";
    }
    for (size_t index = 0; index < client_size; ++index) {
      NodePtr node(new NodeType(true));
      AddNodeDetails(node);
      LOG(kVerbose) << "Node # " << nodes_.size() << " added to network";
    }
  }

  void AddNode(const bool &client_mode, const NodeId &node_id) {
    NodePtr node(new NodeType(client_mode));
    if (node_id != NodeId())
      node->node_info_plus_.node_info.node_id = node_id;
    node->set_client_mode(client_mode);
    AddNodeDetails(node);
  }

  bool RemoveNode(const NodeId &node_id) {
      std::lock_guard<std::mutex> lock(mutex_);
      auto iter = std::find_if(nodes_.begin(), nodes_.end(),
          [&node_id](const NodePtr node) {
              return node_id == node->node_id();
          });
      if (iter == nodes_.end())
        return false;
      nodes_.erase(iter);
      return true;
  }

  virtual void Validate(const NodeId& node_id, GivePublicKeyFunctor give_public_key) {
      auto iter = std::find_if(nodes_.begin(), nodes_.end(),
          [&node_id](const NodePtr &node)->bool {
            EXPECT_FALSE(GetKeys(node->node_info_plus_).identity.empty());
            return GetKeys(node->node_info_plus_).identity == node_id.String();
      });
      EXPECT_NE(iter, nodes_.end());
      if (iter != nodes_.end())
        give_public_key(GetKeys((*iter)->node_info_plus_).public_key);  }

  virtual void SetNodeValidationFunctor(NodePtr node) {
    node->functors_.request_public_key = [this](const NodeId& node_id,
        GivePublicKeyFunctor give_public_key) { this->Validate(node_id, give_public_key);
    };
  }

 private:
  uint16_t NonClientNodesSize() const {
    uint16_t non_client_size(0);
    for (auto node : this->nodes_)
      if (!node->client_mode())
        non_client_size++;
    return non_client_size;
  }

  void PrintRoutingTables() {
    for (auto node : this->nodes_)
      node->PrintRoutingTable();
  }

  void AddNodeDetails(NodePtr node) {
    std::condition_variable cond_var;
    std::mutex mutex;
    SetNodeValidationFunctor(node);
    uint16_t node_size(NonClientNodesSize());
    node->set_expected(NetworkStatus(std::min(node_size, Parameters::closest_nodes_size)));
    nodes_.push_back(node);
    node->functors_.network_status = [&cond_var, node](const int &result)->void {
      ASSERT_GE(result, kSuccess);
      if ((result == node->expected()) && (!node->joined())) {
        node->set_joined(true);
        cond_var.notify_one();
      }
    };
    node->Join(nodes_[1]->endpoint());
    std::unique_lock<std::mutex> lock(mutex);
    auto result = cond_var.wait_for(lock, std::chrono::seconds(10));
    EXPECT_EQ(result, std::cv_status::no_timeout);
  }

  std::vector<Endpoint> bootstrap_endpoints_;
  fs::path bootstrap_path_;
  std::mutex mutex_;
};

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TESTS_ROUTING_NETWORK_H_
