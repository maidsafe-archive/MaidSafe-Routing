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

#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {

namespace routing {

class Routing;

namespace test {

template <typename NodeType>
class RoutingNetwork;

namespace args = std::placeholders;

class RoutingNode {
 public:
  explicit RoutingNode(bool client_mode = false);
  virtual ~RoutingNode();
  asymm::Keys GetKeys() const;
  int GetStatus();
  NodeId Id() const;
  Endpoint endpoint() const;
  std::shared_ptr<Routing> routing() const;
  NodeInfo node_info() const;
  int ZeroStateJoin(const NodeInfo &peer_node_info);
  int Join(const Endpoint &peer_endpoint);
  SendStatus Send(const NodeId &destination_id,
                  const NodeId &group_id,
                  const std::string &data,
                  const int32_t &type,
                  const ResponseFunctor response_functor,
                  const boost::posix_time::time_duration &timeout,
                  const ConnectType &connect_type);

  static size_t next_node_id_;

  template <typename NodeType>
  friend class RoutingNetwork;

 protected:
  size_t id_;
  NodeInfo node_info_;
  std::shared_ptr<Routing> routing_;
  Functors functors_;
  std::mutex mutex_;
};

template <typename NodeType>
class RoutingNetwork : public testing::Test {
 public:
  typedef std::shared_ptr<NodeType> NodePtr;
  RoutingNetwork() : nodes_(),
               bootstrap_endpoints_(),
               bootstrap_path_("bootstrap") {
      LOG(kVerbose) << "RoutingNetwork Constructor";
  }

  ~RoutingNetwork() {}

 protected:
  virtual void SetUp() {
    NodePtr node1(new NodeType(false)), node2(new NodeType(false));
    nodes_.push_back(node1);
    nodes_.push_back(node2);
    SetNodeValidationFunctor(node1);
    SetNodeValidationFunctor(node2);
    LOG(kVerbose) << "Setup started";
    auto f1 = std::async(std::launch::async, &RoutingNode::ZeroStateJoin, node1,
                         node2->node_info());
    auto f2 = std::async(std::launch::async, &RoutingNode::ZeroStateJoin, node2,
                         node1->node_info());
    EXPECT_EQ(kSuccess, f2.get());
    EXPECT_EQ(kSuccess, f1.get());
    LOG(kVerbose) << "Setup succeeded";
  }

  void SetUpNetwork(const size_t &size) {
    std::vector<std::future<int>> results;
    for (size_t index = 2; index < size; ++index) {
      NodePtr node(new NodeType(false));
      SetNodeValidationFunctor(node);
      nodes_.push_back(node);
      EXPECT_EQ(kSuccess, node->Join(nodes_[1]->endpoint()));
    }
  }

  void Validate(const NodeId& node_id, GivePublicKeyFunctor node_validate) {
      auto iter = std::find_if(nodes_.begin(), nodes_.end(),
          [&node_id](const NodePtr &node) { return node->GetKeys().identity == node_id.String(); });  // NOLINT (Mahmoud)
      EXPECT_NE(iter, nodes_.end());
      if (iter != nodes_.end())
        node_validate((*iter)->GetKeys().public_key);  }

  void SetNodeValidationFunctor(NodePtr node) {
    node->functors_.request_public_key = std::bind(&RoutingNetwork::Validate, this, args::_1,
                                                args::_2);
  }

  std::vector<NodePtr> nodes_;
  std::vector<Endpoint> bootstrap_endpoints_;
  fs::path bootstrap_path_;
};

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TESTS_ROUTING_NETWORK_H_
