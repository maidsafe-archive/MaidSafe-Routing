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

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/test.h"

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {
namespace routing {
namespace test {

void SortFromTarget(const NodeId& target, std::vector<NodeInfo>& nodes) {
  std::sort(nodes.begin(), nodes.end(),
            [target](const NodeInfo& lhs, const NodeInfo& rhs) {
                return (lhs.node_id ^ target) < (rhs.node_id ^ target);
              });
}

TEST(RoutingTableTest, FUNC_AddCloseNodes) {
    asymm::Keys keys;
    keys.identity = RandomString(64);
  RoutingTable routing_table(keys, false);
  NodeInfo node;
  // check the node is useful when false is set
  for (unsigned int i = 0; i < Parameters::closest_nodes_size ; ++i) {
     node.node_id = NodeId(RandomString(64));
     EXPECT_TRUE(routing_table.CheckNode(node));
  }
  EXPECT_EQ(routing_table.Size(), 0);
  asymm::PublicKey dummy_key;
  // check we cannot input nodes with invalid public_keys
  for (uint16_t i = 0; i < Parameters::closest_nodes_size ; ++i) {
     NodeInfo node(MakeNode());
     node.public_key = dummy_key;
     EXPECT_FALSE(routing_table.AddNode(node));
  }
  EXPECT_EQ(0, routing_table.Size());

  // everything should be set to go now
  for (uint16_t i = 0; i < Parameters::closest_nodes_size ; ++i) {
    node = MakeNode();
    EXPECT_TRUE(routing_table.AddNode(node));
  }
  EXPECT_EQ(Parameters::closest_nodes_size, routing_table.Size());
}

TEST(RoutingTableTest, FUNC_AddTooManyNodes) {
    asymm::Keys keys;
    keys.identity = RandomString(64);
  RoutingTable routing_table(keys, false);
  routing_table.set_remove_node_functor([](const NodeInfo&, const bool&) {});
  for (uint16_t i = 0; routing_table.Size() < Parameters::max_routing_table_size; ++i) {
    NodeInfo node(MakeNode());
    EXPECT_TRUE(routing_table.AddNode(node));
  }
  EXPECT_EQ(routing_table.Size(), Parameters::max_routing_table_size);
  size_t count(0);
  for (uint16_t i = 0; i < 100; ++i) {
    NodeInfo node(MakeNode());
    if (routing_table.CheckNode(node)) {
      EXPECT_TRUE(routing_table.AddNode(node));
      ++count;
    }
  }
  if (count > 0)
    LOG(kInfo) << "made space for " << count << " node(s) in routing table";
  EXPECT_EQ(routing_table.Size(), Parameters::max_routing_table_size);
}

TEST(RoutingTableTest, FUNC_GroupChange) {
  asymm::Keys keys;
  keys.identity = RandomString(64);
  RoutingTable routing_table(keys, false);
  std::vector<NodeInfo> nodes;
  for (uint16_t i = 0; i < Parameters::max_routing_table_size; ++i)
    nodes.push_back(MakeNode());

  SortFromTarget(routing_table.kNodeId(), nodes);

  int count(0);
  routing_table.set_close_node_replaced_functor([&count](const std::vector<NodeInfo> nodes) {
    ++count;
    LOG(kInfo) << "Close node replaced. count : " << count;
    EXPECT_GE(8, count);
    for (auto i: nodes) {
      LOG(kVerbose) << "NodeId : " << DebugId(i.node_id);
    }
  });

  routing_table.set_network_status_functor([](const int& status) {
    LOG(kVerbose) << "Status : " << status;
  });
  for (uint16_t i = 0; i < Parameters::max_routing_table_size; ++i) {
    ASSERT_TRUE(routing_table.AddNode(nodes.at(i)));
    LOG(kVerbose) << "Added to routing_table : " << DebugId(nodes.at(i).node_id);
  }

  ASSERT_EQ(routing_table.Size(), Parameters::max_routing_table_size);
}

TEST(RoutingTableTest, FUNC_CloseAndInRangeCheck) {
  asymm::Keys keys;
  keys.identity = RandomString(64);
  RoutingTable routing_table(keys, false);
  // Add some nodes to routing_table
  NodeId my_node(keys.identity);
  for (uint16_t i = 0; routing_table.Size() < Parameters::max_routing_table_size; ++i) {
    NodeInfo node(MakeNode());
    EXPECT_TRUE(routing_table.AddNode(node));
  }
  EXPECT_EQ(routing_table.Size(), Parameters::max_routing_table_size);
  std::string my_id_encoded(my_node.ToStringEncoded(NodeId::kBinary));
  my_id_encoded[511] = (my_id_encoded[511] == '0' ? '1' : '0');
  NodeId my_closest_node(NodeId(my_id_encoded, NodeId::kBinary));
  EXPECT_TRUE(routing_table.IsThisNodeClosestTo(my_closest_node));
  EXPECT_TRUE(routing_table.IsThisNodeInRange(my_closest_node, 2));
  EXPECT_TRUE(routing_table.IsThisNodeInRange(my_closest_node, 200));
  EXPECT_TRUE(routing_table.IsThisNodeClosestTo(my_closest_node));
  EXPECT_EQ(routing_table.Size(), Parameters::max_routing_table_size);
  // get closest nodes to me
  std::vector<NodeId> close_nodes(routing_table.GetClosestNodes(my_node,
                                              Parameters::closest_nodes_size));
  // Check against individually selected close nodes
  for (uint16_t i = 0; i < Parameters::closest_nodes_size; ++i)
    EXPECT_TRUE(std::find(close_nodes.begin(),
                          close_nodes.end(),
                          routing_table.GetNthClosestNode(my_node, i + 1).node_id)
                              != close_nodes.end());
  // add the node now
     NodeInfo node(MakeNode());
     node.node_id = my_closest_node;
     EXPECT_TRUE(routing_table.AddNode(node));
     EXPECT_FALSE(routing_table.AddNode(node));
  EXPECT_TRUE(routing_table.ConfirmGroupMembers(
      my_closest_node,
      routing_table.GetNthClosestNode(my_closest_node, 1).node_id));
  EXPECT_TRUE(routing_table.ConfirmGroupMembers(
      my_closest_node,
      routing_table.GetNthClosestNode(my_closest_node,
                                      Parameters::closest_nodes_size - 1).node_id));
  EXPECT_FALSE(routing_table.ConfirmGroupMembers(my_closest_node,
      routing_table.GetNthClosestNode(my_closest_node,
                                      Parameters::closest_nodes_size + 1).node_id));
  // should now be closest node to itself :-)
  EXPECT_EQ(routing_table.GetClosestNode(my_closest_node).node_id.String(),
            my_closest_node.String());
  EXPECT_EQ(routing_table.Size(), Parameters::max_routing_table_size);
  EXPECT_EQ(node.node_id, routing_table.DropNode(node.node_id).node_id);
  EXPECT_EQ(routing_table.Size(), Parameters::max_routing_table_size - 1);
  EXPECT_TRUE(routing_table.AddNode(node));
  EXPECT_EQ(routing_table.Size(), Parameters::max_routing_table_size);
  EXPECT_FALSE(routing_table.AddNode(node));
  EXPECT_EQ(routing_table.Size(), Parameters::max_routing_table_size);
  EXPECT_EQ(node.node_id, routing_table.DropNode(node.node_id).node_id);
  EXPECT_EQ(routing_table.Size(), Parameters::max_routing_table_size -1);
  EXPECT_EQ(NodeId(), routing_table.DropNode(node.node_id).node_id);
}

TEST(RoutingTableTest, FUNC_GetClosestNodeWithExclusion) {
  std::vector<NodeId> nodes_id;
  std::vector<std::string> exclude;
  asymm::Keys keys;
  keys.identity = RandomString(64);
  RoutingTable routing_table(keys, false);
  NodeInfo node_info;
  NodeId my_node(keys.identity);

  // Empty routing_table
  node_info = routing_table.GetClosestNode(my_node, exclude, false);
  NodeInfo node_info2(routing_table.GetClosestNode(my_node, exclude, true));
  EXPECT_EQ(node_info.node_id, node_info2.node_id);
  EXPECT_EQ(node_info.node_id, NodeInfo().node_id);

  // routing_table with one element
  NodeInfo node(MakeNode());
  nodes_id.push_back(node.node_id);
  EXPECT_TRUE(routing_table.AddNode(node));

  node_info = routing_table.GetClosestNode(my_node, exclude, false);
  node_info2 = routing_table.GetClosestNode(my_node, exclude, true);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);
  node_info = routing_table.GetClosestNode(nodes_id[0], exclude, false);
  node_info2 = routing_table.GetClosestNode(nodes_id[0], exclude, true);
  EXPECT_NE(node_info.node_id, node_info2.node_id);

  exclude.push_back(nodes_id[0].String());
  node_info = routing_table.GetClosestNode(nodes_id[0], exclude, false);
  node_info2 = routing_table.GetClosestNode(nodes_id[0], exclude, true);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);
  EXPECT_EQ(node_info.node_id, NodeInfo().node_id);

  // routing_table with Parameters::node_group_size elements
  exclude.clear();
  for (uint16_t i(static_cast<uint16_t>(routing_table.Size()));
       routing_table.Size() < Parameters::node_group_size; ++i) {
    NodeInfo node(MakeNode());
    nodes_id.push_back(node.node_id);
    EXPECT_TRUE(routing_table.AddNode(node));
  }

  node_info = routing_table.GetClosestNode(my_node, exclude, false);
  node_info2 = routing_table.GetClosestNode(my_node, exclude, true);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);

  uint16_t random_index = RandomUint32() % Parameters::node_group_size;
  node_info = routing_table.GetClosestNode(nodes_id[random_index], exclude, false);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], exclude, true);
  EXPECT_NE(node_info.node_id, node_info2.node_id);

  exclude.push_back(nodes_id[random_index].String());
  node_info = routing_table.GetClosestNode(nodes_id[random_index], exclude, false);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], exclude, true);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);

  for (auto node_id : nodes_id)
    exclude.push_back(node_id.String());
  node_info = routing_table.GetClosestNode(nodes_id[random_index], exclude, false);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], exclude, true);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);
  EXPECT_EQ(node_info.node_id, NodeInfo().node_id);

  // routing_table with Parameters::Parameters::max_routing_table_size elements
  exclude.clear();
  for (uint16_t i = static_cast<uint16_t>(routing_table.Size());
       routing_table.Size() < Parameters::max_routing_table_size; ++i) {
    NodeInfo node(MakeNode());
    nodes_id.push_back(node.node_id);
    EXPECT_TRUE(routing_table.AddNode(node));
  }

  node_info = routing_table.GetClosestNode(my_node, exclude, false);
  node_info2 = routing_table.GetClosestNode(my_node, exclude, true);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);

  random_index = RandomUint32() % Parameters::max_routing_table_size;
  node_info = routing_table.GetClosestNode(nodes_id[random_index], exclude, false);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], exclude, true);
  EXPECT_NE(node_info.node_id, node_info2.node_id);

  exclude.push_back(nodes_id[random_index].String());
  node_info = routing_table.GetClosestNode(nodes_id[random_index], exclude, false);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], exclude, true);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);

  for (auto node_id : nodes_id)
    exclude.push_back(node_id.String());
  node_info = routing_table.GetClosestNode(nodes_id[random_index], exclude, false);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], exclude, true);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);
  EXPECT_EQ(node_info.node_id, NodeInfo().node_id);
}

}  // namespace test
}  // namespace routing
}  // namespace maidsafe
