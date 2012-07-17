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

#include <thread>
#include <vector>

#include "boost/thread/future.hpp"

#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/routing_api_impl.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/tests/routing_network.h"

namespace args = std::placeholders;

namespace maidsafe {

namespace routing {

namespace test {

template <typename NodeType>
class FindNodeNetwork : public GenericNetwork<NodeType> {
 public:
  FindNodeNetwork(void) : GenericNetwork<NodeType>() {}

 protected:
  testing::AssertionResult Find(std::shared_ptr<NodeType> source,
                                const NodeId &node_id) {
    protobuf::Message find_node_rpc(rpcs::FindNodes(node_id, source->node_id()));
    boost::promise<bool> message_sent_promise;
    auto message_sent_future = message_sent_promise.get_future();
    uint8_t attempts(0);
    rudp::MessageSentFunctor message_sent_functor = [&] (bool message_sent) {
        if (message_sent) {
          message_sent_promise.set_value(true);
        } else if (attempts < 3) {
          source->RudpSend(
              this->nodes_[1]->endpoint(),
              find_node_rpc,
              message_sent_functor);
        } else {
          message_sent_promise.set_value(false);
        }
      };
//    source->PrintRoutingTable();
    source->RudpSend(this->nodes_[1]->endpoint(), find_node_rpc, message_sent_functor);
    if (!message_sent_future.timed_wait(boost::posix_time::seconds(10))) {
      return testing::AssertionFailure() << "Unable to send FindValue rpc to bootstrap endpoint - "
                                         << this->nodes_[1]->endpoint().port();
    }
    return testing::AssertionSuccess();
  }

  testing::AssertionResult DropNode(const NodeId &node_id) {
    for (auto node : this->nodes_)
      node->DropNode(node_id);
    return testing::AssertionSuccess();
  }

  void PrintAllRoutingTables() {
    for (size_t index = 0; index < this->nodes_.size(); ++index) {
      LOG(kInfo) << "Routing table of node # " << index;
      this->nodes_[index]->PrintRoutingTable();
    }
  }
};


TYPED_TEST_CASE_P(FindNodeNetwork);

TYPED_TEST_P(FindNodeNetwork, FUNC_FindExistingNode) {
  this->SetUpNetwork(kServerSize);
  bool exists(false);
  int index(0);
  for (auto source : this->nodes_)
    for (auto dest : this->nodes_)
      if ((source->node_id() != dest->node_id()) &&
          (source->node_id() != this->nodes_[1]->node_id()) &&
          (dest->node_id() != this->nodes_[1]->node_id())) {
        LOG(kInfo) << "index: " << index++;
        exists = source->RoutingTableHasNode(dest->node_id());
        EXPECT_TRUE(this->Find(source, dest->node_id()));
        Sleep(boost::posix_time::seconds(3));
        EXPECT_EQ(source->RoutingTableHasNode(dest->node_id()), exists);
      }
}

TYPED_TEST_P(FindNodeNetwork, FUNC_FindNonExistingNode) {
  this->SetUpNetwork(kServerSize);
  uint32_t source(RandomUint32() % (this->nodes_.size() - 2) + 2);
  NodeId node_id(GenerateUniqueRandomId(this->nodes_[source]->node_id(), 6));
  EXPECT_TRUE(this->Find(this->nodes_[source], node_id));
  Sleep(boost::posix_time::seconds(3));
  EXPECT_FALSE(this->nodes_[source]->RoutingTableHasNode(node_id));
  LOG(kVerbose) << "Test is over";
}

TYPED_TEST_P(FindNodeNetwork, FUNC_FindNodeAfterDrop) {
  this->SetUpNetwork(kServerSize);
  uint32_t source(RandomUint32() % (this->nodes_.size() - 2) + 2);
  NodeId node_id(GenerateUniqueRandomId(this->nodes_[source]->node_id(), 6));
  EXPECT_FALSE(this->nodes_[source]->RoutingTableHasNode(node_id));
  EXPECT_TRUE(this->AddNode(false, node_id));
  Sleep(boost::posix_time::seconds(1));
  EXPECT_TRUE(this->nodes_[source]->RoutingTableHasNode(node_id));
  EXPECT_TRUE(this->nodes_[source]->DropNode(node_id));
  Sleep(boost::posix_time::seconds(20));
  EXPECT_TRUE(this->nodes_[source]->RoutingTableHasNode(node_id));
  LOG(kVerbose) << "Test is over";
}

TYPED_TEST_P(FindNodeNetwork, FUNC_FindNodeAfterLeaving) {
//  this->SetUpNetwork(kServerSize);
//  uint8_t index(RandomUint32() % (this->nodes_.size() - 2) + 2);
//  NodeId node_id(this->nodes_[index]->node_id());
//  EXPECT_TRUE(this->RemoveNode(node_id));
//  EXPECT_EQ(this->nodes_.size(), kServerSize - 1);
//  Sleep(boost::posix_time::seconds(15));
//  for (auto node : this->nodes_)
//    EXPECT_FALSE(node->RoutingTableHasNode(node_id));
//  LOG(kVerbose) << "Test is over";
}

REGISTER_TYPED_TEST_CASE_P(FindNodeNetwork,
                           FUNC_FindExistingNode,
                           FUNC_FindNonExistingNode,
                           FUNC_FindNodeAfterDrop,
                           FUNC_FindNodeAfterLeaving);
INSTANTIATE_TYPED_TEST_CASE_P(MAIDSAFE, FindNodeNetwork, GenericNode);

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
