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

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/types.h"

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/client_routing_table.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {
namespace routing {
namespace test {

class BasicClientRoutingTableTest : public testing::Test {
 public:
  BasicClientRoutingTableTest() : node_id_(RandomString(NodeId::kSize)) {}

 protected:
  NodeId node_id_;
};

class ClientRoutingTableTest : public BasicClientRoutingTableTest {
 public:
  ClientRoutingTableTest() : nodes_(), furthest_close_node_() {}

  void PopulateNodes(unsigned int size) {
    for (unsigned int i(0); i < size; ++i)
      nodes_.push_back(MakeNode());
  }

  void PopulateNodesSetFurthestCloseNode(unsigned int size, const NodeId& target) {
    for (unsigned int i(0); i <= size; ++i)
      nodes_.push_back(MakeNode());
    SortFromTarget(target, nodes_);
    furthest_close_node_ = nodes_.at(size);
    nodes_.pop_back();
  }

  NodeId BiasNodeIds(std::vector<NodeInfo>& biased_nodes) {
    NodeId sought_id(nodes_.at(RandomUint32() % Parameters::max_client_routing_table_size).id);
    for (unsigned int i(0); i < Parameters::max_client_routing_table_size; ++i) {
      if (nodes_.at(i).id == sought_id) {
        biased_nodes.push_back(nodes_.at(i));
      } else if ((RandomUint32() % 3) == 0) {
        nodes_.at(i).id = sought_id;
        biased_nodes.push_back(nodes_.at(i));
      }
    }
    return sought_id;
  }

  void ScrambleNodesOrder() { SortFromTarget(NodeId(RandomString(NodeId::kSize)), nodes_); }

  void PopulateClientRoutingTable(ClientRoutingTable& client_routing_table) {
    for (auto& node : nodes_)
      EXPECT_TRUE(client_routing_table.AddNode(node, furthest_close_node_.id));
  }

 protected:
  std::vector<NodeInfo> nodes_;
  NodeInfo furthest_close_node_;
};

TEST_F(BasicClientRoutingTableTest, BEH_CheckAddOwnNodeInfo) {
  ClientRoutingTable client_routing_table(node_id_);

  NodeInfo node(MakeNode());
  node.id = client_routing_table.kNodeId();

  EXPECT_FALSE(client_routing_table.CheckNode(node, NodeId(RandomString(NodeId::kSize))));
  EXPECT_FALSE(client_routing_table.AddNode(node, NodeId(RandomString(NodeId::kSize))));
}

TEST_F(ClientRoutingTableTest, BEH_CheckAddFarAwayNode) {
  ClientRoutingTable client_routing_table(node_id_);

  PopulateNodes(2);

  SortFromTarget(client_routing_table.kNodeId(), nodes_);

  EXPECT_FALSE(client_routing_table.CheckNode(nodes_.at(1), nodes_.at(0).id));
  EXPECT_FALSE(client_routing_table.AddNode(nodes_.at(1), nodes_.at(0).id));
}

TEST_F(ClientRoutingTableTest, FUNC_CheckAddSurplusNodes) {
  ClientRoutingTable client_routing_table(node_id_);

  PopulateNodesSetFurthestCloseNode(2 * Parameters::max_client_routing_table_size,
                                    client_routing_table.kNodeId());
  ScrambleNodesOrder();

  for (unsigned int i(0); i < Parameters::max_client_routing_table_size; ++i) {
    EXPECT_TRUE(client_routing_table.CheckNode(nodes_.at(i), furthest_close_node_.id));
    EXPECT_TRUE(client_routing_table.AddNode(nodes_.at(i), furthest_close_node_.id));
  }

  for (unsigned int i(Parameters::max_client_routing_table_size);
       i < 2 * Parameters::max_client_routing_table_size; ++i) {
    EXPECT_FALSE(client_routing_table.CheckNode(nodes_.at(i), furthest_close_node_.id));
    EXPECT_FALSE(client_routing_table.AddNode(nodes_.at(i), furthest_close_node_.id));
  }

  EXPECT_EQ(Parameters::max_client_routing_table_size, client_routing_table.size());
}

TEST_F(ClientRoutingTableTest, BEH_CheckAddSameNodeIdTwice) {
  ClientRoutingTable client_routing_table(node_id_);

  PopulateNodes(2);
  SortFromTarget(client_routing_table.kNodeId(), nodes_);

  EXPECT_TRUE(client_routing_table.CheckNode(nodes_.at(0), nodes_.at(1).id));
  EXPECT_TRUE(client_routing_table.AddNode(nodes_.at(0), nodes_.at(1).id));

  NodeInfo node(MakeNode());
  node.id = nodes_.at(0).id;

  EXPECT_EQ(nodes_.at(0).id, node.id);
  EXPECT_TRUE(client_routing_table.CheckNode(node, nodes_.at(1).id));
  EXPECT_TRUE(client_routing_table.AddNode(node, nodes_.at(1).id));
}

TEST_F(ClientRoutingTableTest, BEH_CheckAddSameConnectionIdTwice) {
  ClientRoutingTable client_routing_table(node_id_);

  PopulateNodes(3);
  SortFromTarget(client_routing_table.kNodeId(), nodes_);

  EXPECT_TRUE(client_routing_table.CheckNode(nodes_.at(0), nodes_.at(2).id));
  EXPECT_TRUE(client_routing_table.AddNode(nodes_.at(0), nodes_.at(2).id));

  nodes_.at(1).connection_id = nodes_.at(0).connection_id;

  EXPECT_EQ(nodes_.at(0).connection_id, nodes_.at(1).connection_id);
  EXPECT_TRUE(client_routing_table.CheckNode(nodes_.at(1), nodes_.at(2).id));
  EXPECT_FALSE(client_routing_table.AddNode(nodes_.at(1), nodes_.at(2).id));
}

// TODO(Alison) - uncomment this test if it becomes relevant again
/*TEST_F(ClientRoutingTableTest, BEH_CheckAddSameKeysTwice) {
  ClientRoutingTable client_routing_table(node_id_);

  PopulateNodes(3);
  SortFromTarget(client_routing_table.kNodeId(), nodes_);

  EXPECT_TRUE(client_routing_table.CheckNode(nodes_.at(0), nodes_.at(2).id));
  EXPECT_TRUE(client_routing_table.AddNode(nodes_.at(0), nodes_.at(2).id));

  nodes_.at(1).public_key = nodes_.at(0).public_key;

  EXPECT_TRUE(asymm::MatchingKeys(nodes_.at(0).public_key, nodes_.at(1).public_key));
  EXPECT_TRUE(client_routing_table.CheckNode(nodes_.at(1), nodes_.at(2).id));
  EXPECT_FALSE(client_routing_table.AddNode(nodes_.at(1), nodes_.at(2).id));
}*/

TEST_F(ClientRoutingTableTest, BEH_CheckAddSameConnectionAndKeysTwice) {
  ClientRoutingTable client_routing_table(node_id_);

  PopulateNodes(3);
  SortFromTarget(client_routing_table.kNodeId(), nodes_);

  EXPECT_TRUE(client_routing_table.CheckNode(nodes_.at(0), nodes_.at(2).id));
  EXPECT_TRUE(client_routing_table.AddNode(nodes_.at(0), nodes_.at(2).id));

  nodes_.at(1).connection_id = nodes_.at(0).connection_id;
  nodes_.at(1).public_key = nodes_.at(0).public_key;

  EXPECT_EQ(nodes_.at(0).connection_id, nodes_.at(1).connection_id);
  EXPECT_TRUE(asymm::MatchingKeys(nodes_.at(0).public_key, nodes_.at(1).public_key));
  EXPECT_TRUE(client_routing_table.CheckNode(nodes_.at(1), nodes_.at(2).id));
  EXPECT_FALSE(client_routing_table.AddNode(nodes_.at(1), nodes_.at(2).id));
}

TEST_F(ClientRoutingTableTest, BEH_AddThenCheckNode) {
  ClientRoutingTable client_routing_table(node_id_);

  PopulateNodes(2);

  SortFromTarget(client_routing_table.kNodeId(), nodes_);

  EXPECT_TRUE(client_routing_table.CheckNode(nodes_.at(0), nodes_.at(1).id));
  EXPECT_TRUE(client_routing_table.AddNode(nodes_.at(0), nodes_.at(1).id));
  EXPECT_TRUE(client_routing_table.CheckNode(nodes_.at(0), nodes_.at(1).id));
  EXPECT_FALSE(client_routing_table.AddNode(nodes_.at(0), nodes_.at(1).id));
}

TEST_F(ClientRoutingTableTest, FUNC_DropNodes) {
  ClientRoutingTable client_routing_table(node_id_);

  PopulateNodesSetFurthestCloseNode(Parameters::max_client_routing_table_size,
                                    client_routing_table.kNodeId());
  ScrambleNodesOrder();

  std::vector<NodeInfo> expected_nodes;
  NodeId sought_id(BiasNodeIds(expected_nodes));

  PopulateClientRoutingTable(client_routing_table);

  std::vector<NodeInfo> dropped_nodes(client_routing_table.DropNodes(sought_id));
  EXPECT_EQ(nodes_.size() - dropped_nodes.size(), client_routing_table.size());

  EXPECT_EQ(expected_nodes.size(), dropped_nodes.size());
  bool found_counterpart(false);
  for (const auto& expected_node : expected_nodes) {
    found_counterpart = false;
    for (const auto& dropped_node : dropped_nodes) {
      if ((expected_node.connection_id == dropped_node.connection_id) &&
          asymm::MatchingKeys(expected_node.public_key, dropped_node.public_key)) {
        found_counterpart = true;
        break;
      }
    }
    EXPECT_TRUE(found_counterpart);
  }
}

TEST_F(ClientRoutingTableTest, FUNC_DropConnection) {
  ClientRoutingTable client_routing_table(node_id_);

  PopulateNodesSetFurthestCloseNode(Parameters::max_client_routing_table_size,
                                    client_routing_table.kNodeId());
  ScrambleNodesOrder();
  PopulateClientRoutingTable(client_routing_table);
  ScrambleNodesOrder();

  while (!nodes_.empty()) {
    NodeInfo dropped_node(
        client_routing_table.DropConnection(nodes_.at(nodes_.size() - 1).connection_id));
    EXPECT_EQ(nodes_.at(nodes_.size() - 1).id, dropped_node.id);
    EXPECT_EQ(nodes_.at(nodes_.size() - 1).connection_id, dropped_node.connection_id);

    nodes_.pop_back();
    EXPECT_EQ(nodes_.size(), client_routing_table.size());
  }

  EXPECT_EQ(0, client_routing_table.size());
}

TEST_F(ClientRoutingTableTest, FUNC_GetNodesInfo) {
  ClientRoutingTable client_routing_table(node_id_);

  PopulateNodesSetFurthestCloseNode(Parameters::max_client_routing_table_size,
                                    client_routing_table.kNodeId());
  ScrambleNodesOrder();

  std::vector<NodeInfo> expected_nodes;
  NodeId sought_id(BiasNodeIds(expected_nodes));

  PopulateClientRoutingTable(client_routing_table);

  std::vector<NodeInfo> got_nodes(client_routing_table.GetNodesInfo(sought_id));

  EXPECT_EQ(expected_nodes.size(), got_nodes.size());
  bool found_counterpart(false);
  for (const auto& expected_node : expected_nodes) {
    found_counterpart = false;
    for (const auto& got_node : got_nodes) {
      if ((expected_node.connection_id == got_node.connection_id) &&
          asymm::MatchingKeys(expected_node.public_key, got_node.public_key)) {
        found_counterpart = true;
        break;
      }
    }
    EXPECT_TRUE(found_counterpart);
  }
}

TEST_F(ClientRoutingTableTest, FUNC_IsConnected) {
  ClientRoutingTable client_routing_table(node_id_);

  PopulateNodesSetFurthestCloseNode(2 * Parameters::max_client_routing_table_size,
                                    client_routing_table.kNodeId());
  ScrambleNodesOrder();

  for (unsigned int i(0); i < Parameters::max_client_routing_table_size; ++i)
    EXPECT_TRUE(client_routing_table.AddNode(nodes_.at(i), furthest_close_node_.id));

  for (unsigned int i(0); i < Parameters::max_client_routing_table_size; ++i)
    EXPECT_TRUE(client_routing_table.Contains(nodes_.at(i).id));

  for (unsigned int i(Parameters::max_client_routing_table_size);
       i < 2 * Parameters::max_client_routing_table_size; ++i)
    EXPECT_FALSE(client_routing_table.Contains(nodes_.at(i).id));
}

TEST_F(BasicClientRoutingTableTest, BEH_IsThisNodeInRange) {
  ClientRoutingTable client_routing_table(node_id_);

  std::vector<NodeInfo> nodes;
  NodeInfo node_info;
  for (int i(0); i < 101; ++i) {
    node_info.id = NodeId(RandomString(NodeId::kSize));
    nodes.push_back(node_info);
  }
  SortNodeInfosFromTarget(client_routing_table.kNodeId(), nodes);
  NodeInfo furthest_close_node_id(nodes.at(50));
  for (int i(0); i < 50; ++i)
    EXPECT_TRUE(client_routing_table.IsThisNodeInRange(nodes.at(i).id, furthest_close_node_id.id));
  for (int i(51); i < 101; ++i)
    EXPECT_FALSE(client_routing_table.IsThisNodeInRange(nodes.at(i).id, furthest_close_node_id.id));
}

}  // namespace test
}  // namespace routing
}  // namespace maidsafe
