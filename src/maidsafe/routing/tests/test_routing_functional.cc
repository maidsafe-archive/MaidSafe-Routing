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

class Node {
 public:
  explicit Node(bool client_mode = false)
      : id_(0),
        key_(MakeKeys()),
        endpoint_(boost::asio::ip::address_v4::loopback(), GetRandomPort()),
        node_config_(),
        functors_(),
        routing_(),
        mutex_(),
        messages_() {
    functors_.close_node_replaced = nullptr;
    functors_.message_received = std::bind(&Node::MessageReceived, this, args::_1, args::_2);
    functors_.network_status = nullptr;
    functors_.node_validation = nullptr;
    routing_.reset(new Routing(key_, functors_, client_mode));
    {
      std::lock_guard<std::mutex> lock(mutex_);
      id_ = next_id_++;
    }
    node_config_ = fs::unique_path(fs::temp_directory_path() /
                       ("node_config_" + std::to_string(id_)));
}

  void MessageReceived(const int32_t &mesasge_type, const std::string &message) {
    LOG(kInfo) << id_ << " -- Received: type <" << mesasge_type
               << "> message : " << message.substr(0, 10);
    std::lock_guard<std::mutex> guard(mutex_);
    messages_.push_back(std::make_pair(mesasge_type, message));
 }

  int GetStatus() { return routing_->GetStatus(); }
  NodeId node_id() { return NodeId(key_.identity); }
  bool BootstrapFromEndpoint(const Endpoint &endpoint) {
    return routing_->BootStrapFromThisEndpoint(endpoint, endpoint_);
  }

  Endpoint endpoint() {
    return endpoint_;
  }

  static size_t next_id_;
 private:
  size_t id_;
  asymm::Keys key_;
  Endpoint endpoint_;
  boost::filesystem::path node_config_;
  Functors functors_;
  std::shared_ptr<Routing> routing_;
  std::mutex mutex_;
  std::vector<std::pair<int32_t, std::string>>  messages_;
};

size_t Node::next_id_(0);

typedef std::shared_ptr<Node> NodePtr;
}  // anonymous namspace

class RoutingFunctionalTest : public testing::Test {
 public:
   RoutingFunctionalTest() : nodes_() {}
  ~RoutingFunctionalTest() {}
 protected:

   virtual void SetUp() {
     NodePtr node1(new Node(false)), node2(new Node(false));
     node1->BootstrapFromEndpoint(node2->endpoint());
     node2->BootstrapFromEndpoint(node1->endpoint());
     nodes_.push_back(node1);
     nodes_.push_back(node2);
   }

   void SetUpNetwork(const size_t &size) {
     for (size_t index = 0; index < size - 2; ++index) {
       NodePtr node(new Node(false));
       node->BootstrapFromEndpoint(nodes_[0]->endpoint());
       nodes_.push_back(node);
     }
   }
   std::vector<NodePtr> nodes_;
};

TEST_F(RoutingFunctionalTest, FUNC_Network) {
//  SetUpNetwork(10);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
