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

#include <vector>

#include "boost/progress.hpp"

#include "maidsafe/rudp/nat_type.h"

#include "maidsafe/routing/tests/routing_network.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

class RoutingStandAloneTest : public GenericNetwork, public testing::Test {
 public:
  RoutingStandAloneTest(void) : GenericNetwork() {}

  virtual void SetUp() override { GenericNetwork::SetUp(); }

  virtual void TearDown() override {
    Sleep(std::chrono::microseconds(100));
    GenericNetwork::TearDown();
  }

  bool PmidIsCloseToMaid(const NodeId& pmid_id, const NodeId& maid_id) {
    std::vector<NodeId> pmid_ids;
    for (size_t index(0); index < ClientIndex(); ++index) {
      pmid_ids.push_back(nodes_[index]->node_id());
    }
    std::partial_sort(std::begin(pmid_ids), std::begin(pmid_ids) + Parameters::closest_nodes_size,
                      std::end(pmid_ids), [&](const NodeId& lhs, const NodeId& rhs) {
                                            return NodeId::CloserToTarget(lhs, rhs, maid_id);
                                          });
    return !NodeId::CloserToTarget(pmid_ids.at(Parameters::closest_nodes_size - 1),
                                   pmid_id, maid_id);
  }
};

// TODO(Mahmoud): This test should be moved to TESTrouting_func as it doesn't affect network.
TEST_F(RoutingStandAloneTest, FUNC_GetGroup) {
  this->SetUpNetwork(kServerSize);
  int counter(100);
  while (counter-- > 0) {
    uint16_t random_node(static_cast<uint16_t>(RandomInt32() % kServerSize));
    NodeId node_id(NodeId::kRandomId);
    std::future<std::vector<NodeId>> future(this->nodes_[random_node]->GetGroup(node_id));
    auto nodes_id(future.get());
    auto group_ids(this->GroupIds(node_id));
    EXPECT_EQ(nodes_id.size(), group_ids.size());
    for (const auto& id : group_ids)
      EXPECT_NE(std::find(nodes_id.begin(), nodes_id.end(), id), nodes_id.end());
  }
}

TEST_F(RoutingStandAloneTest, FUNC_VaultSendToClient) {
  this->SetUpNetwork(kServerSize, 1);
  for (size_t index(0); index < this->ClientIndex(); ++index) {
    EXPECT_TRUE(this->SendDirect(this->nodes_[index], this->nodes_[this->ClientIndex()]->node_id(),
                ExpectedNodeType::kExpectClient)) << DebugId(this->nodes_[index]->node_id());
  }
}

TEST_F(RoutingStandAloneTest, FUNC_ClientRoutingTableUpdate) {
  this->SetUpNetwork(kServerSize);
  this->AddNode(MakeMaid());
  NodeId maid_id(nodes_[kServerSize]->GetMaid().name()), pmid_id;
  while (this->nodes_.size() < kServerSize + Parameters::max_routing_table_size_for_client) {
    auto pmid(MakePmid());
    pmid_id = NodeId(pmid.name());
    this->AddNode(pmid);
    Sleep(std::chrono::milliseconds(500));
    if (PmidIsCloseToMaid(pmid_id, maid_id)) {
      EXPECT_TRUE(this->nodes_[ClientIndex()]->RoutingTableHasNode(pmid_id))
          << DebugId(this->nodes_[ClientIndex()]->node_id()) << " does not have "
          << DebugId(pmid_id);
    }
  }
}

TEST_F(RoutingStandAloneTest, FUNC_SetupNetwork) { this->SetUpNetwork(kServerSize); }

TEST_F(RoutingStandAloneTest, FUNC_SetupSingleClientHybridNetwork) {
  this->SetUpNetwork(kServerSize, 1);
}

TEST_F(RoutingStandAloneTest, FUNC_SetupHybridNetwork) {
  this->SetUpNetwork(kServerSize, kClientSize);
}

TEST_F(RoutingStandAloneTest, FUNC_SetupNetworkWithVaultsBehindSymmetricNat) {
  this->SetUpNetwork(kServerSize, kClientSize, kServerSize / 4, 0);
}

TEST_F(RoutingStandAloneTest, FUNC_SetupNetworkWithNodesBehindSymmetricNat) {
  this->SetUpNetwork(kServerSize, kClientSize, kServerSize / 4, kClientSize);
}

TEST_F(RoutingStandAloneTest, FUNC_SetupNetworkAddVaultsBehindSymmetricNat) {
  this->SetUpNetwork(kServerSize);
  uint16_t num_symmetric_vaults(kServerSize / 3);
  for (auto i(0); i < num_symmetric_vaults; ++i)
    this->AddNode(false, true);
}

TEST_F(RoutingStandAloneTest, FUNC_SetupNetworkAddVaultsBehindSymmetricNatAndClients) {
  this->SetUpNetwork(kServerSize, kClientSize);
  uint16_t num_symmetric_vaults(kServerSize / 3);
  for (auto i(0); i < num_symmetric_vaults; ++i)
    this->AddNode(false, true);
  uint16_t num_symmetric_clients(kClientSize);
  for (auto i(0); i < num_symmetric_clients; ++i)
    this->AddNode(true, false);  // Add more normal clients
}

TEST_F(RoutingStandAloneTest, FUNC_SetupNetworkAddNodesBehindSymmetricNat) {
  this->SetUpNetwork(kServerSize, kClientSize);
  uint16_t num_symmetric_vaults(kServerSize / 3);
  for (auto i(0); i < num_symmetric_vaults; ++i)
    this->AddNode(false, true);
  uint16_t num_symmetric_clients(kClientSize);
  for (auto i(0); i < num_symmetric_clients; ++i)
    this->AddNode(true, true);  // Add clients behind symmetric NAT
}

TEST_F(RoutingStandAloneTest, DISABLED_FUNC_ExtendedSendMulti) {
  // N.B. This test takes approx. 1hr to run, hence it is disabled.
  this->SetUpNetwork(kServerSize);
  uint16_t loop(100);
  while (loop-- > 0) {
    EXPECT_TRUE(SendDirect(40));
    this->ClearMessages();
  }
}

TEST_F(RoutingStandAloneTest, FUNC_ExtendedSendToGroup) {
  uint16_t message_count(10), receivers_message_count(0);
  this->SetUpNetwork(kServerSize);
  size_t last_index(this->nodes_.size() - 1);
  NodeId dest_id(this->nodes_[last_index]->node_id());

  uint16_t loop(100);
  while (loop-- > 0) {
    EXPECT_TRUE(SendGroup(dest_id, message_count));
    for (size_t index = 0; index != (last_index); ++index)
      receivers_message_count += static_cast<uint16_t>(this->nodes_.at(index)->MessagesSize());

    EXPECT_EQ(0, this->nodes_[last_index]->MessagesSize())
        << "Not expected message at Node : "
        << HexSubstr(this->nodes_[last_index]->node_id().string());
    EXPECT_EQ(message_count * (Parameters::group_size), receivers_message_count);
    receivers_message_count = 0;
    this->ClearMessages();
  }
}

TEST_F(RoutingStandAloneTest, FUNC_ExtendedSendToGroupRandomId) {
  uint16_t message_count(50), receivers_message_count(0);
  this->SetUpNetwork(kServerSize);
  uint16_t loop(10);
  while (loop-- > 0) {
    for (int index = 0; index < message_count; ++index) {
      NodeId random_id(NodeId::kRandomId);
      std::vector<NodeId> groupd_ids(this->GroupIds(random_id));
      EXPECT_TRUE(SendGroup(random_id, 1));
      for (const auto& node : this->nodes_) {
        if (std::find(groupd_ids.begin(), groupd_ids.end(), node->node_id()) != groupd_ids.end()) {
          receivers_message_count += static_cast<uint16_t>(node->MessagesSize());
          node->ClearMessages();
        }
      }
    }
    EXPECT_EQ(message_count * (Parameters::group_size), receivers_message_count);
    LOG(kVerbose) << "Total message received count : " << message_count *
                                                              (Parameters::group_size);
    receivers_message_count = 0;
    this->ClearMessages();
  }
}

TEST_F(RoutingStandAloneTest, FUNC_NodeRemoved) {
  this->SetUpNetwork(kServerSize);
  size_t random_index(this->RandomNodeIndex());
  NodeInfo removed_node_info(this->nodes_[random_index]->GetRemovableNode());
  EXPECT_GE(removed_node_info.bucket, 510);
}

TEST_F(RoutingStandAloneTest, FUNC_JoinAfterBootstrapLeaves) {
  this->SetUpNetwork(kServerSize);
  Sleep(std::chrono::seconds(10));
  this->AddNode(MakePmid());
}

TEST_F(RoutingStandAloneTest, FUNC_ReBootstrap) {
  // test currently fine for small network size (approx half max routing table size). Will need
  // updated to deal with larger network.
  int network_size(kServerSize);
  this->SetUpNetwork(network_size);

  NodeId removed_id(this->nodes_[network_size - 1]->node_id());

  for (const auto& element : this->nodes_) {
    if (element->node_id() != removed_id)
      EXPECT_TRUE(element->RoutingTableHasNode(removed_id));
  }

  NodePtr removed_node(this->nodes_[network_size - 1]);
  this->RemoveNode(this->nodes_[network_size - 1]->node_id());
  EXPECT_EQ(network_size - 1, this->nodes_.size());

  for (auto& node : this->nodes_) {
    EXPECT_TRUE(node->DropNode(removed_id));
    EXPECT_FALSE(node->RoutingTableHasNode(removed_id));
  }

  // wait for removed node's routing table to reach zero - re-bootstrap will be triggered
  Sleep(std::chrono::seconds(1));
  std::vector<NodeInfo> routing_table(removed_node->RoutingTable());
  EXPECT_EQ(0, routing_table.size());

  // wait for re_bootstrap_time_lag to expire & bootstrap process to complete
  Sleep(std::chrono::seconds(20));
  for (const auto& node : this->nodes_)
    EXPECT_TRUE(node->RoutingTableHasNode(removed_id));

  routing_table = removed_node->RoutingTable();
  EXPECT_EQ(network_size - 1, routing_table.size());
}

TEST_F(RoutingStandAloneTest, FUNC_GroupsAndSendWithSymmetricNat) {
  // TODO(Alison) - move this into functional tests when can run on mixed NAT network
  this->SetUpNetwork(kServerSize, 0, kServerSize / 4, 0);  // TODO(Alison) - adjust values?

  ASSERT_TRUE(WaitForHealthToStabilise());
  ASSERT_TRUE(WaitForNodesToJoin());

  EXPECT_TRUE(CheckGroupMatrixUniqueNodes());
  EXPECT_TRUE(CheckGroupMatrixUniqueNodes(3 / 2 * Parameters::closest_nodes_size + 1));

  // Check Send between each pair of vaults
  for (auto& source_node : this->nodes_) {
    for (const auto& dest_node : this->nodes_) {
      EXPECT_TRUE(this->SendDirect(source_node, dest_node->node_id()));
    }
  }

  // Check GroupSend from each vault to each vault ID
  for (uint16_t source_index(0); source_index < this->nodes_.size(); ++source_index) {
    for (const auto& node : this->nodes_) {
      EXPECT_TRUE(this->SendGroup(node->node_id(), 1, source_index));
    }
  }

  // Check GroupSend for random targets
  for (uint16_t source_index(0); source_index < this->nodes_.size(); ++source_index) {
    for (uint16_t count(0); count < 1; ++count) {
      NodeId node_id(NodeId::kRandomId);
      EXPECT_TRUE(this->SendGroup(node_id, 1, source_index));
    }
  }
}

TEST_F(RoutingStandAloneTest, FUNC_GroupsAndSendWithClientsAndSymmetricNat) {
  // TODO(Alison) - move this into functional tests when can run on mixed NAT network
  this->SetUpNetwork(kServerSize, kClientSize, kServerSize / 4,
                     kClientSize / 2);  // TODO(Alison) - adjust values?

  ASSERT_TRUE(WaitForHealthToStabilise());
  ASSERT_TRUE(WaitForNodesToJoin());

  EXPECT_TRUE(CheckGroupMatrixUniqueNodes());
  EXPECT_TRUE(CheckGroupMatrixUniqueNodes(3 / 2 * Parameters::closest_nodes_size + 1));

  // Check Send from each node to each vault
  for (auto& source_node : this->nodes_) {
    for (const auto& dest_node : this->nodes_) {
      if (!dest_node->IsClient())
        EXPECT_TRUE(this->SendDirect(source_node, dest_node->node_id()));
    }
  }

  // Check GroupSend from each node to each node ID
  for (uint16_t source_index(0); source_index < this->nodes_.size(); ++source_index) {
    for (const auto& node : this->nodes_) {
      EXPECT_TRUE(this->SendGroup(node->node_id(), 1, source_index));
    }
  }

  // Check GroupSend fdrom each node to random targets
  for (uint16_t source_index(0); source_index < this->nodes_.size(); ++source_index) {
    for (uint16_t count(0); count < 1; ++count) {  // TODO(Alison) - max. value of count?
      NodeId node_id(NodeId::kRandomId);
      EXPECT_TRUE(this->SendGroup(node_id, 1, source_index));
    }
  }
}

class ProportionedRoutingStandAloneTest : public GenericNetwork, public testing::Test {
 public:
  ProportionedRoutingStandAloneTest(void)
      : GenericNetwork(),
        old_max_routing_table_size_(Parameters::max_routing_table_size),
        old_routing_table_size_threshold_(Parameters::routing_table_size_threshold),
        old_max_routing_table_size_for_client_(Parameters::max_routing_table_size_for_client),
        old_closest_nodes_size_(Parameters::closest_nodes_size),
        old_max_client_routing_table_size_(Parameters::max_client_routing_table_size),
        old_max_route_history_(Parameters::max_route_history),
        old_greedy_fraction_(Parameters::greedy_fraction) {
    // NB. relative calculations should match those in parameters.cc
    Parameters::max_routing_table_size = 32;
    Parameters::routing_table_size_threshold = Parameters::max_routing_table_size / 2;
    Parameters::max_routing_table_size_for_client = 8;
    Parameters::closest_nodes_size = 8;
    Parameters::max_client_routing_table_size = Parameters::max_routing_table_size;
    //    Parameters::max_route_history = 3;  // less than closest_nodes_size
    Parameters::greedy_fraction = Parameters::max_routing_table_size * 3 / 4;
  }

  virtual ~ProportionedRoutingStandAloneTest() {
    Parameters::max_routing_table_size = old_max_routing_table_size_;
    Parameters::routing_table_size_threshold = old_routing_table_size_threshold_;
    Parameters::max_routing_table_size_for_client = old_max_routing_table_size_for_client_;
    Parameters::closest_nodes_size = old_closest_nodes_size_;
    Parameters::max_client_routing_table_size = old_max_client_routing_table_size_;
    Parameters::max_route_history = old_max_route_history_;
    Parameters::greedy_fraction = old_greedy_fraction_;
  }

  virtual void SetUp() override { GenericNetwork::SetUp(); }

  virtual void TearDown() override {
    Sleep(std::chrono::microseconds(100));
    GenericNetwork::TearDown();
  }

 private:
  uint16_t old_max_routing_table_size_;
  uint16_t old_routing_table_size_threshold_;
  uint16_t old_max_routing_table_size_for_client_;
  uint16_t old_closest_nodes_size_;
  uint16_t old_max_client_routing_table_size_;
  uint16_t old_max_route_history_;
  uint16_t old_greedy_fraction_;
};

// TODO(Alison) - Add ProportionedRoutingStandAloneTest involving clients
TEST_F(ProportionedRoutingStandAloneTest, DISABLED_FUNC_ExtendedMessagePassing) {
  // Approx duration of test on Linux: 90mins
  this->SetUpNetwork(80, 0, 0, 0);

  ASSERT_TRUE(WaitForNodesToJoin());
  ASSERT_TRUE(WaitForHealthToStabiliseInLargeNetwork());

  EXPECT_TRUE(CheckGroupMatrixUniqueNodes());
  EXPECT_TRUE(CheckGroupMatrixUniqueNodes(3 / 2 * Parameters::closest_nodes_size + 1));

  for (uint16_t repeat(0); repeat < 10; ++repeat) {
    std::cout << "Repeat: " << repeat << std::endl;
    std::cout << "SendDirect..." << std::endl;
    ASSERT_TRUE(this->SendDirect(2, 10));
    NodeId target;
    std::cout << "SendGroup (to random)..." << std::endl;
    for (uint16_t i(0); i < nodes_.size(); ++i) {
      target = NodeId(NodeId::kRandomId);
      ASSERT_TRUE(SendGroup(target, 1, i, 10));
    }
    std::cout << "SendGroup (to existing)..." << std::endl;
    for (uint16_t i(0); i < nodes_.size(); ++i) {
      for (const auto& node : nodes_) {
        ASSERT_TRUE(SendGroup(node->node_id(), 1, i, 10));
      }
    }
  }
}

TEST_F(ProportionedRoutingStandAloneTest, DISABLED_FUNC_ExtendedMessagePassingSymmetricNat) {
  // Approx duration of test on Linux: 90mins
  this->SetUpNetwork(80, 0, 20, 0);

  ASSERT_TRUE(WaitForNodesToJoin());
  ASSERT_TRUE(WaitForHealthToStabiliseInLargeNetwork());

  EXPECT_TRUE(CheckGroupMatrixUniqueNodes());
  EXPECT_TRUE(CheckGroupMatrixUniqueNodes(3 / 2 * Parameters::closest_nodes_size + 1));

  for (uint16_t repeat(0); repeat < 10; ++repeat) {
    std::cout << "Repeat: " << repeat << std::endl;
    std::cout << "SendDirect..." << std::endl;
    ASSERT_TRUE(this->SendDirect(1, 10));
    NodeId target;
    std::cout << "SendGroup (to random)..." << std::endl;
    for (uint16_t i(0); i < nodes_.size(); ++i) {
      target = NodeId(NodeId::kRandomId);
      ASSERT_TRUE(SendGroup(target, 1, i, 10));
    }
    std::cout << "SendGroup (to existing)..." << std::endl;
    for (uint16_t i(0); i < nodes_.size(); ++i) {
      for (const auto& node : nodes_) {
        ASSERT_TRUE(SendGroup(node->node_id(), 1, i, 10));
      }
    }
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
