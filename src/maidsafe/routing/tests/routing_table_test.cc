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
  NodeId node_id(NodeId::kRandomId);
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
  NodeId node_id(NodeId::kRandomId);
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

TEST(RoutingTableTest, BEH_PopulateAndDepopulateGroupCheckGroupChange) {
  NodeId node_id(NodeId::kRandomId);
  NetworkStatistics network_statistics(node_id);
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair(), network_statistics);
  std::vector<NodeInfo> nodes;

  for (uint16_t i = 0; i < Parameters::closest_nodes_size; ++i)
    nodes.push_back(MakeNode());

  int count(0);
  NetworkStatusFunctor network_status_functor = [](const int & status) {
    LOG(kVerbose) << "Status : " << status;
  };
  std::function<void(const NodeInfo&, bool)> remove_node_functor = [](
      const NodeInfo&, bool) { LOG(kVerbose) << "RemoveNodeFunctor!"; };  // NOLINT
  ConnectedGroupChangeFunctor group_change_functor = [&count](const std::vector<NodeInfo> nodes,
                                                              const std::vector<NodeInfo>) {
    ++count;
    LOG(kInfo) << "Group changed. count : " << count;
    EXPECT_GE(Parameters::closest_nodes_size, count);
    for (const auto& i : nodes) {
      LOG(kVerbose) << "NodeId : " << DebugId(i.node_id);
    }
  };
  routing_table.InitialiseFunctors(network_status_functor, remove_node_functor, []() {},
                                   group_change_functor, [](std::shared_ptr<MatrixChange>) {});
  for (uint16_t i = 0; i < Parameters::closest_nodes_size; ++i) {
    ASSERT_TRUE(routing_table.AddNode(nodes.at(i)));
    LOG(kVerbose) << "Added to routing_table : " << DebugId(nodes.at(i).node_id);
  }

  Sleep(std::chrono::microseconds(200));
  EXPECT_EQ(Parameters::closest_nodes_size, count);
  ASSERT_EQ(routing_table.size(), Parameters::closest_nodes_size);

  // Remove nodes from routing table
  count = 0;
  for (uint16_t i = 0; i < Parameters::closest_nodes_size; ++i) {
    routing_table.DropNode(nodes.at(i).node_id, true);
    LOG(kVerbose) << "Dropped from routing_table : " << DebugId(nodes.at(i).node_id);
  }

  EXPECT_EQ(0, routing_table.size());
}

TEST(RoutingTableTest, FUNC_OrderedGroupChange) {
  NodeId node_id(NodeId::kRandomId);
  NetworkStatistics network_statistics(node_id);
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair(), network_statistics);
  std::vector<NodeInfo> nodes;

  for (uint16_t i = 0; i < Parameters::max_routing_table_size; ++i)
    nodes.push_back(MakeNode());

  SortFromTarget(routing_table.kNodeId(), nodes);

  int count(0);
  ConnectedGroupChangeFunctor group_change_functor([&count](const std::vector<NodeInfo> nodes,
                                                            const std::vector<NodeInfo>) {
    ++count;
    LOG(kInfo) << "Group changed. count : " << count;
    EXPECT_GE(Parameters::closest_nodes_size, count);
    for (const auto& i : nodes) {
      LOG(kVerbose) << "NodeId : " << DebugId(i.node_id);
    }
  });

  routing_table.InitialiseFunctors([](const int &
                                      status) { LOG(kVerbose) << "Status : " << status; },
                                   [](const NodeInfo&, bool) {}, []() {}, group_change_functor,
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

TEST(RoutingTableTest, FUNC_ReverseOrderedGroupChange) {
  NodeId node_id(NodeId::kRandomId);
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

TEST(RoutingTableTest, FUNC_CheckGroupChangeRemoveNodesFromGroup) {
  NodeId node_id(NodeId::kRandomId);
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

TEST(RoutingTableTest, FUNC_CheckGroupChangeAddGroupNodesToFullTable) {
  NodeId node_id(NodeId::kRandomId);
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

TEST(RoutingTableTest, FUNC_FillEmptyRefillRoutingTable) {
  NodeId node_id(NodeId::kRandomId);
  NetworkStatistics network_statistics(node_id);
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair(), network_statistics);

  NodeId own_node_id(routing_table.kNodeId());
  std::vector<NodeInfo> nodes;
  for (uint16_t i = 0; i < Parameters::max_routing_table_size; ++i)
    nodes.push_back(MakeNode());

  // Set functors
  NetworkStatusFunctor network_status_functor = [](const int & status) {
    LOG(kVerbose) << "Status : " << status;
  };
  std::function<void(const NodeInfo&, bool)> remove_node_functor = [](
      const NodeInfo&, bool) { LOG(kVerbose) << "RemoveNodeFunctor!"; };  // NOLINT
  int expected_count(0);
  std::vector<NodeInfo> expected_group;
  int count(0);
  bool filling_table(true);
  ConnectedGroupChangeFunctor group_change_functor = [&](const std::vector<NodeInfo> node_infos,
                                                         const std::vector<NodeInfo>) {
    ++count;
    LOG(kInfo) << "Group changed. count : " << count;
    if (filling_table) {
      EXPECT_EQ(expected_count, count);
      for (auto& node : expected_group) {
        auto node_id(node.node_id);
        EXPECT_NE(std::find_if(node_infos.begin(), node_infos.end(),
                               [node_id](const NodeInfo&
                                         node_info) { return node_id == node_info.node_id; }),
                  node_infos.end());
      }
    } else {
//      EXPECT_EQ(expected_count, count);
      EXPECT_GE(nodes.size(), node_infos.size());
      for (const auto& node_info : node_infos)
        EXPECT_NE(std::find_if(std::begin(nodes), std::end(nodes),
                  [&](const NodeInfo& nodes_node_info) {
                    return nodes_node_info.node_id == node_info.node_id;
                  }), std::end(nodes));
    }
    for (const auto& node_info : node_infos) {
      LOG(kVerbose) << "NodeId : " << DebugId(node_info.node_id);
    }
  };
  routing_table.InitialiseFunctors(network_status_functor, remove_node_functor, []() {},
                                   group_change_functor, [](std::shared_ptr<MatrixChange>) {});
  // Fill routing table
  for (uint16_t i(0); i < Parameters::max_routing_table_size; ++i) {
    if (expected_group.size() < Parameters::closest_nodes_size) {
      ++expected_count;
      expected_group.push_back(nodes.at(i));
      SortFromTarget(own_node_id, expected_group);
    } else if ((own_node_id ^ nodes.at(i).node_id) <
               (own_node_id ^ expected_group.at(Parameters::closest_nodes_size - 1).node_id)) {
      ++expected_count;
      expected_group.pop_back();
      expected_group.push_back(nodes.at(i));
      SortFromTarget(own_node_id, expected_group);
    }

    ASSERT_TRUE(routing_table.AddNode(nodes.at(i)));
    LOG(kVerbose) << "Added to routing_table : " << DebugId(nodes.at(i).node_id);
  }

  EXPECT_EQ(expected_count, count);
  ASSERT_EQ(routing_table.size(), Parameters::max_routing_table_size);

  // Reset functor arguments
  expected_count = 0;
  SortFromTarget(own_node_id, nodes);
  count = 0;
  filling_table = false;

  // Empty routing table
  while (nodes.size() > 0) {
    uint32_t removal_index(RandomUint32() % nodes.size());
    NodeId removal_id(nodes.at(removal_index).node_id);
    nodes.erase(nodes.begin() + removal_index);
    if (nodes.size() < Parameters::closest_nodes_size) {
      ++expected_count;
    } else if ((own_node_id ^ removal_id) <=
               (own_node_id ^ nodes.at(Parameters::closest_nodes_size - 1).node_id)) {
      ++expected_count;
    }
    ++expected_count;
    routing_table.DropNode(removal_id, true);
    LOG(kVerbose) << "Removed from routing_table : " << DebugId(removal_id);
  }

  ASSERT_EQ(0, routing_table.size());

  // Reset functors
  expected_count = 0;
  expected_group.clear();
  count = 0;
  filling_table = true;
  for (uint16_t i = 0; i < Parameters::max_routing_table_size; ++i)
    nodes.push_back(MakeNode());

  // Refill routing table
  for (uint16_t i = 0; i < Parameters::max_routing_table_size; ++i) {
    if (expected_group.size() < Parameters::closest_nodes_size) {
      ++expected_count;
      expected_group.push_back(nodes.at(i));
      SortFromTarget(own_node_id, expected_group);
    } else if ((own_node_id ^ nodes.at(i).node_id) <
               (own_node_id ^ expected_group.at(Parameters::closest_nodes_size - 1).node_id)) {
      ++expected_count;
      expected_group.pop_back();
      expected_group.push_back(nodes.at(i));
      SortFromTarget(own_node_id, expected_group);
    }

    ASSERT_TRUE(routing_table.AddNode(nodes.at(i)));
    LOG(kVerbose) << "Added to routing_table : " << DebugId(nodes.at(i).node_id);
  }

  EXPECT_EQ(expected_count, count);
  ASSERT_EQ(routing_table.size(), Parameters::max_routing_table_size);
}

TEST(RoutingTableTest, BEH_CheckMockSendGroupChangeRpcs) {
  // Set up 1
//  NodeId node_id(NodeId::kRandomId);
  NodeId node_id("fa759d086f45592fb78538c1bc1990bc5d9797ef13d3eddfb427bc578de71a76cad53d5cd2cee871332342558afbba4b8ddf8bc05cad7fe01a7e002af51f7fcb", NodeId::EncodingType::kHex);
  LOG(kAlways) << "  NodeId node_id(\"" << node_id.ToStringEncoded(NodeId::EncodingType::kHex) << "\", NodeId::EncodingType::kHex);";
  NetworkStatistics network_statistics(node_id);
  RoutingTable routing_table_1(false, node_id, asymm::GenerateKeyPair(), network_statistics);
  NodeInfo node_info_1(MakeNode());
  node_info_1.node_id = routing_table_1.kNodeId();
  node_info_1.connection_id = node_info_1.node_id;
  NodeId node_id_1(routing_table_1.kNodeId());
  ASSERT_EQ(routing_table_1.kNodeId(), node_info_1.node_id);

  // Generate other NodeInfo
  std::vector<NodeInfo> extra_nodes;
  //uint16_t limit(2 * Parameters::closest_nodes_size + 1);
  //for (uint16_t i = 0; i < limit; ++i)
  //  extra_nodes.push_back(MakeNode());
  extra_nodes.push_back(MakeNode("daab2c7b04e68d663d207f61a9132aab68ab9e11287d98711ac82bbcb477a9ec4d742bc3636f30a541c162809a001b521986e08d892301e0731661651ad5db87"));
  extra_nodes.push_back(MakeNode("b432bf3c64ed97dc98d903c7ac803d77162c3e690e50fa2a19cd4a9d5a50f66fb39470bacde5632f79e63ed4d82121981c6f094bd926429bf019d2c15c399dde"));
  extra_nodes.push_back(MakeNode("d80f46456fdcaa79c7e7b369e5628bcc0090175aae8dfb694dbd3634134b8df2777b582c26473d24dbdc22c24db88f195c9581b930d39b753bd588bca31197f2"));
  extra_nodes.push_back(MakeNode("095fd67f69740e090aa8ca7cf2eb0a249d19276e268d9119cb0cb3115cef5ca3d45dd7cb7d809a5ea1a9aa001cce295825c2fdf0d0b276c7ea97f84a3741badc"));
  extra_nodes.push_back(MakeNode("1946fa071cdac47c767fdc0b3335c9d78473485dea59c1012d3628fa171f7366a9ca9c24d1e57ff009be6ef036b440dcceba0f116a87604e67221a1d0446d2ed"));
  extra_nodes.push_back(MakeNode("b052a4c702133d5beb96e143c5c56e5d6aa167743599084b49234b49f5084d7586a744458d9db88a8ed197cc69115f8bad7f821638cb3e4e40e9263fc4c7bec3"));
  extra_nodes.push_back(MakeNode("e30bb869a955472f0e5efe3750141d578091cbe46f4a27b5266b3fb00ece111a2e2bb380ed0e59ddb14818036c7f4a55c40b25b031ee822bdf2d6ad2ca250fc8"));
  extra_nodes.push_back(MakeNode("e03d439281918fb51ba2d6c1f44712197d9b96467a436f0bf5b413be4f654449311ab27c9c928dc9f71adfc5da0ddf455dabd99072ba74d7a9b405340cdc36e2"));
  extra_nodes.push_back(MakeNode("fff30913a28beba805291d63dacb9dc114919d307cf0dc033845ac979ca6c6a61f59215f632b3a3803d7e2e1556617a8b04b49fff900e6d7dac744f5e6ac3c57"));
  extra_nodes.push_back(MakeNode("57041c9a1cf1594ee1a0c2204df536c48107514bdd938710668613192a747f30a46e1dfc076e20dc45ff17ff3e33ca15e433cdc30aa84ff1fa6d7510408bf72a"));
  extra_nodes.push_back(MakeNode("fd9aed15a8e0d375f8a826569693cd02d6797d316eec3ec450736c45767f46bb80af762bf0de1fa2bf01bf92194ac84c3bc9f3bf3b95ffdbc4df1001f9ca642d"));
  extra_nodes.push_back(MakeNode("2b980716871ac3cdb4601947bacd2c8260216c13479d14c12512589cebafa48763768f4b02b745e15d2defd4329e5759ca403f72c7018ab9528e1b587cba90c1"));
  extra_nodes.push_back(MakeNode("ba5cc1fb671be6145c96de36220bb4f22b37c5ea23922d8b5bf11d62889357c5a3db685912da6578e295ee9b94e7c5a3266ecba3d3840f4ecff9b015e1579811"));
  extra_nodes.push_back(MakeNode("8699a45121c05aeeb95a432b8d3fd456c0a0c2c582f0f4af99654838a8e7ac73f91bc1fbf1c0b4c40fe410601d05fd7007b605774288ba7d3aad32447938d331"));
  extra_nodes.push_back(MakeNode("06172d31148ae24ddbad1229fc3ac19d4d2875255184d8a166ee6180f64d3a66e940919f8f94c1e98dd521257a1381fb0b55a56f067a725fe838b909e7dfdd65"));
  extra_nodes.push_back(MakeNode("7c6a934c33ae7ee8af9118c690c461d58d8cc7a3728f9e1f0b3f2584c538a922d3504153082d78e8c674893660cc462356a323064b3f6b0a8f501bfacf322c8f"));
  extra_nodes.push_back(MakeNode("92808a42df91e40621c8b6be68bf0838cde23b14f4bf0bec35829db5f4b72f5302c263ad15ce3c2994affe835b0fbbb50c90a0d9e2448701472915d9ea1b89d0"));
  for (auto& node : extra_nodes)
    LOG(kAlways) << "  extra_nodes.push_back(MakeNode(\"" << node.node_id.ToStringEncoded(NodeId::EncodingType::kHex) << "\"));";
  SortFromTarget(node_id_1, extra_nodes);

  // Set up 2 to be in 1's closest_nodes_size group
//  uint16_t index_2(RandomUint32() % Parameters::closest_nodes_size);
  uint16_t index_2 = 0;
  LOG(kAlways) << "  uint16_t index_2 = " << index_2 << ";";
  RoutingTable routing_table_2(false, extra_nodes.at(index_2).node_id, asymm::GenerateKeyPair(),
                               network_statistics);

  NodeInfo node_info_2(extra_nodes.at(index_2));
  ASSERT_EQ(routing_table_2.kNodeId(), node_info_2.node_id);
  ASSERT_EQ(node_info_2.connection_id, node_info_2.node_id);
  extra_nodes.erase(extra_nodes.begin() + index_2);

  // Add 2 to 1
  ASSERT_TRUE(routing_table_1.AddNode(node_info_2));
  EXPECT_EQ(1, routing_table_1.group_matrix_.GetConnectedPeers().size());

  // Add 1 to 2
  ASSERT_TRUE(routing_table_2.AddNode(node_info_1));
  EXPECT_EQ(1, routing_table_2.group_matrix_.GetConnectedPeers().size());

  // Set group_change_functor for routing_table_1
  bool expecting_group_change(true);
  int count(0);
  int expected_count(0);
  NetworkStatusFunctor network_status_functor = [](const int & status) {
    LOG(kVerbose) << "Status : " << status;
  };
  std::function<void(const NodeInfo&, bool)> remove_node_functor = [](
      const NodeInfo&, bool) { LOG(kVerbose) << "RemoveNodeFunctor!"; }; // NOLINT
  ConnectedGroupChangeFunctor group_change_functor =
      [&node_id_1, &routing_table_2, &expecting_group_change, &count, &expected_count](
          const std::vector<NodeInfo> nodes, const std::vector<NodeInfo> /*old_nodes*/) {
    ++count;
    EXPECT_EQ(count, expected_count);
    EXPECT_TRUE(expecting_group_change);
    bool found_2(false);
    std::vector<NodeInfo> close_nodes;
    for (const auto& node_info : nodes) {
      if (node_info.node_id == routing_table_2.kNodeId())
        found_2 = true;
      else
        close_nodes.push_back(node_info);
    }
    EXPECT_TRUE(found_2);
    if (!found_2) {
      LOG(kError) << "Haven't found NodeId for routing_table_2 in group change!";
    } else {
      routing_table_2.GroupUpdateFromConnectedPeer(node_id_1, close_nodes);
    }
    LOG(kVerbose) << "NodeIds for routing_table_1 (group change):";
    for (auto& j : nodes) {
      LOG(kVerbose) << "NodeId : " << DebugId(j.node_id);
    }
  };
  routing_table_1.InitialiseFunctors(network_status_functor, remove_node_functor, []() {},
                                     group_change_functor, [](std::shared_ptr<MatrixChange>) {});

  // Check that 2's group matrix is updated correctly - Add nodes
  std::vector<NodeInfo> close_nodes;
//  NodeId rand_node_id(NodeId::kRandomId);
  NodeId rand_node_id("4555129c606912ea695237da60422f4e7d129defd586370d0bdc6aeabab4f06081fdd71afa54bb098ae286325435ee590fb42045a98f3063ffac117a863bb6af", NodeId::EncodingType::kHex);
  LOG(kAlways) << "  NodeId rand_node_id(\"" << rand_node_id.ToStringEncoded(NodeId::EncodingType::kHex) << "\", NodeId::EncodingType::kHex);";
  SortFromTarget(rand_node_id, extra_nodes);
  std::vector<NodeId> expected_close_nodes(1, node_info_2.node_id);
  for (const auto& node_info : extra_nodes) {
    SortIdsFromTarget(node_id_1, expected_close_nodes);
    EXPECT_EQ(expected_count, count);
    if (expected_close_nodes.size() < size_t(Parameters::closest_nodes_size)) {
      expected_close_nodes.push_back(node_info.node_id);
      ++expected_count;
      expecting_group_change = true;
    } else if (!NodeId::CloserToTarget(expected_close_nodes.at(Parameters::closest_nodes_size - 1),
                                       node_info.node_id, node_id_1)) {
      expected_close_nodes.pop_back();
      expected_close_nodes.push_back(node_info.node_id);
      ++expected_count;
      expecting_group_change = true;
    } else {
      expecting_group_change = false;
    }
    ASSERT_TRUE(routing_table_1.AddNode(node_info));
    EXPECT_TRUE(routing_table_2.group_matrix_.GetRow(node_id_1, close_nodes));
    for (uint16_t i(0); i < std::min(expected_close_nodes.size(), close_nodes.size()); ++i) {
      auto id(expected_close_nodes.at(i));
      if (id != node_info_2.node_id)
        EXPECT_NE(std::find_if(close_nodes.begin(), close_nodes.end(),
                               [id](const NodeInfo& info) { return id == info.node_id; }),
                  close_nodes.end()) << " missing expected node " << DebugId(id);
    }
  }

  EXPECT_EQ(2 * Parameters::closest_nodes_size + 1, routing_table_1.size());
  EXPECT_EQ(expected_count, count);
  EXPECT_LE(Parameters::closest_nodes_size - 1, count);

  // Check that 2's group matrix is updated correctly - Drop nodes
  SortFromTarget(node_id_1, extra_nodes);
  count = 0;
  expected_count = 0;
  NodeInfo removal_node;
  uint16_t removal_index;
  std::reverse(std::begin(extra_nodes), std::end(extra_nodes));
  while (extra_nodes.size() > 0) {
//    removal_index = static_cast<uint16_t>(RandomUint32() % extra_nodes.size());
    removal_index = 0;
    removal_node = extra_nodes.at(removal_index);
    LOG(kWarning) << "Removing " << DebugId(removal_node.node_id);
    extra_nodes.erase(extra_nodes.begin() + removal_index);
    auto connected_nodes(routing_table_1.group_matrix_.GetConnectedPeers());
    if (std::find_if(std::begin(connected_nodes), std::end(connected_nodes),
                     [&](const NodeInfo& connected_node) {
                       return connected_node.node_id == removal_node.node_id;
                     }) != std::end(connected_nodes)) {
      expected_close_nodes.clear();
      int extra_nodes_size = static_cast<int>(extra_nodes.size());
      for (int i(0); i < std::min(extra_nodes_size, Parameters::closest_nodes_size - 1); ++i) {
        expected_close_nodes.push_back(extra_nodes.at(i).node_id);
      }
      ++expected_count;
      expecting_group_change = true;
    } else {
      expecting_group_change = false;
    }
    routing_table_1.DropNode(removal_node.node_id, true);
    EXPECT_TRUE(routing_table_2.group_matrix_.GetRow(node_id_1, close_nodes));
    for (uint16_t i(0); i < std::min(expected_close_nodes.size(), close_nodes.size()); ++i) {
      auto id(close_nodes.at(i).node_id);
      EXPECT_NE(std::find(expected_close_nodes.begin(), expected_close_nodes.end(), id),
                expected_close_nodes.end());
    }
  }

  EXPECT_EQ(1, routing_table_1.size());
  EXPECT_EQ(2, routing_table_2.group_matrix_.GetUniqueNodes().size());
  EXPECT_EQ(expected_count, count);
  EXPECT_LE(Parameters::closest_nodes_size - 1, count);
}

TEST(RoutingTableTest, BEH_GroupUpdateFromConnectedPeer) {
  NodeId node_id(NodeId::kRandomId);
  NetworkStatistics network_statistics(node_id);
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair(), network_statistics);
  std::vector<NodeInfo> nodes;

  for (uint16_t i(0); i < Parameters::closest_nodes_size; ++i)
    nodes.push_back(MakeNode());

  std::vector<NodeInfo> row_contents;
  for (const auto& node_info : nodes) {
    EXPECT_TRUE(routing_table.AddNode(node_info));
    NodeInfo node;
    node.node_id = (NodeId(NodeId::kRandomId));
    row_contents.push_back(node);
    EXPECT_TRUE(routing_table.group_matrix_.GetRow(node_info.node_id, row_contents));
    EXPECT_EQ(0, row_contents.size());
  }

  std::vector<NodeInfo> new_row_entries;
  for (const auto& node_info : nodes) {
    for (uint16_t i(0); i < RandomUint32() % (Parameters::closest_nodes_size - 1); ++i) {
      NodeInfo node;
      node.node_id = (NodeId(NodeId::kRandomId));
      new_row_entries.push_back(node);
    }
    routing_table.GroupUpdateFromConnectedPeer(node_info.node_id, new_row_entries);
    EXPECT_TRUE(routing_table.group_matrix_.GetRow(node_info.node_id, row_contents));
    EXPECT_EQ(new_row_entries.size(), row_contents.size());
    for (uint16_t i(0); i < std::min(new_row_entries.size(), row_contents.size()); ++i)
      EXPECT_EQ(row_contents.at(i).node_id, new_row_entries.at(i).node_id);
    new_row_entries.clear();
  }
}

TEST(RoutingTableTest, BEH_GetNthClosest) {
  std::vector<NodeId> nodes_id;
  NodeId node_id(NodeId::kRandomId);
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
  NodeId node_id(NodeId::kRandomId);
  NetworkStatistics network_statistics(node_id);
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair(), network_statistics);
  std::vector<NodeId> nodes_id;
  std::vector<std::string> exclude;
  NodeInfo node_info;
  NodeId my_node(routing_table.kNodeId());

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

  exclude.push_back(nodes_id[0].string());
  node_info = routing_table.GetClosestNode(nodes_id[0], exclude, false);
  node_info2 = routing_table.GetClosestNode(nodes_id[0], exclude, true);
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

  node_info = routing_table.GetClosestNode(my_node, exclude, false);
  node_info2 = routing_table.GetClosestNode(my_node, exclude, true);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);

  uint16_t random_index = RandomUint32() % Parameters::group_size;
  node_info = routing_table.GetClosestNode(nodes_id[random_index], exclude, false);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], exclude, true);
  EXPECT_NE(node_info.node_id, node_info2.node_id);

  exclude.push_back(nodes_id[random_index].string());
  node_info = routing_table.GetClosestNode(nodes_id[random_index], exclude, false);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], exclude, true);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);

  for (const auto& node_id : nodes_id)
    exclude.push_back(node_id.string());
  node_info = routing_table.GetClosestNode(nodes_id[random_index], exclude, false);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], exclude, true);
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

  node_info = routing_table.GetClosestNode(my_node, exclude, false);
  node_info2 = routing_table.GetClosestNode(my_node, exclude, true);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);

  random_index = RandomUint32() % Parameters::max_routing_table_size;
  node_info = routing_table.GetClosestNode(nodes_id[random_index], exclude, false);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], exclude, true);
  EXPECT_NE(node_info.node_id, node_info2.node_id);

  exclude.push_back(nodes_id[random_index].string());
  node_info = routing_table.GetClosestNode(nodes_id[random_index], exclude, false);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], exclude, true);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);

  for (const auto& node_id : nodes_id)
    exclude.push_back(node_id.string());
  node_info = routing_table.GetClosestNode(nodes_id[random_index], exclude, false);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], exclude, true);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);
  EXPECT_EQ(node_info.node_id, NodeInfo().node_id);
}

TEST(RoutingTableTest, BEH_IsThisNodeGroupLeader) {
  // Tests routing table's handling of 2 basic cases. More detailed cases in group matrix tests.

  // Setting up
  std::vector<NodeInfo> nodes;
  NodeInfo node_info;

  size_t nodes_size(static_cast<size_t>(Parameters::closest_nodes_size + 1));
  while (nodes.size() < nodes_size) {
    node_info = MakeNode();
    nodes.push_back(node_info);
  }
  NodeId target_id(NodeId::kRandomId);
  NodeId inverse_target_id(target_id ^ NodeId(NodeId::kMaxId));
  SortFromTarget(target_id, nodes);
  NodeId own_node_id(nodes.back().node_id);
  nodes.pop_back();

  NetworkStatistics network_statistics(own_node_id);
  RoutingTable routing_table(false, own_node_id, asymm::GenerateKeyPair(), network_statistics);

  SortFromTarget(NodeId(NodeId::kRandomId), nodes);
  for (const auto& node : nodes) {
    EXPECT_TRUE(routing_table.AddNode(node));
  }

  SortFromTarget(target_id, nodes);
  NodeInfo connected_peer;

  // Test 'true' version
  EXPECT_TRUE(routing_table.IsThisNodeGroupLeader(inverse_target_id, connected_peer));
  EXPECT_TRUE(connected_peer.node_id.IsZero());
  EXPECT_TRUE(connected_peer.connection_id.IsZero());

  // Test 'false' version
  EXPECT_FALSE(routing_table.IsThisNodeGroupLeader(target_id, connected_peer));
  EXPECT_EQ(connected_peer.node_id, nodes.at(0).node_id);
  EXPECT_EQ(connected_peer.connection_id, nodes.at(0).connection_id);
  EXPECT_TRUE(maidsafe::asymm::MatchingKeys(connected_peer.public_key, nodes.at(0).public_key));
}

TEST(RoutingTableTest, FUNC_IsConnected) {
  NodeId own_node_id(NodeId::kRandomId);
  NetworkStatistics network_statistics(own_node_id);
  RoutingTable routing_table(false, own_node_id, asymm::GenerateKeyPair(), network_statistics);

  std::vector<NodeInfo> nodes_in_table;
  NodeInfo node_info;
  // Populate routing table
  for (uint16_t i(0); i < Parameters::max_routing_table_size; ++i) {
    node_info = MakeNode();
    nodes_in_table.push_back(node_info);
    EXPECT_TRUE(routing_table.AddNode(node_info));
  }

  // Populate group matrix
  SortFromTarget(own_node_id, nodes_in_table);
  std::vector<NodeInfo> matrix_row_leaders(nodes_in_table.begin(),
                                           nodes_in_table.begin() + Parameters::closest_nodes_size);
  std::vector<std::vector<NodeInfo>> rows_in_matrix;
  std::vector<NodeInfo> row;
  uint32_t row_size;
  for (const auto& row_leader : matrix_row_leaders) {
    row.clear();
    row_size = 1 + RandomUint32() % (Parameters::closest_nodes_size - 1);
    while (row.size() < row_size)
      row.push_back(MakeNode());

    rows_in_matrix.push_back(row);
    routing_table.GroupUpdateFromConnectedPeer(row_leader.node_id, row);
  }

  // IsConnected - nodes in routing table
  for (const auto& node : nodes_in_table)
    EXPECT_TRUE(routing_table.IsConnected(node.node_id));

  // IsConnected - nodes in rows of group matrix
  for (const auto& row : rows_in_matrix) {
    for (const auto& node : row) {
      EXPECT_TRUE(routing_table.IsConnected(node.node_id));
    }
  }

  // IsConnected - nodes not in routing table or group matrix
  for (uint16_t i(0); i < 50; ++i)
    EXPECT_FALSE(routing_table.IsConnected(NodeId(NodeId::kRandomId)));
}

TEST(RoutingTableTest, FUNC_IsThisNodeClosestToIncludingMatrix) {
  NodeId own_node_id(NodeId::kRandomId);
  NodeInfo own_node_info;
  own_node_info.node_id = own_node_id;
  NetworkStatistics network_statistics(own_node_id);
  RoutingTable routing_table(false, own_node_id, asymm::GenerateKeyPair(), network_statistics);

  // Test IsThisNodeClosestToIncludingMatrix for empty routing table
  EXPECT_FALSE(routing_table.IsThisNodeClosestToIncludingMatrix(NodeId()));
  EXPECT_TRUE(routing_table.IsThisNodeClosestToIncludingMatrix(NodeId(NodeId::kRandomId)));

  std::vector<NodeInfo> nodes(1, own_node_info);
  NodeInfo node_info;

  // Add one node to routing table and test IsThisNodeClosestToIncludingMatrix
  node_info = MakeNode();
  nodes.push_back(node_info);
  EXPECT_TRUE(routing_table.AddNode(node_info));
  EXPECT_TRUE(routing_table.IsThisNodeClosestToIncludingMatrix(nodes.at(1).node_id, true));
  EXPECT_FALSE(routing_table.IsThisNodeClosestToIncludingMatrix(nodes.at(1).node_id, false));

  // Partially populate routing table
  for (uint16_t i(0); i < Parameters::max_routing_table_size / 2; ++i) {
    node_info = MakeNode();
    nodes.push_back(node_info);
    EXPECT_TRUE(routing_table.AddNode(node_info));
  }

  // Test IsThisNodeClosestToIncludingMatrix (empty matrix)
  for (const auto& node : nodes) {
    if (node.node_id != own_node_id) {
      EXPECT_TRUE(routing_table.Contains(node.node_id));
      EXPECT_FALSE(routing_table.IsThisNodeClosestToIncludingMatrix(node.node_id));
    }
  }

  // Populate group matrix
  SortFromTarget(own_node_id, nodes);
  std::vector<NodeInfo> row;
  uint32_t row_size;
  for (uint16_t i(1); i <= Parameters::closest_nodes_size; ++i) {
    row.clear();
    row_size = 1 + RandomUint32() % (Parameters::closest_nodes_size - 1);
    while (row.size() < row_size) {
      node_info = MakeNode();
      row.push_back(node_info);
      nodes.push_back(node_info);
    }
    routing_table.GroupUpdateFromConnectedPeer(nodes.at(i).node_id, row);
  }

  // Test IsThisNodeClosestToIncludingMatrix (populated matrix)
  NodeId target;
  for (uint16_t i(0); i < 100; ++i) {
    target = NodeId(NodeId::kRandomId);
    PartialSortFromTarget(target, nodes, 1);
    if (nodes.at(0).node_id == own_node_id)
      EXPECT_TRUE(routing_table.IsThisNodeClosestToIncludingMatrix(target));
    else
      EXPECT_FALSE(routing_table.IsThisNodeClosestToIncludingMatrix(target));
  }
}

TEST(RoutingTableTest, FUNC_GetNodeForSendingMessage) {
  NodeId own_node_id(NodeId::kRandomId);
  NetworkStatistics network_statistics(own_node_id);
  RoutingTable routing_table(false, own_node_id, asymm::GenerateKeyPair(), network_statistics);

  std::vector<NodeInfo> nodes_in_table;
  NodeInfo node_info;
  // Populate routing table
  for (uint16_t i(0); i < Parameters::max_routing_table_size; ++i) {
    node_info = MakeNode();
    nodes_in_table.push_back(node_info);
  }
  SortFromTarget(own_node_id, nodes_in_table);
  for (const auto& node : nodes_in_table)
    EXPECT_TRUE(routing_table.AddNode(node));

  // Populate group matrix
  std::vector<NodeInfo> matrix_row_leaders(nodes_in_table.begin(),
                                           nodes_in_table.begin() + Parameters::closest_nodes_size);
  std::vector<std::vector<NodeInfo>> rows_in_matrix;
  std::vector<NodeInfo> row;
  uint32_t row_size;
  for (const auto& row_leader : matrix_row_leaders) {
    row.clear();
    row_size = 1 + RandomUint32() % (Parameters::closest_nodes_size - 1);
    while (row.size() < row_size)
      row.push_back(MakeNode());

    rows_in_matrix.push_back(row);
    routing_table.GroupUpdateFromConnectedPeer(row_leader.node_id, row);
  }

  // Test GetNodeForSendingMessage for nodes in routing table
  std::vector<std::string> exclude;
  for (const auto& node : nodes_in_table) {
    EXPECT_EQ(node.node_id, routing_table.GetNodeForSendingMessage(node.node_id, exclude).node_id);
    EXPECT_NE(node.node_id,
              routing_table.GetNodeForSendingMessage(node.node_id, exclude, true).node_id);
  }

  // Test GetNodeForSendingMessage for nodes in group matrix (without exclusions)
  for (uint16_t i(0); i < matrix_row_leaders.size(); ++i) {
    for (const auto& row_entry : rows_in_matrix.at(i)) {
      EXPECT_EQ(matrix_row_leaders.at(i).node_id,
                routing_table.GetNodeForSendingMessage(row_entry.node_id, exclude).node_id);
    }
  }

  // Make all matrix rows the same
  for (const auto& row_leader : matrix_row_leaders) {
    routing_table.GroupUpdateFromConnectedPeer(row_leader.node_id, rows_in_matrix.at(0));
  }

  // Test GetNodeForSendingMessage for node in group matrix (with exclusions)
  NodeId target(rows_in_matrix.at(0).at(RandomUint32() % rows_in_matrix.at(0).size()).node_id);
  for (uint16_t i(0); i < matrix_row_leaders.size() - 1; ++i) {
    exclude.push_back(matrix_row_leaders.at(i).node_id.string());
    EXPECT_EQ(matrix_row_leaders.at(i + 1).node_id,
              routing_table.GetNodeForSendingMessage(target, exclude).node_id);
  }
}

TEST(RoutingTableTest, FUNC_GetNodeForSendingMessageIgnoreExactMatch) {
  // populate routing table
  NodeId own_node_id(NodeId::kRandomId);
  NetworkStatistics network_statistics(own_node_id);
  RoutingTable routing_table(false, own_node_id, asymm::GenerateKeyPair(), network_statistics);

  std::vector<NodeInfo> nodes_in_table;
  NodeInfo node_info;
  // Populate routing table
  for (uint16_t i(0); i < Parameters::max_routing_table_size; ++i) {
    node_info = MakeNode();
    nodes_in_table.push_back(node_info);
  }
  SortFromTarget(own_node_id, nodes_in_table);
  for (const auto& node : nodes_in_table)
    EXPECT_TRUE(routing_table.AddNode(node));

  // test GetNodeForSendingMessage
  std::vector<std::string> exclude;
  std::vector<NodeInfo> nodes2(nodes_in_table);
  for (const auto& node : nodes2) {
    PartialSortFromTarget(node.node_id, nodes_in_table, 2);
    EXPECT_EQ(nodes_in_table.at(1).node_id,
              routing_table.GetNodeForSendingMessage(node.node_id, exclude, true).node_id);
  }

  // Generate target and plant close node in group matrix
  NodeId target(NodeId::kRandomId);
  PartialSortFromTarget(target, nodes_in_table, 1);
  NodeInfo matrix_entry(MakeNode());
  bool generated_closer(false);
  while (!generated_closer) {
    matrix_entry.node_id = NodeId(NodeId::kRandomId);
    if (NodeId::CloserToTarget(matrix_entry.node_id, nodes_in_table.at(0).node_id, target))
      generated_closer = true;
  }
  LOG(kInfo) << "Matrix entry: " << DebugId(matrix_entry.node_id);
  std::vector<NodeInfo> row(1, matrix_entry);
  PartialSortFromTarget(own_node_id, nodes_in_table, Parameters::closest_nodes_size);
  uint16_t row_index(RandomUint32() % Parameters::closest_nodes_size);
  NodeInfo row_leader(nodes_in_table.at(row_index));
  LOG(kInfo) << "Row leader: " << DebugId(row_leader.node_id);
  routing_table.GroupUpdateFromConnectedPeer(row_leader.node_id, row);
  NodeInfo node_for_message(routing_table.GetNodeForSendingMessage(target, exclude, true));
  EXPECT_EQ(row_leader.node_id, node_for_message.node_id)
      << "For target: " << DebugId(target) << "\tExpected: " << DebugId(row_leader.node_id)
      << "\tGot: " << DebugId(node_for_message.node_id);
  PartialSortFromTarget(target, nodes_in_table, 1);
  LOG(kInfo) << "Closest table node: " << DebugId(nodes_in_table.at(0).node_id);
}

TEST(RoutingTableTest, FUNC_IsNodeIdInGroupRange) {
  NodeId own_node_id(NodeId::kRandomId);
  NetworkStatistics network_statistics(own_node_id);
  RoutingTable routing_table(false, own_node_id, asymm::GenerateKeyPair(), network_statistics);
  std::vector<NodeInfo> nodes_in_table;
  NodeInfo node_info;

  // Empty RT
  for (uint16_t i(0); i < 20; ++i) {
    EXPECT_EQ(GroupRangeStatus::kInRange,
              routing_table.IsNodeIdInGroupRange(NodeId(NodeId::kRandomId)));
  }

  // Partially populated RT, but not enough nodes to have 'furthest close' node
  uint16_t partial_size(1 + RandomUint32() % (Parameters::closest_nodes_size - 1));
  for (uint16_t i(0); i < partial_size; ++i) {
    node_info = MakeNode();
    nodes_in_table.push_back(node_info);
    EXPECT_TRUE(routing_table.AddNode(node_info));
  }
  for (uint16_t i(0); i < 20; ++i) {
    NodeId target(NodeId::kRandomId);
    EXPECT_NE(GroupRangeStatus::kOutwithRange, routing_table.IsNodeIdInGroupRange(target));
  }

  // Populated RT
  while (routing_table.size() < Parameters::max_routing_table_size) {
    node_info = MakeNode();
    nodes_in_table.push_back(node_info);
    EXPECT_TRUE(routing_table.AddNode(node_info));
  }
  SortFromTarget(own_node_id, nodes_in_table);

  NodeId fcn_node = nodes_in_table.at(Parameters::closest_nodes_size - 1).node_id;
  NodeId radius_id(own_node_id ^ fcn_node);
  crypto::BigInt radius((radius_id.ToStringEncoded(NodeId::EncodingType::kHex) + 'h').c_str());
  radius = Parameters::proximity_factor * radius;

  nodes_in_table = routing_table.group_matrix_.GetUniqueNodes();  // FIXME(Mahmoud)

  for (uint16_t i(0); i < 100; ++i) {
    NodeId target_id(NodeId::kRandomId);
    SortFromTarget(target_id, nodes_in_table);
    if (!NodeId::CloserToTarget(nodes_in_table.at(Parameters::group_size - 1).node_id,
                                own_node_id, target_id)) {
      EXPECT_EQ(GroupRangeStatus::kInRange, routing_table.IsNodeIdInGroupRange(target_id));
    } else {
      NodeId my_distance_id(own_node_id ^ target_id);
      crypto::BigInt my_distance(
          (my_distance_id.ToStringEncoded(NodeId::EncodingType::kHex) + 'h').c_str());
      if (my_distance < radius)
        EXPECT_EQ(GroupRangeStatus::kInProximalRange,
                  routing_table.IsNodeIdInGroupRange(target_id));
    }
  }
}

TEST(RoutingTableTest, BEH_MatrixChange) {
  NodeId node_id(NodeId::kRandomId);
  NetworkStatistics network_statistics(node_id);
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair(), network_statistics);
  int count(0);
  MatrixChangedFunctor matrix_change_functor = [&count](
      std::shared_ptr<MatrixChange> /*matrix_change*/) { count++; };  // NOLINT
  routing_table.InitialiseFunctors([](int) {}, [](const NodeInfo&, bool) {}, []() {},  // NOLINT
                                   [](const std::vector<NodeInfo>&,
                                      const std::vector<NodeInfo> /*old_nodes*/) {},
                                   matrix_change_functor);
  std::vector<NodeId> node_ids;
  for (size_t index(0); index < Parameters::closest_nodes_size; ++index) {
    NodeInfo node_info(MakeNode());
    node_info.node_id = NodeId(NodeId::kRandomId);
    node_ids.push_back(node_info.node_id);
    routing_table.AddNode(node_info);
  }
  EXPECT_EQ(count, Parameters::closest_nodes_size);
  routing_table.DropNode(node_ids.at(Parameters::group_size), true);
  node_ids.erase(std::remove(std::begin(node_ids), std::end(node_ids),
                             node_ids.at(Parameters::group_size)),
                 std::end(node_ids));
  EXPECT_EQ(count, Parameters::closest_nodes_size + 1);
  routing_table.GroupUpdateFromConnectedPeer(NodeId(NodeId::kRandomId), std::vector<NodeInfo>());
  EXPECT_EQ(count, Parameters::closest_nodes_size + 1);
  std::vector<NodeInfo> node_infos;
  for (size_t index(0); index < 10; ++index) {
    NodeInfo node_info(MakeNode());
    node_infos.push_back(node_info);
  }
  routing_table.GroupUpdateFromConnectedPeer(NodeId(NodeId::kRandomId), node_infos);
  EXPECT_EQ(count, Parameters::closest_nodes_size + 1);
  routing_table.GroupUpdateFromConnectedPeer(node_ids[RandomUint32() % node_ids.size()],
                                             node_infos);
  EXPECT_EQ(count, Parameters::closest_nodes_size + 2);
}

TEST(RoutingTableTest, FUNC_ClosestToId) {
  NodeId own_node_id(NodeId::kRandomId);
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
      result = routing_table.ClosestToId(target.node_id);
      expectation = false;
      if (NodeId::CloserToTarget(own_node_id, known_nodes.at(1).node_id, target.node_id) &&
          !NodeId::CloserToTarget(furthest_group_node, target.node_id, own_node_id))
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
      target = NodeId(NodeId::kRandomId);
      PartialSortFromTarget(target, known_nodes, 1);
      result = routing_table.ClosestToId(target);
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
  EXPECT_FALSE(routing_table.ClosestToId(own_node_id));

  for (uint16_t i(0); i < 200; ++i) {
    target = NodeId(NodeId::kRandomId);
    EXPECT_TRUE(routing_table.ClosestToId(target));
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
  EXPECT_FALSE(routing_table.ClosestToId(own_node_id));
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
  EXPECT_FALSE(routing_table.ClosestToId(own_node_id));
  EXPECT_TRUE(test_known_ids());
  EXPECT_TRUE(test_unknown_ids());

  // ------- Fully populated routing table and populated group matrix -------
  LOG(kInfo) << "Populating group matrix...";
  PartialSortFromTarget(own_node_id, known_nodes, Parameters::closest_nodes_size);

  std::vector<NodeInfo> new_row_entries;
  for (size_t index(0); index < Parameters::closest_nodes_size; ++index) {
    for (uint16_t i(0); i < RandomUint32() % (Parameters::closest_nodes_size - 1); ++i) {
      NodeInfo node;
      node.node_id = (NodeId(NodeId::kRandomId));
      new_row_entries.push_back(node);
      known_nodes.push_back(node);
      known_targets.push_back(node);
    }
    routing_table.GroupUpdateFromConnectedPeer(known_nodes.at(index).node_id, new_row_entries);
    new_row_entries.clear();
  }
  PartialSortFromTarget(own_node_id, known_nodes, Parameters::group_size);
  furthest_group_node = known_nodes.at(Parameters::group_size - 2).node_id;

  LOG(kInfo) << "Testing fully populated routing table with populated group matrix...";
  EXPECT_FALSE(routing_table.ClosestToId(own_node_id));
  EXPECT_TRUE(test_known_ids());
  EXPECT_TRUE(test_unknown_ids());
}

TEST(RoutingTableTest, FUNC_GetRandomExistingNode) {
  NodeId own_node_id(NodeId::kRandomId);
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
