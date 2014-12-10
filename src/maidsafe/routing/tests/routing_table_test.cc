/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#include <bitset>
#include <memory>
#include <vector>

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/tests/test_utils.h"
#include "maidsafe/routing/network_statistics.h"

// TODO(Alison) - test IsNodeIdInGroupRange

namespace maidsafe {

namespace routing {

namespace test {

TEST(RoutingTableTest, BEH_AddCloseNodes) {
  NodeId node_id(RandomString(NodeId::kSize));
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair());
  NodeInfo node;
  // check the node is useful when false is set
  for (unsigned int i = 0; i < Parameters::closest_nodes_size; ++i) {
    node.id = NodeId(RandomString(64));
    EXPECT_TRUE(routing_table.CheckNode(node));
  }
  EXPECT_EQ(routing_table.size(), 0);
  asymm::PublicKey dummy_key;
  // check we cannot input nodes with invalid public_keys
  for (unsigned int i = 0; i < Parameters::closest_nodes_size; ++i) {
    NodeInfo node(MakeNode());
    node.public_key = dummy_key;
    EXPECT_FALSE(routing_table.AddNode(node));
  }
  EXPECT_EQ(0, routing_table.size());
  // everything should be set to go now
  for (unsigned int i = 0; i < Parameters::closest_nodes_size; ++i) {
    node = MakeNode();
    EXPECT_TRUE(routing_table.AddNode(node));
  }
  EXPECT_EQ(Parameters::closest_nodes_size, routing_table.size());
}

TEST(RoutingTableTest, FUNC_AddTooManyNodes) {
  NodeId node_id(RandomString(NodeId::kSize));
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair());

  for (unsigned int i = 0; routing_table.size() < Parameters::max_routing_table_size; ++i) {
    NodeInfo node(MakeNode());
    EXPECT_TRUE(routing_table.AddNode(node));
  }
  EXPECT_EQ(routing_table.size(), Parameters::max_routing_table_size);
  size_t count(0);
  for (unsigned int i = 0; i < 1000; ++i) {
    NodeInfo node(MakeNode());
    if (routing_table.CheckNode(node)) {
      EXPECT_TRUE(routing_table.AddNode(node));
      ++count;
    }
  }
  if (count > 0)
    LOG(kInfo) << "made space for " << count << " node(s) in routing table";
  EXPECT_EQ(routing_table.size(), Parameters::max_routing_table_size);
}

TEST(RoutingTableTest, BEH_GetNthClosest) {
  std::vector<NodeId> nodes_id;
  NodeId node_id(RandomString(NodeId::kSize));
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair());
  NodeId my_node(routing_table.kNodeId());

  for (unsigned int i(static_cast<unsigned int>(routing_table.size())); routing_table.size() < 10;
       ++i) {
    NodeInfo node(MakeNode());
    nodes_id.push_back(node.id);
    EXPECT_TRUE(routing_table.AddNode(node));
  }
  std::sort(nodes_id.begin(), nodes_id.end(), [&](const NodeId& lhs, const NodeId& rhs) {
    return NodeId::CloserToTarget(lhs, rhs, my_node);
  });
  for (unsigned int index = 0; index < 10; ++index) {
    EXPECT_EQ(nodes_id[index], routing_table.GetNthClosestNode(my_node, index + 1).id)
        << DebugId(nodes_id[index]) << " not eq to "
        << DebugId(routing_table.GetNthClosestNode(my_node, index + 1).id);
  }
}

TEST(RoutingTableTest, FUNC_GetClosestNodeWithExclusion) {
  NodeId node_id(RandomString(NodeId::kSize));
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair());
  std::vector<NodeId> nodes_id;
  std::vector<std::string> exclude;
  NodeInfo node_info;
  NodeId my_node(routing_table.kNodeId());

  // Empty routing_table
  node_info = routing_table.GetClosestNode(my_node, false, exclude);
  NodeInfo node_info2(routing_table.GetClosestNode(my_node, true, exclude));
  EXPECT_EQ(node_info.id, node_info2.id);
  EXPECT_EQ(node_info.id, NodeInfo().id);

  // routing_table with one element
  NodeInfo node(MakeNode());
  nodes_id.push_back(node.id);
  EXPECT_TRUE(routing_table.AddNode(node));

  node_info = routing_table.GetClosestNode(my_node, false, exclude);
  node_info2 = routing_table.GetClosestNode(my_node, true, exclude);
  EXPECT_EQ(node_info.id, node_info2.id);
  node_info = routing_table.GetClosestNode(nodes_id[0], false, exclude);
  node_info2 = routing_table.GetClosestNode(nodes_id[0], true, exclude);
  EXPECT_NE(node_info.id, node_info2.id);

  exclude.push_back(nodes_id[0].string());
  node_info = routing_table.GetClosestNode(nodes_id[0], false, exclude);
  node_info2 = routing_table.GetClosestNode(nodes_id[0], true, exclude);
  EXPECT_EQ(node_info.id, node_info2.id);
  EXPECT_EQ(node_info.id, NodeInfo().id);

  // routing_table with Parameters::group_size elements
  exclude.clear();
  for (unsigned int i(static_cast<unsigned int>(routing_table.size()));
       routing_table.size() < Parameters::group_size; ++i) {
    NodeInfo node(MakeNode());
    nodes_id.push_back(node.id);
    EXPECT_TRUE(routing_table.AddNode(node));
  }

  node_info = routing_table.GetClosestNode(my_node, false, exclude);
  node_info2 = routing_table.GetClosestNode(my_node, true, exclude);
  EXPECT_EQ(node_info.id, node_info2.id);

  unsigned int random_index = RandomUint32() % Parameters::group_size;
  node_info = routing_table.GetClosestNode(nodes_id[random_index], false, exclude);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], true, exclude);
  EXPECT_NE(node_info.id, node_info2.id);

  exclude.push_back(nodes_id[random_index].string());
  node_info = routing_table.GetClosestNode(nodes_id[random_index], false, exclude);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], true, exclude);
  EXPECT_EQ(node_info.id, node_info2.id);

  for (const auto& node_id : nodes_id)
    exclude.push_back(node_id.string());
  node_info = routing_table.GetClosestNode(nodes_id[random_index], false, exclude);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], true, exclude);
  EXPECT_EQ(node_info.id, node_info2.id);
  EXPECT_EQ(node_info.id, NodeInfo().id);

  // routing_table with Parameters::Parameters::max_routing_table_size elements
  exclude.clear();
  for (unsigned int i = static_cast<unsigned int>(routing_table.size());
       routing_table.size() < Parameters::max_routing_table_size; ++i) {
    NodeInfo node(MakeNode());
    nodes_id.push_back(node.id);
    EXPECT_TRUE(routing_table.AddNode(node));
  }

  node_info = routing_table.GetClosestNode(my_node, false, exclude);
  node_info2 = routing_table.GetClosestNode(my_node, true, exclude);
  EXPECT_EQ(node_info.id, node_info2.id);

  random_index = RandomUint32() % Parameters::max_routing_table_size;
  node_info = routing_table.GetClosestNode(nodes_id[random_index], false, exclude);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], true, exclude);
  EXPECT_NE(node_info.id, node_info2.id);

  exclude.push_back(nodes_id[random_index].string());
  node_info = routing_table.GetClosestNode(nodes_id[random_index], false, exclude);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], true, exclude);
  EXPECT_EQ(node_info.id, node_info2.id);

  for (const auto& node_id : nodes_id)
    exclude.push_back(node_id.string());
  node_info = routing_table.GetClosestNode(nodes_id[random_index], false, exclude);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], true, exclude);
  EXPECT_EQ(node_info.id, node_info2.id);
  EXPECT_EQ(node_info.id, NodeInfo().id);
}

TEST(RoutingTableTest, FUNC_ClosestToId) {
  NodeId own_node_id(RandomString(NodeId::kSize));
  RoutingTable routing_table(false, own_node_id, asymm::GenerateKeyPair());
  std::vector<NodeInfo> known_nodes;
  std::vector<NodeInfo> known_targets;
  NodeId target;
  NodeInfo node_info;
  NodeId furthest_group_node;

  auto test_known_ids = [&, this ]()->bool {
    LOG(kInfo) << "\tTesting known ids...";
    bool passed(true);
    bool result(false);
    bool expectation(false);
    for (const auto& target : known_targets) {
      PartialSortFromTarget(target.id, known_nodes, 2);
      result = routing_table.IsThisNodeClosestTo(target.id, true);
      expectation = false;
      if (NodeId::CloserToTarget(own_node_id, known_nodes.at(1).id, target.id))
        expectation = true;
      EXPECT_EQ(expectation, result);
      if (expectation != result)
        passed = false;
    }
    return passed;
  };  // NOLINT

  auto test_unknown_ids = [&, this ]()->bool {
    LOG(kInfo) << "\tTesting unknown ids...";
    bool passed(true);
    bool result(false);
    bool expectation(false);
    for (unsigned int i(0); i < 200; ++i) {
      target = NodeId(RandomString(NodeId::kSize));
      PartialSortFromTarget(target, known_nodes, 1);
      result = routing_table.IsThisNodeClosestTo(target, true);
      expectation = false;
      if (NodeId::CloserToTarget(own_node_id, known_nodes.at(0).id, target))
        expectation = true;
      EXPECT_EQ(expectation, result);
      if (expectation != result)
        passed = false;
    }
    return passed;
  };  // NOLINT

  // ------- Empty routing table -------
  LOG(kInfo) << "Testing empty routing table...";
  EXPECT_FALSE(routing_table.IsThisNodeClosestTo(own_node_id, true));

  for (unsigned int i(0); i < 200; ++i) {
    target = NodeId(RandomString(NodeId::kSize));
    routing_table.IsThisNodeClosestTo(target, true);
  }

  // ------- Partially populated routing table -------
  LOG(kInfo) << "Partially populating routing table...";
  while (routing_table.size() < static_cast<size_t>(Parameters::max_routing_table_size / 4)) {
    node_info = MakeNode();
    known_nodes.push_back(node_info);
    known_targets.push_back(node_info);
    EXPECT_TRUE(routing_table.AddNode(node_info));
  }
  PartialSortFromTarget(own_node_id, known_nodes, Parameters::group_size);
  furthest_group_node = known_nodes.at(Parameters::group_size - 2).id;

  LOG(kInfo) << "Testing partially populated routing table...";
  EXPECT_FALSE(routing_table.IsThisNodeClosestTo(own_node_id, true));
  EXPECT_TRUE(test_known_ids());
  EXPECT_TRUE(test_unknown_ids());

  // ------- Fully populated routing table -------
  LOG(kInfo) << "Fully populating routing table...";
  while (routing_table.size() < Parameters::max_routing_table_size) {
    node_info = MakeNode();
    known_nodes.push_back(node_info);
    known_targets.push_back(node_info);
    EXPECT_TRUE(routing_table.AddNode(node_info));
  }
  PartialSortFromTarget(own_node_id, known_nodes, Parameters::group_size);
  furthest_group_node = known_nodes.at(Parameters::group_size - 2).id;

  LOG(kInfo) << "Testing fully populated routing table...";
  EXPECT_FALSE(routing_table.IsThisNodeClosestTo(own_node_id, true));
  EXPECT_TRUE(test_known_ids());
  EXPECT_TRUE(test_unknown_ids());
}

TEST(RoutingTableTest, FUNC_GetRandomExistingNode) {
  NodeId own_node_id(RandomString(NodeId::kSize));
  RoutingTable routing_table(false, own_node_id, asymm::GenerateKeyPair());
  NodeInfo node_info;
  std::vector<NodeInfo> known_nodes;

#ifdef NDEBUG
  EXPECT_FALSE(routing_table.RandomConnectedNode().IsValid());
#endif
  auto run_random_connected_node_test = [&]() {
    NodeId random_connected_node_id = routing_table.RandomConnectedNode();
    LOG(kVerbose) << "Got random connected node: " << DebugId(random_connected_node_id);
    auto found(
        std::find_if(std::begin(known_nodes), std::end(known_nodes),
                     [=](const NodeInfo& node) { return (node.id == random_connected_node_id); }));
    ASSERT_FALSE(found == std::end(known_nodes));
  };

  while (routing_table.size() < Parameters::max_routing_table_size) {
    node_info = MakeNode();
    known_nodes.push_back(node_info);
    EXPECT_TRUE(routing_table.AddNode(node_info));
    run_random_connected_node_test();
  }
  for (auto i(0); i < 100; ++i)
    run_random_connected_node_test();
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
