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
#include "maidsafe/common/test.h"

#include "maidsafe/common/utils.h"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace args = std::placeholders;

namespace maidsafe {
namespace routing {
namespace test {

namespace{
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

class TestNode {
 public:
  explicit TestNode(const uint32_t &id, bool client_mode = false)
      : id_(id),
        mutex_(),
        key_(MakeKeys()),
        node_config_(fs::unique_path(fs::temp_directory_path() /
            ("node_config_" + std::to_string(id)))),
        functors_(),
        routing_api_() {
    functors_.close_node_replaced = nullptr;
    functors_.message_received = std::bind(&TestNode::MessageReceived, this, args::_1, args::_2);
    functors_.network_status = nullptr;
    functors_.node_validation = nullptr;
    routing_api_.reset(new Routing(key_, functors_, client_mode));
  }

  void MessageReceived(const int32_t &mesasge_type, const std::string &message) {
    LOG(kInfo) << id_ << " -- Received: type <" << mesasge_type
               << "> message : " << message.substr(0, 10);
    std::lock_guard<std::mutex> guard(mutex_);
    messages_.push_back(std::make_pair(mesasge_type, message));
//    if (mesasge_type == 200) reply

  }

  int GetStatus() { return routing_api_->GetStatus(); }
  NodeId node_id() { return NodeId(key_.identity); }
//std::shared_ptr<Routing> routing_api() { return routing_api_;}

 private:
  uint32_t id_;
  std::mutex mutex_;
  asymm::Keys key_;
  boost::filesystem::path node_config_;
  Functors functors_;
  std::shared_ptr<Routing> routing_api_;
  std::vector<std::pair<int32_t, std::string>>  messages_;
};

typedef std::shared_ptr<TestNode> TestNodePtr;
}  // anonymous namspace

//class RoutingFunctionalTest : public testing::Test {
// public:
//   RoutingFunctionalTest() {}
//  ~RoutingFunctionalTest() {}
// protected:
//};

//TEST(RoutingFunctionalTest, FUNC_Network) {
//  std::vector<TestNodePtr> nodes;
//  for (uint8_t i(0); i != 10; ++i) {
//    TestNodePtr node = std::make_shared<TestNode>(i);
//    ASSERT_EQ(kSuccess, node->GetStatus());
//    nodes.push_back(node);
//  }
//}
//
//TEST(RoutingFunctionalTest, FUNC_Send) {
//  std::vector<TestNodePtr> nodes;
//  for (uint8_t i(0); i != 10; ++i) {
//    TestNodePtr node = std::make_shared<TestNode>(i);
//    ASSERT_EQ(kSuccess, node->GetStatus());
//    nodes.push_back(node);
//  }
//  // Send
//
//
//
//}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
