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

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {
namespace routing {
namespace test {


TEST(RoutingTableTest, BEH_AddCloseNodes) {
  RoutingTable routing_table(false, NodeId(NodeId::kRandomId), asymm::GenerateKeyPair());
  NodeInfo node;
  // check the node is useful when false is set
  for (unsigned int i = 0; i < Parameters::closest_nodes_size ; ++i) {
     node.node_id = NodeId(RandomString(64));
     EXPECT_TRUE(routing_table.CheckNode(node));
  }
  EXPECT_EQ(routing_table.size(), 0);
  asymm::PublicKey dummy_key;
  // check we cannot input nodes with invalid public_keys
  for (uint16_t i = 0; i < Parameters::closest_nodes_size ; ++i) {
     NodeInfo node(MakeNode());
     node.public_key = dummy_key;
     EXPECT_FALSE(routing_table.AddNode(node));
  }
  EXPECT_EQ(0, routing_table.size());
  // everything should be set to go now
  for (uint16_t i = 0; i < Parameters::closest_nodes_size ; ++i) {
    node = MakeNode();
    EXPECT_TRUE(routing_table.AddNode(node));
  }
  EXPECT_EQ(Parameters::closest_nodes_size, routing_table.size());
}

TEST(RoutingTableTest, BEH_AddTooManyNodes) {
  RoutingTable routing_table(false, NodeId(NodeId::kRandomId), asymm::GenerateKeyPair());
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
  RoutingTable routing_table(false, NodeId(NodeId::kRandomId), asymm::GenerateKeyPair());
  std::vector<NodeInfo> nodes;
  for (uint16_t i = 0; i < Parameters::closest_nodes_size; ++i)
    nodes.push_back(MakeNode());

  int count(0);
  NetworkStatusFunctor network_status_functor = [](const int& status) {
      LOG(kVerbose) << "Status : " << status;
    };
  std::function<void(const NodeInfo&, bool)> remove_node_functor = [] (const NodeInfo&, bool) {
      LOG(kVerbose) << "RemoveNodeFunctor!";
  };
  ConnectedGroupChangeFunctor group_change_functor = [&count](const std::vector<NodeInfo> nodes) {
      ++count;
      LOG(kInfo) << "Group changed. count : " << count;
      EXPECT_GE(Parameters::closest_nodes_size, count);
      for (auto i: nodes) {
        LOG(kVerbose) << "NodeId : " << DebugId(i.node_id);
      }
    };
  routing_table.InitialiseFunctors(network_status_functor,
                                   remove_node_functor,
                                   []() {},
                                   group_change_functor,
                                   [](const std::vector<NodeInfo>& ) {},
                                   [](const bool&, NodeInfo) {});
  for (uint16_t i = 0; i < Parameters::closest_nodes_size; ++i) {
    ASSERT_TRUE(routing_table.AddNode(nodes.at(i)));
    LOG(kVerbose) << "Added to routing_table : " << DebugId(nodes.at(i).node_id);
  }

  Sleep(boost::posix_time::microseconds(200));
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

TEST(RoutingTableTest, BEH_OrderedGroupChange) {
  RoutingTable routing_table(false, NodeId(NodeId::kRandomId), asymm::GenerateKeyPair());
  std::vector<NodeInfo> nodes;
  for (uint16_t i = 0; i < Parameters::max_routing_table_size; ++i)
    nodes.push_back(MakeNode());

  SortFromTarget(routing_table.kNodeId(), nodes);

  int count(0);
  ConnectedGroupChangeFunctor group_change_functor([&count](const std::vector<NodeInfo> nodes) {
    ++count;
    LOG(kInfo) << "Group changed. count : " << count;
    EXPECT_GE(Parameters::closest_nodes_size, count);
    for (auto i: nodes) {
      LOG(kVerbose) << "NodeId : " << DebugId(i.node_id);
    }
  });

  routing_table.InitialiseFunctors(
      [](const int& status) { LOG(kVerbose) << "Status : " << status; },
      [](const NodeInfo&, bool) {},
      []() {},
      group_change_functor,
      [](const std::vector<NodeInfo>&) {},
      [](const bool&, NodeInfo) {});

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

TEST(RoutingTableTest, BEH_ReverseOrderedGroupChange) {
  RoutingTable routing_table(false, NodeId(NodeId::kRandomId), asymm::GenerateKeyPair());
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
  std::function<void(const NodeInfo&, bool)> remove_node_functor = [] (const NodeInfo&, bool) {
      LOG(kVerbose) << "RemoveNodeFunctor!";
    };
  ConnectedGroupChangeFunctor group_change_functor =
      [&count, &expected_close_nodes](const std::vector<NodeInfo> nodes) {
    ++count;
    LOG(kInfo) << "Group changed. count : " << count;
    EXPECT_GE(2 * Parameters::max_routing_table_size, count);
    for (auto i: nodes) {
      LOG(kVerbose) << "NodeId : " << DebugId(i.node_id);
    }
    for (auto i: expected_close_nodes) {
      LOG(kVerbose) << "Expected Id : " << DebugId(i.node_id);
    }
    EXPECT_EQ(nodes.size(), expected_close_nodes.size());
    size_t max_index(std::min(expected_close_nodes.size(), nodes.size()));
    for (uint16_t i(0); i < max_index; ++i) {
      EXPECT_EQ(nodes.at(i).node_id, expected_close_nodes.at(i).node_id) <<
      "actual node: " << DebugId(nodes.at(i).node_id) <<
      "\n expected node: " << DebugId(expected_close_nodes.at(i).node_id);
    }
  };
  routing_table.InitialiseFunctors(network_status_functor,
                                   remove_node_functor,
                                   []() {},
                                   group_change_functor,
                                   [](const std::vector<NodeInfo>&) {},
                                   [](const bool&, NodeInfo) {});

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
  for (uint16_t i(0); i < Parameters::closest_nodes_size; ++i)
    expected_close_nodes2.push_back(nodes.at(i).node_id);
  std::vector<NodeInfo> close_nodes2(routing_table.group_matrix_.GetConnectedPeers());
  EXPECT_EQ(expected_close_nodes2.size(), close_nodes2.size());
  for (uint16_t i(0); i < std::min(expected_close_nodes2.size(), close_nodes2.size()); ++i)
    EXPECT_EQ(expected_close_nodes2.at(i), close_nodes2.at(i).node_id);

  // Remove nodes from routing table
  std::vector<NodeInfo>::iterator itr_near = nodes.begin();
  std::vector<NodeInfo>::iterator itr_far = nodes.begin() + Parameters::closest_nodes_size;
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
  EXPECT_EQ(2 * Parameters::max_routing_table_size - 1, count);
  EXPECT_EQ(0, routing_table.group_matrix_.GetConnectedPeers().size());
}

TEST(RoutingTableTest, BEH_CheckGroupChangeRemoveNodesFromGroup) {
  RoutingTable routing_table(false, NodeId(NodeId::kRandomId), asymm::GenerateKeyPair());
  std::vector<NodeInfo> nodes;
  for (uint16_t i = 0; i < Parameters::max_routing_table_size; ++i)
    nodes.push_back(MakeNode());

  SortFromTarget(routing_table.kNodeId(), nodes);

  // Set functors
  int count(0);
  NetworkStatusFunctor network_status_functor = [](const int& status) {
      LOG(kVerbose) << "Status : " << status;
    };
  std::function<void(const NodeInfo&, bool)> remove_node_functor = [] (const NodeInfo&, bool) {
      LOG(kVerbose) << "RemoveNodeFunctor!";
  };
  bool setting_up(true);
  std::vector<NodeInfo> expected_close_nodes;
  ConnectedGroupChangeFunctor group_change_functor =
      [&count, &setting_up, &expected_close_nodes](const std::vector<NodeInfo> nodes) {
    ++count;
    LOG(kInfo) << "Group changed. count : " << count;
    if (setting_up) {
      EXPECT_GE(Parameters::closest_nodes_size, count);
      for (auto i: nodes) {
        LOG(kVerbose) << "NodeId : " << DebugId(i.node_id);
      }
    } else {
      EXPECT_GE(Parameters::max_routing_table_size / 4, count);
      for (auto i: nodes) {
        LOG(kVerbose) << "NodeId : " << DebugId(i.node_id);
      }
      for (auto i: expected_close_nodes) {
        LOG(kVerbose) << "Expected Id : " << DebugId(i.node_id);
      }
      EXPECT_EQ(nodes.size(), expected_close_nodes.size());
      uint16_t max_index(
          static_cast<uint16_t>(std::min(expected_close_nodes.size(), nodes.size())));
      for (uint16_t i(0); i < max_index; ++i) {
        EXPECT_EQ(nodes.at(i).node_id, expected_close_nodes.at(i).node_id) <<
            "actual node: " << DebugId(nodes.at(i).node_id) <<
            "\n expected node: " << DebugId(expected_close_nodes.at(i).node_id);
      }
    }
  };
  routing_table.InitialiseFunctors(network_status_functor,
                                   remove_node_functor,
                                   []() {},
                                   group_change_functor,
                                   [](const std::vector<NodeInfo>&) {},
                                   [](const bool&, NodeInfo) {});

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

TEST(RoutingTableTest, BEH_CheckGroupChangeAddGroupNodesToFullTable) {
  RoutingTable routing_table(false, NodeId(NodeId::kRandomId), asymm::GenerateKeyPair());
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
  std::function<void(const NodeInfo&, bool)> remove_node_functor = [] (const NodeInfo&, bool) {
      LOG(kVerbose) << "RemoveNodeFunctor!";
  };
  bool setting_up(true);
  std::vector<NodeInfo> expected_close_nodes;
  ConnectedGroupChangeFunctor group_change_functor =
      [&count, &nodes, &expected_close_nodes, &setting_up](const std::vector<NodeInfo> nodes) {
    ++count;
    LOG(kInfo) << "Group changed. count : " << count;
    if (setting_up) {
      EXPECT_GE(Parameters::closest_nodes_size, count);
      for (auto i: nodes) {
        LOG(kVerbose) << "NodeId : " << DebugId(i.node_id);
      }
    } else {
      EXPECT_GE(3 * Parameters::closest_nodes_size / 2, count);
      for (auto i: nodes) {
        LOG(kVerbose) << "NodeId : " << DebugId(i.node_id);
      }
      for (auto i: expected_close_nodes) {
        LOG(kVerbose) << "Expected Id : " << DebugId(i.node_id);
      }
      EXPECT_EQ(nodes.size(), expected_close_nodes.size());
      uint16_t max_index(
          static_cast<uint16_t>(std::min(expected_close_nodes.size(), nodes.size())));
      for (uint16_t i(0); i < max_index; ++i) {
        EXPECT_EQ(nodes.at(i).node_id, expected_close_nodes.at(i).node_id) <<
            "actual node: " << DebugId(nodes.at(i).node_id) <<
            "\n expected node: " << DebugId(expected_close_nodes.at(i).node_id);
      }
    }
  };
  routing_table.InitialiseFunctors(network_status_functor,
                                   remove_node_functor,
                                   []() {},
                                   group_change_functor,
                                   [](const std::vector<NodeInfo>&) {},
                                   [](const bool&, NodeInfo) {});

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

TEST(RoutingTableTest, BEH_FillEmptyRefillRoutingTable) {
  RoutingTable routing_table(false, NodeId(NodeId::kRandomId), asymm::GenerateKeyPair());
  NodeId own_node_id(routing_table.kNodeId());
  std::vector<NodeInfo> nodes;
  for (uint16_t i = 0; i < Parameters::max_routing_table_size; ++i)
    nodes.push_back(MakeNode());

  // Set functors
  NetworkStatusFunctor network_status_functor = [](const int& status) {
      LOG(kVerbose) << "Status : " << status;
    };
  std::function<void(const NodeInfo&, bool)> remove_node_functor = [] (const NodeInfo&, bool) {
      LOG(kVerbose) << "RemoveNodeFunctor!";
  };
  int expected_count(0);
  std::vector<NodeInfo> expected_group;
  int count(0);
  bool filling_table(true);
  ConnectedGroupChangeFunctor group_change_functor =
      [&](const std::vector<NodeInfo> node_infos) {
      ++count;
      LOG(kInfo) << "Group changed. count : " << count;
    if (filling_table) {
      EXPECT_EQ(expected_count, count);
      EXPECT_EQ(expected_group.size(), node_infos.size());
      for (uint32_t i(0); i < std::min(expected_group.size(), node_infos.size()); ++i)
        EXPECT_EQ(expected_group.at(i).node_id, node_infos.at(i).node_id);
    } else {
      EXPECT_EQ(expected_count, count);
      EXPECT_GE(nodes.size(), node_infos.size());
      for (uint32_t i(0); i < std::min(nodes.size(), node_infos.size()); ++i)
        EXPECT_EQ(nodes.at(i).node_id, node_infos.at(i).node_id);
    }
    for (auto node_info: node_infos) {
      LOG(kVerbose) << "NodeId : " << DebugId(node_info.node_id);
    }
  };
  routing_table.InitialiseFunctors(network_status_functor,
                                   remove_node_functor,
                                   [] () {},
                                   group_change_functor,
                                   [](const std::vector<NodeInfo>&) {},
                                   [](const bool&, NodeInfo) {});
  // Fill routing table
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
    routing_table.DropNode(removal_id, true);
    LOG(kVerbose) << "Removed from routing_table : " << DebugId(removal_id);
  }

  EXPECT_EQ(expected_count - 1, count);
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
  RoutingTable routing_table_1(false, NodeId(NodeId::kRandomId), asymm::GenerateKeyPair());
  NodeInfo node_info_1(MakeNode());
  node_info_1.node_id = routing_table_1.kNodeId();
  node_info_1.connection_id = node_info_1.node_id;
  NodeId node_id_1(routing_table_1.kNodeId());
  ASSERT_EQ(routing_table_1.kNodeId(), node_info_1.node_id);

  // Generate other NodeInfo
  std::vector<NodeInfo> extra_nodes;
  uint16_t limit(2 * Parameters::closest_nodes_size + 1);
  for (uint16_t i = 0; i < limit; ++i)
    extra_nodes.push_back(MakeNode());
  SortFromTarget(node_id_1, extra_nodes);

  // Set up 2 to be in 1's closest_nodes_size group
  uint16_t index_2(RandomUint32() % Parameters::closest_nodes_size);
  RoutingTable routing_table_2(false, extra_nodes.at(index_2).node_id, asymm::GenerateKeyPair());

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
  NetworkStatusFunctor network_status_functor = [](const int& status) {
      LOG(kVerbose) << "Status : " << status;
    };
  std::function<void(const NodeInfo&, bool)> remove_node_functor = [] (const NodeInfo&, bool) {
      LOG(kVerbose) << "RemoveNodeFunctor!";
  };
  ConnectedGroupChangeFunctor group_change_functor =
      [&node_id_1, &routing_table_2, &expecting_group_change, &count, &expected_count]
  (const std::vector<NodeInfo> nodes) {
    ++count;
    EXPECT_EQ(count, expected_count);
    EXPECT_TRUE(expecting_group_change);
    bool found_2(false);
    std::vector<NodeInfo> close_nodes;
    for (auto node_info : nodes) {
      if (node_info.node_id == routing_table_2.kNodeId())
        found_2 = true;
      else
        close_nodes.push_back(node_info);
    }
    EXPECT_GE(Parameters::closest_nodes_size - 1, close_nodes.size());
    EXPECT_TRUE(found_2);
    if (!found_2) {
      LOG(kError) << "Haven't found NodeId for routing_table_2 in group change!";
    } else {
      routing_table_2.GroupUpdateFromConnectedPeer(node_id_1, close_nodes);
    }
    LOG(kVerbose) << "NodeIds for routing_table_1 (group change):";
    for (auto j: nodes) {
      LOG(kVerbose) << "NodeId : " << DebugId(j.node_id);
    }
  };
  routing_table_1.InitialiseFunctors(network_status_functor,
                                     remove_node_functor,
                                     []() {},
                                     group_change_functor,
                                     [](const std::vector<NodeInfo>&) {},
                                     [](const bool&, NodeInfo) {});

  // Check that 2's group matrix is updated correctly - Add nodes
  std::vector<NodeInfo> close_nodes;
  SortFromTarget(NodeId(NodeId::kRandomId), extra_nodes);
  std::vector<NodeId> expected_close_nodes;
  for (auto node_info : extra_nodes) {
    if (expected_close_nodes.size() < size_t(Parameters::closest_nodes_size - 1)) {
      expected_close_nodes.push_back(node_info.node_id);
      SortIdsFromTarget(node_id_1, expected_close_nodes);
      ++expected_count;
      expecting_group_change = true;
    } else if ((node_info.node_id ^ node_id_1) <
               (expected_close_nodes.at(Parameters::closest_nodes_size - 2) ^ node_id_1)) {
      expected_close_nodes.pop_back();
      expected_close_nodes.push_back(node_info.node_id);
      SortIdsFromTarget(node_id_1, expected_close_nodes);
      ++expected_count;
      expecting_group_change = true;
    } else {
      expecting_group_change = false;
    }
    ASSERT_TRUE(routing_table_1.AddNode(node_info));
    EXPECT_TRUE(routing_table_2.group_matrix_.GetRow(node_id_1, close_nodes));
    EXPECT_EQ(expected_close_nodes.size(), close_nodes.size());
    for (uint16_t i(0); i <  std::min(expected_close_nodes.size(), close_nodes.size()); ++i) {
      EXPECT_EQ(expected_close_nodes.at(i), close_nodes.at(i).node_id);
    }
  }

  EXPECT_EQ(2 * Parameters::closest_nodes_size + 1, routing_table_1.size());
  EXPECT_EQ(Parameters::closest_nodes_size + 1,
            routing_table_2.group_matrix_.GetUniqueNodes().size());
  EXPECT_EQ(expected_count, count);
  EXPECT_LE(Parameters::closest_nodes_size - 1, count);

  // Check that 2's group matrix is updated correctly - Drop nodes
  SortFromTarget(node_id_1, extra_nodes);
  count = 0;
  expected_count = 0;
  expected_close_nodes.clear();
  for (uint16_t i(0); i < Parameters::closest_nodes_size - 1; ++i)
    expected_close_nodes.push_back(extra_nodes.at(i).node_id);
  NodeInfo removal_node;
  uint16_t removal_index;
  while (extra_nodes.size() > 0) {
    removal_index = static_cast<uint16_t>(RandomUint32() % extra_nodes.size());
    removal_node = extra_nodes.at(removal_index);
    extra_nodes.erase(extra_nodes.begin() + removal_index);
    if (removal_index < (Parameters::closest_nodes_size - 1)) {
      expected_close_nodes.clear();
      int extra_nodes_size = static_cast<int>(extra_nodes.size());
      for (int i(0);
           i < std::min(extra_nodes_size, Parameters::closest_nodes_size - 1);
           ++i) {
        expected_close_nodes.push_back(extra_nodes.at(i).node_id);
      }
      ++expected_count;
      expecting_group_change = true;
    } else {
      expecting_group_change = false;
    }
    routing_table_1.DropNode(removal_node.node_id, true);
    EXPECT_TRUE(routing_table_2.group_matrix_.GetRow(node_id_1, close_nodes));
    EXPECT_EQ(expected_close_nodes.size(), close_nodes.size());
    for (uint16_t i(0); i <  std::min(expected_close_nodes.size(), close_nodes.size()); ++i) {
      EXPECT_EQ(expected_close_nodes.at(i), close_nodes.at(i).node_id);
    }
  }

  EXPECT_EQ(1, routing_table_1.size());
  EXPECT_EQ(2, routing_table_2.group_matrix_.GetUniqueNodes().size());
  EXPECT_EQ(expected_count, count);
  EXPECT_LE(Parameters::closest_nodes_size - 1, count);
}

TEST(RoutingTableTest, BEH_GroupUpdateFromConnectedPeer) {
  RoutingTable routing_table(false, NodeId(NodeId::kRandomId), asymm::GenerateKeyPair());

  std::vector<NodeInfo> nodes;
  for (uint16_t i(0); i < Parameters::closest_nodes_size; ++i)
    nodes.push_back(MakeNode());

  std::vector<NodeInfo> row_contents;
  for (auto node_info : nodes) {
    EXPECT_TRUE(routing_table.AddNode(node_info));
    NodeInfo node;
    node.node_id = NodeId(NodeId::kRandomId);
    row_contents.push_back(node);
    EXPECT_TRUE(routing_table.group_matrix_.GetRow(node_info.node_id, row_contents));
    EXPECT_EQ(0, row_contents.size());
  }

  std::vector<NodeInfo> new_row_entries;
  for (auto node_info : nodes) {
    for (uint16_t i(0); i < RandomUint32() % (Parameters::closest_nodes_size - 1); ++i) {
      NodeInfo node;
      node.node_id = NodeId(NodeId::kRandomId);
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
  RoutingTable routing_table(false, NodeId(NodeId::kRandomId), asymm::GenerateKeyPair());
  NodeId my_node(routing_table.kNodeId());

  for (uint16_t i(static_cast<uint16_t>(routing_table.size()));
       routing_table.size() < 10; ++i) {
    NodeInfo node(MakeNode());
    nodes_id.push_back(node.node_id);
    EXPECT_TRUE(routing_table.AddNode(node));
  }
  std::sort(nodes_id.begin(), nodes_id.end(),
            [&](const NodeId& lhs, const NodeId& rhs) {
              return NodeId::CloserToTarget(lhs, rhs, my_node);
            });
  for (uint16_t index = 0; index < 10; ++index) {
    EXPECT_EQ(nodes_id[index], routing_table.GetNthClosestNode(my_node, index + 1).node_id)
        << DebugId(nodes_id[index]) << " not eq to "
        << DebugId(routing_table.GetNthClosestNode(my_node, index + 1).node_id);
  }
}

TEST(RoutingTableTest, BEH_IsIdInGroupRange) {
  RoutingTable routing_table(false, NodeId(NodeId::kRandomId), asymm::GenerateKeyPair());
  std::vector<NodeId> nodes_id;
  NodeInfo node_info;
  NodeId my_node(routing_table.kNodeId());
  while (static_cast<uint16_t>(routing_table.size()) <
             Parameters::routing_table_ready_to_response - 1) {
    NodeInfo node(MakeNode());
    nodes_id.push_back(node.node_id);
    routing_table.group_matrix_.unique_nodes_.push_back(node);
    EXPECT_TRUE(routing_table.AddNode(node));
  }
  EXPECT_FALSE(routing_table.IsIdInGroup(NodeId(NodeId::kRandomId),
                                         NodeId(NodeId::kRandomId)));
  while (static_cast<uint16_t>(routing_table.size()) <
             Parameters::max_routing_table_size) {
    NodeInfo node(MakeNode());
    nodes_id.push_back(node.node_id);
    routing_table.group_matrix_.unique_nodes_.push_back(node);
    EXPECT_TRUE(routing_table.AddNode(node));
  }

  NodeId info_id(NodeId::kRandomId);
  std::nth_element(nodes_id.begin(),
                   nodes_id.begin() + Parameters::node_group_size,
                   nodes_id.end(),
                   [&](const NodeId& lhs, const NodeId& rhs) {
                     return NodeId::CloserToTarget(lhs, rhs, info_id);
                   });
  uint16_t index(0);
  while (index < Parameters::node_group_size) {
    if ((nodes_id.at(index) ^ info_id) <= (my_node ^ nodes_id[Parameters::node_group_size - 1]))
      EXPECT_TRUE(routing_table.IsIdInGroup(nodes_id.at(index++), info_id));
    else
      EXPECT_FALSE(routing_table.IsIdInGroup(nodes_id.at(index++), info_id));
  }
}

TEST(RoutingTableTest, BEH_GetClosestNodeWithExclusion) {
  std::vector<NodeId> nodes_id;
  std::vector<std::string> exclude;
  RoutingTable routing_table(false, NodeId(NodeId::kRandomId), asymm::GenerateKeyPair());
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

  // routing_table with Parameters::node_group_size elements
  exclude.clear();
  for (uint16_t i(static_cast<uint16_t>(routing_table.size()));
       routing_table.size() < Parameters::node_group_size; ++i) {
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

  exclude.push_back(nodes_id[random_index].string());
  node_info = routing_table.GetClosestNode(nodes_id[random_index], exclude, false);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], exclude, true);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);

  for (auto node_id : nodes_id)
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

  for (auto node_id : nodes_id)
    exclude.push_back(node_id.string());
  node_info = routing_table.GetClosestNode(nodes_id[random_index], exclude, false);
  node_info2 = routing_table.GetClosestNode(nodes_id[random_index], exclude, true);
  EXPECT_EQ(node_info.node_id, node_info2.node_id);
  EXPECT_EQ(node_info.node_id, NodeInfo().node_id);
}

}  // namespace test
}  // namespace routing
}  // namespace maidsafe
