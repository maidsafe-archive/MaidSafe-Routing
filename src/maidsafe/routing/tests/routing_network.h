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

#include <future>
#include <string>
#include <vector>

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

#ifdef FAKE_RUDP
  const uint32_t kClientSize(8);
  const uint32_t kServerSize(8);
#else
  const uint32_t kClientSize(4);
  const uint32_t kServerSize(6);
#endif

const uint32_t kNetworkSize = kClientSize + kServerSize;


template <typename NodeType>
class GenericNetwork;

class GenericNode {
 public:
  GenericNode(bool client_mode, const NodeInfoAndPrivateKey &node_info);
  explicit GenericNode(bool client_mode = false);
  virtual ~GenericNode();
//  asymm::Keys GetKeys() const;
  int GetStatus() const;
  NodeId node_id() const;
  size_t id() const;
  Endpoint endpoint() const;
  std::shared_ptr<Routing> routing() const;
  NodeInfo node_info() const;
  bool IsClient() const;
  int ZeroStateJoin(const NodeInfo &peer_node_info);
  int Join(const Endpoint &peer_endpoint);
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
  NodeInfoAndPrivateKey node_info_;
  std::shared_ptr<Routing> routing_;
  Functors functors_;
  std::mutex mutex_;
  bool client_mode_;
};

template <typename NodeType>
class GenericNetwork : public testing::Test {
 public:
  typedef std::shared_ptr<NodeType> NodePtr;
  GenericNetwork() : nodes_(),
      bootstrap_endpoints_(),
      bootstrap_path_("bootstrap") {
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

  virtual void SetUpNetwork(const size_t &size) {
    std::vector<std::future<int>> results;
    for (size_t index = 2; index < size; ++index) {
      NodePtr node(new NodeType(false));
      SetNodeValidationFunctor(node);
      boost::promise<bool> join_promise;
      auto join_future = join_promise.get_future();
      int expected = (nodes_.size() > Parameters::closest_nodes_size) ?
          Parameters::closest_nodes_size : nodes_.size();
      node->functors_.network_status = [expected, &join_promise](const int &result)->void {
        ASSERT_GE(result, kSuccess);
        if (result == 2)
          join_promise.set_value(true);
      };

      nodes_.push_back(node);
      EXPECT_EQ(kSuccess, node->Join(nodes_[1]->endpoint()));
      EXPECT_TRUE(join_future.timed_wait(boost::posix_time::seconds(10)));
    }
  }

  bool AddNode(const bool &client_mode, const NodeId &node_id) {
    NodeInfoAndPrivateKey node_info(MakeNodeInfoAndKeys());
    if (node_id != NodeId())
      node_info.node_info.node_id = node_id;
    NodePtr node(new NodeType(client_mode, node_info));
    SetNodeValidationFunctor(node);
    nodes_.push_back(node);
    return (kSuccess == node->Join(nodes_[1]->endpoint()));
  }

  virtual void Validate(const NodeId& node_id, GivePublicKeyFunctor give_public_key) {
      auto iter = std::find_if(nodes_.begin(), nodes_.end(),
          [&node_id](const NodePtr &node) { return GetKeys(node->node_info_).identity == node_id.String(); });  // NOLINT (Mahmoud)
      EXPECT_NE(iter, nodes_.end());
      if (iter != nodes_.end())
        give_public_key(GetKeys((*iter)->node_info_).public_key);  }

  virtual void SetNodeValidationFunctor(NodePtr node) {
    node->functors_.request_public_key = [this](const NodeId& node_id,
        GivePublicKeyFunctor give_public_key) { this->Validate(node_id, give_public_key);
    };
  }

  std::vector<Endpoint> bootstrap_endpoints_;
  fs::path bootstrap_path_;
};

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TESTS_ROUTING_NETWORK_H_
