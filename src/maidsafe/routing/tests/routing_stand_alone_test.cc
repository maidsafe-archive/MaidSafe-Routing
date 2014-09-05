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

#include "boost/filesystem.hpp"

#include "maidsafe/passport/passport.h"
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
    std::sort(std::begin(pmid_ids), std::end(pmid_ids), [&](const NodeId& lhs, const NodeId& rhs) {
      return NodeId::CloserToTarget(lhs, rhs, maid_id);
    });
    return !NodeId::CloserToTarget(pmid_ids.at(Parameters::max_routing_table_size_for_client - 1),
                                   pmid_id, maid_id);
  }
};

TEST_F(RoutingStandAloneTest, FUNC_VaultSendToClient) {
  this->SetUpNetwork(kServerSize, 1);
  for (size_t index(0); index < this->ClientIndex(); ++index) {
    EXPECT_TRUE(this->SendDirect(this->nodes_[index], this->nodes_[this->ClientIndex()]->node_id(),
                                 ExpectedNodeType::kExpectClient))
        << DebugId(this->nodes_[index]->node_id());
  }
}

TEST_F(RoutingStandAloneTest, FUNC_ClientRoutingTableUpdate) {
  this->SetUpNetwork(kServerSize);
  this->AddNode(passport::CreateMaidAndSigner().first);
  NodeId maid_id(nodes_[kServerSize]->GetMaid().name()), pmid_id;
  while (this->nodes_.size() < kServerSize + Parameters::max_routing_table_size_for_client) {
    auto pmid(passport::CreatePmidAndSigner().first);
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
  unsigned int num_symmetric_vaults(kServerSize / 3);
  for (unsigned int i(0); i < num_symmetric_vaults; ++i)
    this->AddNode(false, true);
}

TEST_F(RoutingStandAloneTest, FUNC_SetupNetworkAddVaultsBehindSymmetricNatAndClients) {
  this->SetUpNetwork(kServerSize, kClientSize);
  unsigned int num_symmetric_vaults(kServerSize / 3);
  for (unsigned int i(0); i < num_symmetric_vaults; ++i)
    this->AddNode(false, true);
  unsigned int num_symmetric_clients(kClientSize);
  for (unsigned int i(0); i < num_symmetric_clients; ++i)
    this->AddNode(true, false);  // Add more normal clients
}

TEST_F(RoutingStandAloneTest, FUNC_SetupNetworkAddNodesBehindSymmetricNat) {
  this->SetUpNetwork(kServerSize, kClientSize);
  unsigned int num_symmetric_vaults(kServerSize / 3);
  for (unsigned int i(0); i < num_symmetric_vaults; ++i)
    this->AddNode(false, true);
  unsigned int num_symmetric_clients(kClientSize);
  for (unsigned int i(0); i < num_symmetric_clients; ++i)
    this->AddNode(true, true);  // Add clients behind symmetric NAT
}

TEST_F(RoutingStandAloneTest, DISABLED_FUNC_ExtendedSendMulti) {
  // N.B. This test takes approx. 1hr to run, hence it is disabled.
  this->SetUpNetwork(kServerSize);
  unsigned int loop(100);
  while (loop-- > 0) {
    EXPECT_TRUE(SendDirect(40));
    this->ClearMessages();
  }
}

TEST_F(RoutingStandAloneTest, FUNC_ExtendedSendToGroup) {
  unsigned int message_count(10), receivers_message_count(0);
  this->SetUpNetwork(kServerSize);
  size_t last_index(this->nodes_.size() - 1);
  NodeId dest_id(this->nodes_[last_index]->node_id());

  unsigned int loop(100);
  while (loop-- > 0) {
    EXPECT_TRUE(SendGroup(dest_id, message_count));
    for (size_t index = 0; index != (last_index); ++index)
      receivers_message_count += static_cast<unsigned int>(this->nodes_.at(index)->MessagesSize());

    EXPECT_EQ(0, this->nodes_[last_index]->MessagesSize())
        << "Not expected message at Node : "
        << HexSubstr(this->nodes_[last_index]->node_id().string());
    EXPECT_EQ(message_count * (Parameters::group_size), receivers_message_count);
    receivers_message_count = 0;
    this->ClearMessages();
  }
}

TEST_F(RoutingStandAloneTest, FUNC_ExtendedSendToGroupRandomId) {
  unsigned int message_count(50), receivers_message_count(0);
  this->SetUpNetwork(kServerSize);
  unsigned int loop(10);
  while (loop-- > 0) {
    for (unsigned int index(0); index < message_count; ++index) {
      NodeId random_id(NodeId::IdType::kRandomId);
      std::vector<NodeId> groupd_ids(this->GroupIds(random_id));
      EXPECT_TRUE(SendGroup(random_id, 1));
      for (const auto& node : this->nodes_) {
        if (std::find(groupd_ids.begin(), groupd_ids.end(), node->node_id()) != groupd_ids.end()) {
          receivers_message_count += static_cast<unsigned int>(node->MessagesSize());
          node->ClearMessages();
        }
      }
    }
    EXPECT_EQ(message_count * (Parameters::group_size), receivers_message_count);
    LOG(kVerbose) << "Total message received count : " << message_count * (Parameters::group_size);
    receivers_message_count = 0;
    this->ClearMessages();
  }
}

TEST_F(RoutingStandAloneTest, FUNC_JoinAfterBootstrapLeaves) {
  this->SetUpNetwork(kServerSize);
  Sleep(std::chrono::seconds(10));
  this->AddNode(passport::CreatePmidAndSigner().first);
}

TEST_F(RoutingStandAloneTest, FUNC_ReBootstrap) {
  this->SetUpNetwork(3);
  nodes_.erase(std::begin(nodes_));
  nodes_.erase(std::begin(nodes_));
  Sleep(std::chrono::seconds(1));
  boost::filesystem::remove(detail::GetOverrideBootstrapFilePath<false>());
  SetUp();
  Sleep(std::chrono::seconds(Parameters::re_bootstrap_time_lag));
  EXPECT_EQ(nodes_.front()->RoutingTable().size(), nodes_.size() - 1);
}

TEST_F(RoutingStandAloneTest, FUNC_RetryingJoin) {
  nodes_.erase(std::begin(nodes_));
  nodes_.erase(std::begin(nodes_));
  Sleep(std::chrono::seconds(1));
  auto node(std::make_shared<GenericNode>(passport::CreatePmidAndSigner().first));
  nodes_.insert(std::begin(nodes_), node);
  AddPublicKey(node->node_id(), node->public_key());
  SetNodeValidationFunctor(node);
  node->Join();
  Sleep(Parameters::re_bootstrap_time_lag);
  EXPECT_EQ(node->RoutingTable().size(), 0);
  boost::filesystem::remove(detail::GetOverrideBootstrapFilePath<false>());
  SetUp();
  Sleep(Parameters::re_bootstrap_time_lag);
  Sleep(Parameters::re_bootstrap_time_lag);
  EXPECT_EQ(node->RoutingTable().size(), nodes_.size() - 1);
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
        old_max_route_history_(Parameters::max_route_history) {
    // NB. relative calculations should match those in parameters.cc
    Parameters::max_routing_table_size = 32;
    Parameters::routing_table_size_threshold = Parameters::max_routing_table_size / 2;
    Parameters::max_routing_table_size_for_client = 8;
    Parameters::closest_nodes_size = 8;
    Parameters::max_client_routing_table_size = Parameters::max_routing_table_size;
    //    Parameters::max_route_history = 3;  // less than closest_nodes_size
  }

  virtual ~ProportionedRoutingStandAloneTest() {
    Parameters::max_routing_table_size = old_max_routing_table_size_;
    Parameters::routing_table_size_threshold = old_routing_table_size_threshold_;
    Parameters::max_routing_table_size_for_client = old_max_routing_table_size_for_client_;
    Parameters::closest_nodes_size = old_closest_nodes_size_;
    Parameters::max_client_routing_table_size = old_max_client_routing_table_size_;
    Parameters::max_route_history = old_max_route_history_;
  }

  virtual void SetUp() override { GenericNetwork::SetUp(); }

  virtual void TearDown() override {
    Sleep(std::chrono::microseconds(100));
    GenericNetwork::TearDown();
  }

 private:
  unsigned int old_max_routing_table_size_;
  unsigned int old_routing_table_size_threshold_;
  unsigned int old_max_routing_table_size_for_client_;
  unsigned int old_closest_nodes_size_;
  unsigned int old_max_client_routing_table_size_;
  unsigned int old_max_route_history_;
};

// TODO(Alison) - Add ProportionedRoutingStandAloneTest involving clients
TEST_F(ProportionedRoutingStandAloneTest, DISABLED_FUNC_ExtendedMessagePassing) {
  // Approx duration of test on Linux: 90mins
  this->SetUpNetwork(80, 0, 0, 0);

  ASSERT_TRUE(WaitForNodesToJoin());
  ASSERT_TRUE(WaitForHealthToStabiliseInLargeNetwork());

  for (unsigned int repeat(0); repeat < 10; ++repeat) {
    std::cout << "Repeat: " << repeat << std::endl;
    std::cout << "SendDirect..." << std::endl;
    ASSERT_TRUE(this->SendDirect(2, 10));
    NodeId target;
    std::cout << "SendGroup (to random)..." << std::endl;
    for (unsigned int i(0); i < nodes_.size(); ++i) {
      target = NodeId(NodeId::IdType::kRandomId);
      ASSERT_TRUE(SendGroup(target, 1, i, 10));
    }
    std::cout << "SendGroup (to existing)..." << std::endl;
    for (unsigned int i(0); i < nodes_.size(); ++i) {
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

  for (unsigned int repeat(0); repeat < 10; ++repeat) {
    std::cout << "Repeat: " << repeat << std::endl;
    std::cout << "SendDirect..." << std::endl;
    ASSERT_TRUE(this->SendDirect(1, 10));
    NodeId target;
    std::cout << "SendGroup (to random)..." << std::endl;
    for (unsigned int i(0); i < nodes_.size(); ++i) {
      target = NodeId(NodeId::IdType::kRandomId);
      ASSERT_TRUE(SendGroup(target, 1, i, 10));
    }
    std::cout << "SendGroup (to existing)..." << std::endl;
    for (unsigned int i(0); i < nodes_.size(); ++i) {
      for (const auto& node : nodes_) {
        ASSERT_TRUE(SendGroup(node->node_id(), 1, i, 10));
      }
    }
  }
}

TEST_F(RoutingStandAloneTest, FUNC_SendToClientsWithSameId) {
  this->SetUpNetwork(20, 0);
  const unsigned int kMessageCount(5);
  auto maid(passport::CreateMaidAndSigner().first);
  for (unsigned int index(0); index < 4; ++index)
    AddNode(maid);

  for (unsigned int index(0); index < kMessageCount; ++index)
    EXPECT_TRUE(SendDirect(nodes_[kServerSize], nodes_[kServerSize]->node_id(), kExpectClient));
  unsigned int num_of_tries(0);
  bool done(false);
  do {
    Sleep(std::chrono::seconds(1));
    size_t size(0);
    for (const auto& node : nodes_) {
      size += node->MessagesSize();
    }
    if (4 * kMessageCount == size) {
      done = true;
      num_of_tries = 4;
    }
    ++num_of_tries;
  } while (num_of_tries < 5);
  EXPECT_TRUE(done);  // the number of 20 may need to be increased
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
