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

#include <bitset>
#include <memory>
#include <vector>
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/transport/managed_connection.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/log.h"

namespace maidsafe {
namespace routing {
namespace test {

class RoutingTableTest {
  RoutingTableTest();
};

namespace {
// These are defined in routing_table.cc as private to that file
// Any changes there will NOT be reflected here unless you
// match these parameters
const unsigned int kClosestNodesSize(8);
const unsigned int kMaxRoutingTableSize(64);
const unsigned int kBucketTargetSize(1);
}

NodeInfo MakeNode() {
  NodeInfo node;
  node.node_id = NodeId(RandomString(64));
  asymm::Keys keys;
  asymm::GenerateKeyPair(&keys);
  node.public_key = keys.public_key;
  transport::Port port = 1500;
  transport::IP ip;
  node.endpoint = transport::Endpoint(ip.from_string("192.168.1.1") , port);
  return node;
}

TEST(RoutingTableTest, FUNC_AddCloseNodes) {
  const NodeId test_node(NodeId(RandomString(64)));
  std::shared_ptr<transport::ManagedConnection>
                              ptr(new transport::ManagedConnection);
  RoutingTable RT(test_node, ptr);
  NodeInfo node;
  // check the node is useful when false is set
  for (unsigned int i = 0; i < kClosestNodesSize ; ++i) {
     node.node_id = NodeId(RandomString(64));
     EXPECT_TRUE(RT.CheckNode(node));
  }
  EXPECT_EQ(RT.Size(), 0);
  asymm::PublicKey dummy_key;
  // check we cannot input nodes with invalid public_keys
  for (transport::Port i = 0; i < kClosestNodesSize ; ++i) {
     NodeInfo node(MakeNode());
     node.endpoint.port = 1501 + i;  // has to be unique
     node.public_key = dummy_key;
     EXPECT_FALSE(RT.AddNode(node));
  }
  EXPECT_EQ(RT.Size(), 0);

  // everything should be set to go now
  // TODO should we also test for valid enpoints ??
  // TODO we should fail when public keys are the same
  for (transport::Port i = 0; i < kClosestNodesSize ; ++i) {
     node = MakeNode();
     node.endpoint.port = 1501 + i;  // has to be unique
     EXPECT_TRUE(RT.AddNode(node));
  }
  EXPECT_EQ(RT.Size(), kClosestNodesSize);
}

TEST(RoutingTableTest, FUNC_AddTooManyNodes) {
    std::shared_ptr<transport::ManagedConnection>
                              ptr(new transport::ManagedConnection);
  RoutingTable RT(NodeId(RandomString(64)), ptr);
  for (transport::Port i = 0; RT.Size() < kMaxRoutingTableSize; ++i) {
     NodeInfo node(MakeNode());
     node.endpoint.port = 1501 + i;  // has to be unique
     EXPECT_TRUE(RT.AddNode(node));
  }
  EXPECT_EQ(RT.Size(), kMaxRoutingTableSize);
  size_t count(0);
  for (transport::Port i = 0; i < 100U; ++i) {
     NodeInfo node(MakeNode());
     node.endpoint.port = 1700 + i;  // has to be unique
     if (RT.CheckNode(node)) {
        EXPECT_TRUE(RT.AddNode(node));
       ++count;
     }
  }
  if (count > 0)
     DLOG(INFO) << "made space for " << count << " node(s) in routing table";
  EXPECT_EQ(RT.Size(), kMaxRoutingTableSize);
}

TEST(RoutingTableTest, BEH_CloseAndInRangeCheck) {
  const NodeId my_node(NodeId(RandomString(64)));
    std::shared_ptr<transport::ManagedConnection>
                              ptr(new transport::ManagedConnection);
  RoutingTable RT(my_node, ptr);
  // Add some nodes to RT
  for (transport::Port i = 0; RT.Size() < kMaxRoutingTableSize; ++i) {
     NodeInfo node(MakeNode());
     node.endpoint.port = 1501 + i;  // has to be unique
     EXPECT_TRUE(RT.AddNode(node));
  }
  EXPECT_EQ(RT.Size(), kMaxRoutingTableSize);
  std::string my_id_encoded(my_node.ToStringEncoded(NodeId::kBinary));
  my_id_encoded[511] = (my_id_encoded[511] == '0' ? '1' : '0');
  NodeId my_closest_node(NodeId(my_id_encoded, NodeId::kBinary));

  EXPECT_TRUE(RT.AmIClosestNode(my_closest_node));
  EXPECT_TRUE(RT.IsMyNodeInRange(my_closest_node, 2));
  EXPECT_TRUE(RT.IsMyNodeInRange(my_closest_node, 200));
  EXPECT_TRUE(RT.AmIClosestNode(my_closest_node));
  EXPECT_EQ(RT.Size(), kMaxRoutingTableSize);
  // get closest nodes to me
  std::vector<NodeId> close_nodes(RT.GetClosestNodes(my_node,
                                              kClosestNodesSize));
  // Check against individually selected close nodes
  for (uint16_t i = 0; i < kClosestNodesSize; ++i)
    EXPECT_TRUE(std::find(close_nodes.begin(),
                          close_nodes.end(),
                          RT.GetClosestNode(my_node, i).node_id)
                              != close_nodes.end());
  // add the node now
     NodeInfo node(MakeNode());
     node.endpoint.port = 1502;  // duplicate endpoint
     node.node_id = my_closest_node;
     EXPECT_FALSE(RT.AddNode(node));
     node.endpoint.port = 20000;
     EXPECT_TRUE(RT.AddNode(node));
  // should now be closest node to itself :-)
  EXPECT_EQ(RT.GetClosestNode(my_closest_node, 0).node_id.String(),
            my_closest_node.String());
  EXPECT_EQ(RT.Size(), kMaxRoutingTableSize);
  EXPECT_TRUE(RT.DropNode(node.endpoint));
  EXPECT_TRUE(RT.AddNode(node));
  EXPECT_TRUE(RT.DropNode(node.endpoint));
  EXPECT_FALSE(RT.DropNode(node.endpoint));
}

}  // namespace test
}  // namespace routing
}  // namespace maidsafe
