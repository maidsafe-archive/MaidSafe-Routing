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
  NodeId node_id(NodeId::IdType::kRandomId);
  NetworkStatistics network_statistics(node_id);
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair(), network_statistics);
  NodeInfo node;
  // check the node is useful when false is set
  for (unsigned int i = 0; i < Parameters::closest_nodes_size; ++i) {
    node.node_id = NodeId(RandomString(64));
    EXPECT_TRUE(routing_table.CheckNode(node));
  }
  EXPECT_EQ(routing_table.size(), 0);
  asymm::PublicKey dummy_key;
  // check we cannot input nodes with invalid public_keys
  for (uint16_t i = 0; i < Parameters::closest_nodes_size; ++i) {
    NodeInfo node(MakeNode());
    node.public_key = dummy_key;
    EXPECT_FALSE(routing_table.AddNode(node));
  }
  EXPECT_EQ(0, routing_table.size());
  // everything should be set to go now
  for (uint16_t i = 0; i < Parameters::closest_nodes_size; ++i) {
    node = MakeNode();
    EXPECT_TRUE(routing_table.AddNode(node));
  }
  EXPECT_EQ(Parameters::closest_nodes_size, routing_table.size());
}

TEST(RoutingTableTest, FUNC_AddTooManyNodes) {
  NodeId node_id(NodeId::IdType::kRandomId);
  NetworkStatistics network_statistics(node_id);
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair(), network_statistics);

  for (uint16_t i = 0; routing_table.size() < Parameters::max_routing_table_size; ++i) {
    NodeInfo node(MakeNode());
    EXPECT_TRUE(routing_table.AddNode(node));
  }
  EXPECT_EQ(routing_table.size(), Parameters::max_routing_table_size);
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
  EXPECT_EQ(routing_table.size(), Parameters::max_routing_table_size);
}

/* Is removed as matrix removed
TEST(RoutingTableTest, FUNC_OrderedGroupChange) {
  NodeId node_id(NodeId::IdType::kRandomId);
  NetworkStatistics network_statistics(node_id);
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair(), network_statistics);
  std::vector<NodeInfo> nodes;

  for (uint16_t i = 0; i < Parameters::max_routing_table_size; ++i)
    nodes.push_back(MakeNode());

  SortFromTarget(routing_table.kNodeId(), nodes);

  int count(0);

  routing_table.InitialiseFunctors([](const int& status) { LOG(kVerbose) << "Status: " << status; },
                                   [](const NodeInfo&, bool) {},
                                   [](std::shared_ptr<MatrixChange>) {});

  for (uint16_t i = 0; i < Parameters::max_routing_table_size; ++i) {
    ASSERT_TRUE(routing_table.AddNode(nodes.at(i)));
    LOG(kVerbose) << "Added to routing_table : " << DebugId(nodes.at(i).node_id);
  }

  EXPECT_EQ(Parameters::closest_nodes_size, count);
  ASSERT_EQ(routing_table.size(), Parameters::max_routing_table_size);
  std::vector<NodeId> expected_close_nodes;
  for (uint16_t i(0); i < Parameters::closest_nodes_size; ++i)
    expected_close_nodes.push_back(nodes.at(i).node_id);
  std::vector<NodeInfo> close_nodes(routing_table.group_matrix_.GetConnectedPeers());
  EXPECT_EQ(expected_close_nodes.size(), close_nodes.size());
  for (uint16_t i(0); i < std::min(expected_close_nodes.size(), close_nodes.size()); ++i)
    EXPECT_EQ(expected_close_nodes.at(i), close_nodes.at(i).node_id);
}
*/

/* Is removed as matrix removed
TEST(RoutingTableTest, FUNC_ReverseOrderedGroupChange) {
  NodeId node_id(NodeId::IdType::kRandomId);
  NetworkStatistics network_statistics(node_id);
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair(), network_statistics);
  std::vector<NodeInfo> nodes;

  for (uint16_t i = 0; i < Parameters::max_routing_table_size; ++i)
    nodes.push_back(MakeNode());

  SortFromTarget(routing_table.kNodeId(), nodes);

  // Set functors
  int count(0);
  std::vector<NodeInfo> expected_close_nodes;
  NetworkStatusFunctor network_status_functor = [](const int& status) {
    LOG(kVerbose) << "Status : " << status;
  };
  std::function<void(const NodeInfo&, bool)> remove_node_functor = [](
      const NodeInfo&, bool) { LOG(kVerbose) << "RemoveNodeFunctor!"; };  // NOLINT
  ConnectedGroupChangeFunctor group_change_functor = [&count, &expected_close_nodes](
      const std::vector<NodeInfo> nodes, const std::vector<NodeInfo>) {
    ++count;
    LOG(kInfo) << "Group changed. count : " << count;
    EXPECT_GE(2 * Parameters::max_routing_table_size, count);
    for (const auto& i : nodes) {
      LOG(kVerbose) << "NodeId : " << DebugId(i.node_id);
    }
    for (const auto& i : expected_close_nodes) {
      LOG(kVerbose) << "Expected Id : " << DebugId(i.node_id);
    }
    size_t max_index(std::min(expected_close_nodes.size(), nodes.size()));
    for (uint16_t i(0); i < max_index; ++i) {
      auto node_id(expected_close_nodes.at(i).node_id);
      EXPECT_NE(std::find_if(nodes.begin(), nodes.end(),
                             [node_id](const NodeInfo& info) { return info.node_id == node_id; }),
                nodes.end());
    }
  };
  routing_table.InitialiseFunctors(network_status_functor, remove_node_functor, []() {},
                                   group_change_functor, [](std::shared_ptr<MatrixChange>) {});

  // Add nodes to routing table
  for (auto ritr = nodes.rbegin(); ritr < nodes.rend(); ++ritr) {
    if (expected_close_nodes.size() == Parameters::closest_nodes_size)
      expected_close_nodes.pop_back();
    expected_close_nodes.insert(expected_close_nodes.begin(), *ritr);
    ASSERT_TRUE(routing_table.AddNode(*ritr));
    LOG(kVerbose) << "Added to routing_table : " << DebugId((*ritr).node_id);
  }

  EXPECT_EQ(routing_table.size(), Parameters::max_routing_table_size);
  EXPECT_EQ(Parameters::max_routing_table_size, count);
  std::vector<NodeId> expected_close_nodes2;
  for (const auto& node : nodes)
    expected_close_nodes2.push_back(node.node_id);
  std::vector<NodeInfo> close_nodes2(routing_table.group_matrix_.GetConnectedPeers());
  for (const auto& expected_close_node : expected_close_nodes2)
    EXPECT_NE(std::find_if(std::begin(close_nodes2), std::end(close_nodes2),
                           [expected_close_node](const NodeInfo& node_info) {
                             return node_info.node_id == expected_close_node;
                           }), std::end(close_nodes2));

  // Remove nodes from routing table
  auto itr_near = nodes.begin();
  auto itr_far = nodes.begin() + Parameters::closest_nodes_size;
  while (itr_near != nodes.end()) {
    if (itr_far != nodes.end()) {
      expected_close_nodes.push_back(*itr_far);
      ++itr_far;
    }
    expected_close_nodes.erase(expected_close_nodes.begin());
    routing_table.DropNode((*itr_near).node_id, true);
    LOG(kVerbose) << "Dropped from routing_table : " << DebugId((*itr_near).node_id);
    ++itr_near;
  }

  EXPECT_EQ(routing_table.size(), 0);
  EXPECT_EQ(2 * Parameters::max_routing_table_size, count);
  EXPECT_EQ(0, routing_table.group_matrix_.GetConnectedPeers().size());
}
*/

/* Is removed as matrix removed
TEST(RoutingTableTest, FUNC_CheckGroupChangeRemoveNodesFromGroup) {
  NodeId node_id(NodeId::IdType::kRandomId);
  NetworkStatistics network_statistics(node_id);
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair(), network_statistics);
  std::vector<NodeInfo> nodes;

  for (uint16_t i = 0; i < Parameters::max_routing_table_size; ++i)
    nodes.push_back(MakeNode());

  SortFromTarget(routing_table.kNodeId(), nodes);

  // Set functors
  int count(0);
  NetworkStatusFunctor network_status_functor = [](const int & status) {
    LOG(kVerbose) << "Status : " << status;
  };
  std::function<void(const NodeInfo&, bool)> remove_node_functor = [](
      const NodeInfo&, bool) { LOG(kVerbose) << "RemoveNodeFunctor!"; };  // NOLINT
  bool setting_up(true);
  std::vector<NodeInfo> expected_close_nodes;
  ConnectedGroupChangeFunctor group_change_functor = [&count, &setting_up, &expected_close_nodes](
      const std::vector<NodeInfo> nodes, const std::vector<NodeInfo>) {
    ++count;
    LOG(kInfo) << "Group changed. count : " << count;
    if (setting_up) {
      EXPECT_GE(Parameters::closest_nodes_size, count);
      for (const auto& i : nodes) {
        LOG(kVerbose) << "NodeId : " << DebugId(i.node_id);
      }
    } else {
      EXPECT_GE(Parameters::max_routing_table_size / 4, count);
      for (const auto& i : nodes) {
        LOG(kVerbose) << "NodeId : " << DebugId(i.node_id);
      }
      for (const auto& i : expected_close_nodes) {
        LOG(kVerbose) << "Expected Id : " << DebugId(i.node_id);
      }
      EXPECT_EQ(nodes.size(), expected_close_nodes.size());
      uint16_t max_index(
          static_cast<uint16_t>(std::min(expected_close_nodes.size(), nodes.size())));
      for (uint16_t i(0); i < max_index; ++i) {
        EXPECT_EQ(nodes.at(i).node_id, expected_close_nodes.at(i).node_id)
            << "actual node: " << DebugId(nodes.at(i).node_id)
            << "\n expected node: " << DebugId(expected_close_nodes.at(i).node_id);
      }
    }
  };
  routing_table.InitialiseFunctors(network_status_functor, remove_node_functor, []() {},
                                   group_change_functor, [](std::shared_ptr<MatrixChange>) {});

  // Populate routing table
  for (uint16_t i = 0; i < Parameters::max_routing_table_size; ++i) {
    ASSERT_TRUE(routing_table.AddNode(nodes.at(i)));
    LOG(kVerbose) << "Added to routing_table : " << DebugId(nodes.at(i).node_id);
  }

  EXPECT_EQ(Parameters::closest_nodes_size, count);
  ASSERT_EQ(routing_table.size(), Parameters::max_routing_table_size);

  // Reset functor arguments
  count = 0;
  setting_up = false;

  // Remove nodes from closest Parameters::closest_nodes_size
  int index_to_remove;
  NodeId node_id_to_remove;
  for (int i(0); i < Parameters::max_routing_table_size / 4; ++i) {
    index_to_remove = RandomUint32() % Parameters::closest_nodes_size;
    node_id_to_remove = nodes.at(index_to_remove).node_id;
    nodes.erase(nodes.begin() + index_to_remove);
    expected_close_nodes.clear();
    for (int j(0); j < Parameters::closest_nodes_size; ++j)
      expected_close_nodes.push_back(nodes.at(j));
    routing_table.DropNode(node_id_to_remove, true);
    LOG(kVerbose) << "Dropped from routing_table : " << DebugId(node_id_to_remove);
  }

  EXPECT_EQ(Parameters::max_routing_table_size / 4, count);
}
*/

/* Is removed as matrix removed
TEST(RoutingTableTest, FUNC_CheckGroupChangeAddGroupNodesToFullTable) {
  NodeId node_id(NodeId::IdType::kRandomId);
  NetworkStatistics network_statistics(node_id);
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair(), network_statistics);
  std::vector<NodeInfo> nodes;

  for (uint16_t i = 0; i < Parameters::max_routing_table_size; ++i)
    nodes.push_back(MakeNode());

  SortFromTarget(routing_table.kNodeId(), nodes);

  // Bias nodes' NodeIds away from routing table's own NodeId
  nodes.erase(nodes.begin(), nodes.begin() + Parameters::closest_nodes_size);
  for (uint16_t i = 0; i < Parameters::closest_nodes_size; ++i)
    nodes.push_back(MakeNode());

  SortFromTarget(routing_table.kNodeId(), nodes);

  // Set functors
  int count(0);
  NetworkStatusFunctor network_status_functor = [](const int& status) {
    LOG(kVerbose) << "Status : " << status;
  };
  std::function<void(const NodeInfo&, bool)> remove_node_functor = [](
      const NodeInfo&, bool) { LOG(kVerbose) << "RemoveNodeFunctor!"; };  // NOLINT
  bool setting_up(true);
  std::vector<NodeInfo> expected_close_nodes;
  ConnectedGroupChangeFunctor group_change_functor =
      [&count, &nodes, &expected_close_nodes, &setting_up](const std::vector<NodeInfo> new_group,
                                                           const std::vector<NodeInfo>) {
    ++count;
    LOG(kInfo) << "Group changed. count : " << count;
    if (setting_up) {
      EXPECT_GE(Parameters::closest_nodes_size, count);
      for (const auto& i : nodes) {
        LOG(kVerbose) << "NodeId : " << DebugId(i.node_id);
      }
    } else {
      EXPECT_GE(3 * Parameters::closest_nodes_size / 2, count);
      for (const auto& i : nodes) {
        LOG(kVerbose) << "NodeId : " << DebugId(i.node_id);
      }
      for (const auto& i : expected_close_nodes) {
        LOG(kVerbose) << "Expected Id : " << DebugId(i.node_id);
      }
      uint16_t max_index(
          static_cast<uint16_t>(std::min(expected_close_nodes.size(), new_group.size())));
      for (uint16_t i(0); i < max_index; ++i) {
        EXPECT_EQ(new_group.at(i).node_id, expected_close_nodes.at(i).node_id)
            << "actual node: " << DebugId(new_group.at(i).node_id)
            << "\n expected node: " << DebugId(expected_close_nodes.at(i).node_id);
      }
    }
  };
  routing_table.InitialiseFunctors(network_status_functor, remove_node_functor, []() {},
                                   group_change_functor, [](std::shared_ptr<MatrixChange>) {});

  // Populate routing table
  for (uint16_t i = 0; i < Parameters::max_routing_table_size; ++i) {
    ASSERT_TRUE(routing_table.AddNode(nodes.at(i)));
    LOG(kVerbose) << "Added to routing_table : " << DebugId(nodes.at(i).node_id);
  }

  EXPECT_EQ(Parameters::closest_nodes_size, count);
  ASSERT_EQ(routing_table.size(), Parameters::max_routing_table_size);
  LOG(kVerbose) << "Own NodeId: " << DebugId(routing_table.kNodeId());

  // Reset functor arguments
  count = 0;
  setting_up = false;
  expected_close_nodes = nodes;
  expected_close_nodes.erase(expected_close_nodes.begin() + Parameters::closest_nodes_size,
                             expected_close_nodes.end());

  // Add more nodes for routing table's group
  NodeInfo new_node;
  bool found_node;
  NodeId bucket_centre(routing_table.kNodeId());
  NodeId bucket_edge;

  int j(0);
  while (j < 3 * Parameters::closest_nodes_size / 2) {
    found_node = false;
    bucket_edge = expected_close_nodes.at(Parameters::closest_nodes_size - 1).node_id;
    int k(0);
    while (!found_node && k++ < 70) {
      new_node = MakeNode();
      found_node = (bucket_centre ^ new_node.node_id) < (bucket_centre ^ bucket_edge);
    }
    if (!found_node)
      break;
    expected_close_nodes.push_back(new_node);
    SortFromTarget(bucket_centre, expected_close_nodes);
    expected_close_nodes.pop_back();
    ASSERT_TRUE(routing_table.AddNode(new_node));
    LOG(kVerbose) << "Added to routing_table : " << DebugId(new_node.node_id);
    ++j;
  }
  if (j != 3 * Parameters::closest_nodes_size / 2) {
    LOG(kError) << "Failed to generate enough close nodes (up to 70 attemps allowed per node)";
    ASSERT_TRUE(false);
  }

  EXPECT_EQ(3 * Parameters::closest_nodes_size / 2, count);
  ASSERT_EQ(routing_table.size(), Parameters::max_routing_table_size);
}
*/

TEST(RoutingTableTest, BEH_GetNthClosest) {
  std::vector<NodeId> nodes_id;
  NodeId node_id(NodeId::IdType::kRandomId);
  NetworkStatistics network_statistics(node_id);
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair(), network_statistics);
  NodeId my_node(routing_table.kNodeId());

  for (uint16_t i(static_cast<uint16_t>(routing_table.size())); routing_table.size() < 10; ++i) {
    NodeInfo node(MakeNode());
    nodes_id.push_back(node.node_id);
    EXPECT_TRUE(routing_table.AddNode(node));
  }
  std::sort(nodes_id.begin(), nodes_id.end(), [&](const NodeId & lhs, const NodeId & rhs) {
    return NodeId::CloserToTarget(lhs, rhs, my_node);
  });
  for (uint16_t index = 0; index < 10; ++index) {
    EXPECT_EQ(nodes_id[index], routing_table.GetNthClosestNode(my_node, index + 1).node_id)
        << DebugId(nodes_id[index]) << " not eq to "
        << DebugId(routing_table.GetNthClosestNode(my_node, index + 1).node_id);
  }
}

TEST(RoutingTableTest, FUNC_GetClosestNodeWithExclusion) {
  NodeId node_id(NodeId::IdType::kRandomId);
  NetworkStatistics network_statistics(node_id);
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair(), network_statistics);
  std::vector<NodeId> nodes_id;
  std::vector<std::string> exclude;
  NodeInfo node_info;
  NodeId my_node(routing_table.kNodeId());

  // Empty routing_table
  node_info = routing_table.GetClosestNode(my_node, false, exclude);
  NodeInfo node_info2(routing_table.GetClosestNode(my_node, true, exclude));
  EXPECT_EQ(node_info.node_id, node_info2.node_id);
  EXPECT_EQ(node_info.node_id, NodeInfo().node_id);

  // routing_table with one element
  NodeInfo node(MakeNode());
  nodes_id.push_back(node.node_id);
  EXPECT_TRUE(routing_table.AddNode(node));

  node_info = routing_table.GetClosestNode(my_node, false, exclude);
  node_info2 = routing_table.GetClosestNode(my_node, true, exclude);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);
  node_info = routing_table.GetClosestNode(nodes_id[0], false, exclude);
  node_info2 = routing_table.GetClosestNode(nodes_id[0], true, exclude);
  EXPECT_NE(node_info.node_id, node_info2.node_id);

  exclude.push_back(nodes_id[0].string());
  node_info = routing_table.GetClosestNode(nodes_id[0], false, exclude);
  node_info2 = routing_table.GetClosestNode(nodes_id[0], true, exclude);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);
  EXPECT_EQ(node_info.node_id, NodeInfo().node_id);

  // routing_table with Parameters::group_size elements
  exclude.clear();
  for (uint16_t i(static_cast<uint16_t>(routing_table.size()));
       routing_table.size() < Parameters::group_size; ++i) {
    NodeInfo node(MakeNode());
    nodes_id.push_back(node.node_id);
    EXPECT_TRUE(routing_table.AddNode(node));
  }

  node_info = routing_table.GetClosestNode(my_node, false, exclude);
  node_info2 = routing_table.GetClosestNode(my_node, true, exclude);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);

  uint16_t random_index = RandomUint32() % Parameters::group_size;
  node_info = routing_table.GetClosestNode(nodes_id[random_index], false, exclude);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], true, exclude);
  EXPECT_NE(node_info.node_id, node_info2.node_id);

  exclude.push_back(nodes_id[random_index].string());
  node_info = routing_table.GetClosestNode(nodes_id[random_index], false, exclude);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], true, exclude);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);

  for (const auto& node_id : nodes_id)
    exclude.push_back(node_id.string());
  node_info = routing_table.GetClosestNode(nodes_id[random_index], false, exclude);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], true, exclude);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);
  EXPECT_EQ(node_info.node_id, NodeInfo().node_id);

  // routing_table with Parameters::Parameters::max_routing_table_size elements
  exclude.clear();
  for (uint16_t i = static_cast<uint16_t>(routing_table.size());
       routing_table.size() < Parameters::max_routing_table_size; ++i) {
    NodeInfo node(MakeNode());
    nodes_id.push_back(node.node_id);
    EXPECT_TRUE(routing_table.AddNode(node));
  }

  node_info = routing_table.GetClosestNode(my_node, false, exclude);
  node_info2 = routing_table.GetClosestNode(my_node, true, exclude);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);

  random_index = RandomUint32() % Parameters::max_routing_table_size;
  node_info = routing_table.GetClosestNode(nodes_id[random_index], false, exclude);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], true, exclude);
  EXPECT_NE(node_info.node_id, node_info2.node_id);

  exclude.push_back(nodes_id[random_index].string());
  node_info = routing_table.GetClosestNode(nodes_id[random_index], false, exclude);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], true, exclude);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);

  for (const auto& node_id : nodes_id)
    exclude.push_back(node_id.string());
  node_info = routing_table.GetClosestNode(nodes_id[random_index], false, exclude);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], true, exclude);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);
  EXPECT_EQ(node_info.node_id, NodeInfo().node_id);
}

TEST(RoutingTableTest, FUNC_ClosestToId) {
  NodeId own_node_id(NodeId::IdType::kRandomId);
  NetworkStatistics network_statistics(own_node_id);
  RoutingTable routing_table(false, own_node_id, asymm::GenerateKeyPair(), network_statistics);
  std::vector<NodeInfo> known_nodes;
  std::vector<NodeInfo> known_targets;
  NodeId target;
  NodeInfo node_info;
  NodeId furthest_group_node;

  auto test_known_ids = [&, this]()->bool {
    LOG(kInfo) << "\tTesting known ids...";
    bool passed(true);
    bool result(false);
    bool expectation(false);
    for (const auto& target : known_targets) {
      PartialSortFromTarget(target.node_id, known_nodes, 2);
      result = routing_table.IsThisNodeClosestTo(target.node_id, true);
      expectation = false;
      if (NodeId::CloserToTarget(own_node_id, known_nodes.at(1).node_id, target.node_id))
        expectation = true;
      EXPECT_EQ(expectation, result);
      if (expectation != result)
        passed = false;
    }
    return passed;
  };  // NOLINT

  auto test_unknown_ids = [&, this]()->bool {
    LOG(kInfo) << "\tTesting unknown ids...";
    bool passed(true);
    bool result(false);
    bool expectation(false);
    for (uint16_t i(0); i < 200; ++i) {
      target = NodeId(NodeId::IdType::kRandomId);
      PartialSortFromTarget(target, known_nodes, 1);
      result = routing_table.IsThisNodeClosestTo(target, true);
      expectation = false;
      if (NodeId::CloserToTarget(own_node_id, known_nodes.at(0).node_id, target) &&
          !NodeId::CloserToTarget(furthest_group_node, target, own_node_id))
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

  for (uint16_t i(0); i < 200; ++i) {
    target = NodeId(NodeId::IdType::kRandomId);
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
  furthest_group_node = known_nodes.at(Parameters::group_size - 2).node_id;

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
  furthest_group_node = known_nodes.at(Parameters::group_size - 2).node_id;

  LOG(kInfo) << "Testing fully populated routing table...";
  EXPECT_FALSE(routing_table.IsThisNodeClosestTo(own_node_id, true));
  EXPECT_TRUE(test_known_ids());
  EXPECT_TRUE(test_unknown_ids());
}

TEST(RoutingTableTest, FUNC_GetRandomExistingNode) {
  NodeId own_node_id(NodeId::IdType::kRandomId);
  NetworkStatistics network_statistics(own_node_id);
  RoutingTable routing_table(false, own_node_id, asymm::GenerateKeyPair(), network_statistics);
  NodeInfo node_info;
  std::vector<NodeInfo> known_nodes;

#ifndef NDEBUG
    EXPECT_DEATH(routing_table.RandomConnectedNode(), "");
#else
    EXPECT_TRUE(routing_table.RandomConnectedNode().IsZero());
#endif
  auto run_random_connected_node_test = [&] () {
    NodeId random_connected_node_id = routing_table.RandomConnectedNode();
    LOG(kVerbose) << "Got random connected node: " << DebugId(random_connected_node_id);
    auto found(std::find_if(std::begin(known_nodes), std::end(known_nodes),
                            [=] (const NodeInfo& node) {
                              return (node.node_id ==  random_connected_node_id);
                            }));
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
