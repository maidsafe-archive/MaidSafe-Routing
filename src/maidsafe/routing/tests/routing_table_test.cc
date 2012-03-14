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
#include "maidsafe/transport/utils.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/log.h"

namespace maidsafe {
namespace routing {
namespace test {

class RoutingTableTest {
  RoutingTableTest();
};

TEST(RoutingTableTest, BEH_AddCloseNodes) {
  protobuf::Contact contact;
  contact.set_node_id(RandomString(64));
  RoutingTable RT(contact);
   for (unsigned int i = 0; i < Parameters::kClosestNodesSize ; ++i) {
     EXPECT_TRUE(RT.AddNode(NodeId(RandomString(64))));
   }
   EXPECT_EQ(RT.Size(), Parameters::kClosestNodesSize);
}

TEST(RoutingTableTest, BEH_AddTooManyNodes) {
  protobuf::Contact contact;
  contact.set_node_id(RandomString(64));
  RoutingTable RT(contact);
   for (int i = 0; RT.Size() < Parameters::kMaxRoutingTableSize; ++i) {
     EXPECT_TRUE(RT.AddNode(NodeId(RandomString(64))));
   }
   EXPECT_EQ(RT.Size(), Parameters::kMaxRoutingTableSize);
   size_t count(0);
   for (int i = 0; i < 100; ++i)
     if (RT.AddNode(NodeId(RandomString(64))))
       ++count;
   if (count > 0)
     DLOG(INFO) << "made space for " << count << " node(s) in routing table";
   EXPECT_EQ(RT.Size(), Parameters::kMaxRoutingTableSize);
}

TEST(RoutingTableTest, BEH_CloseAndInRangeCheck) {
  protobuf::Contact contact;
  contact.set_node_id(RandomString(64));
  RoutingTable RT(contact);
  NodeId my_node(NodeId(contact.node_id()));
  // Add some nodes to RT
  for (int i = 0; RT.Size() < Parameters::kMaxRoutingTableSize; ++i) {
    EXPECT_TRUE(RT.AddNode(NodeId(RandomString(64))));
  }
  EXPECT_EQ(RT.Size(), Parameters::kMaxRoutingTableSize);
  std::string my_id_encoded(my_node.ToStringEncoded(NodeId::kBinary));
  my_id_encoded[511] = (my_id_encoded[511] == '0' ? '1' : '0');
  NodeId my_closest_node(NodeId(my_id_encoded, NodeId::kBinary));
  EXPECT_TRUE(RT.AmIClosestNode(my_closest_node));
  EXPECT_TRUE(RT.IsMyNodeInRange(my_closest_node, 2));
  EXPECT_TRUE(RT.IsMyNodeInRange(my_closest_node, 200));
  EXPECT_TRUE(RT.AmIClosestNode(my_closest_node));
  EXPECT_EQ(RT.Size(), Parameters::kMaxRoutingTableSize);
  // get closest nodes to me 
  std::vector<NodeId> close_nodes(RT.GetClosestNodes(my_node, Parameters::kClosestNodesSize));
  // Check against individually selected close nodes
  for (uint16_t i = 0; i < Parameters::kClosestNodesSize; ++i)
    EXPECT_TRUE(std::find(close_nodes.begin(),
                          close_nodes.end(),
                          RT.GetClosestNode(my_node, i)) != close_nodes.end());
  // add the node now
  EXPECT_TRUE(RT.AddNode(my_closest_node));
  // houdl nwo be closest node to itself :-)
  EXPECT_EQ(RT.GetClosestNode(my_closest_node, 0).String(),
            my_closest_node.String());
  EXPECT_EQ(RT.Size(), Parameters::kMaxRoutingTableSize); // make sure we removed a
                                           // node to insert this one
}

}  // namespace test
}  // namespace routing
}  // namespace maidsafe
