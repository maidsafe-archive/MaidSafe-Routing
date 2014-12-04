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
#include <atomic>

#include "maidsafe/passport/passport.h"
#include "maidsafe/rudp/nat_type.h"

#include "maidsafe/routing/tests/routing_network.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

template <typename T>
typename std::vector<T>::const_iterator Find(const T& t, const std::vector<T>& v) {
  return std::find_if(v.begin(), v.end(), [&t](const T& element) { return element == t; });
}

class RoutingChurnTest : public GenericNetwork, public testing::Test {
 public:
  RoutingChurnTest(void)
      : GenericNetwork(),
        old_close_nodes_(),
        new_close_nodes_(),
        expect_affected_(),
        close_nodes_change_check_(false),
        dropping_node_(false),
        adding_node_(false),
        node_on_operation_(RandomString(NodeId::kSize)),
        affected_nodes_(),
        checking_mutex_() {}

  virtual void SetUp() override { GenericNetwork::SetUp(); }

  virtual void TearDown() override { Sleep(std::chrono::microseconds(100)); }

  void CheckCloseNodesChange(std::shared_ptr<routing::CloseNodesChange> close_nodes_change,
                             const NodeId affected_node) {
    if (!close_nodes_change_check_)
      return;
    LOG(kInfo) << "Node " << HexSubstr(affected_node.string())
               << " having close_nodes_change change : ";
    close_nodes_change->Print();
    if (dropping_node_)
      DoDroppingCheck(close_nodes_change, affected_node);
    else if (adding_node_)
      DoAddingCheck(close_nodes_change, affected_node);
  }

 protected:
  void PopulateGlobals(const std::vector<NodeId>& existing_vault_node_ids,
                       const std::vector<NodeId>& bootstrap_node_ids,
                       const NodeId& node_to_operate) {
    old_close_nodes_.clear();
    new_close_nodes_.clear();
    expect_affected_.clear();
    affected_nodes_.clear();
    std::copy(existing_vault_node_ids.begin(), existing_vault_node_ids.end(),
              std::back_inserter(old_close_nodes_));
    std::copy(existing_vault_node_ids.begin(), existing_vault_node_ids.end(),
              std::back_inserter(new_close_nodes_));
    if (dropping_node_) {
      new_close_nodes_.erase(
          std::find(new_close_nodes_.begin(), new_close_nodes_.end(), node_to_operate));
    }

    SortIdsFromTarget(node_to_operate, new_close_nodes_);
    // Though reporting within Parameters::closest_nodes_size + Parameters::proximity_factor,
    // only radius of Parameters::closest_nodes_size is guaranteed
    std::copy(new_close_nodes_.begin(), new_close_nodes_.begin() + Parameters::closest_nodes_size,
              std::back_inserter(expect_affected_));
    for (auto& node : bootstrap_node_ids) {
      auto find(std::find(expect_affected_.begin(), expect_affected_.end(), node));
      if (find != expect_affected_.end())
        expect_affected_.erase(find);
    }
    LOG(kVerbose) << expect_affected_.size() << " nodes will be affected";
    if (adding_node_) {
      new_close_nodes_.push_back(node_to_operate);
    }
    node_on_operation_ = node_to_operate;
  }

  bool IsAllExpectedResponded() {
    std::lock_guard<std::mutex> lock(checking_mutex_);
    LOG(kVerbose) << "Following nodes expect to be affected : ";
    for (const auto& node : expect_affected_)
      LOG(kVerbose) << "     expect to be affected : " << HexSubstr(node.string());
    LOG(kVerbose) << "Following nodes are affected : ";
    for (const auto& node : affected_nodes_)
      LOG(kVerbose) << "                  affected : " << HexSubstr(node.string());
    //     for(auto& node : affected_nodes_)
    //       if (std::find(expect_affected_.begin(), expect_affected_.end(), node) ==
    //           expect_affected_.end()) {
    //         ADD_FAILURE() << "node " << HexSubstr(node.string()) << " shall not be affected";
    //         return;
    //       }
    for (const auto& node : expect_affected_)
      if (std::find(affected_nodes_.begin(), affected_nodes_.end(), node) ==
          affected_nodes_.end()) {
        ADD_FAILURE() << "node " << HexSubstr(node.string()) << " shall be affected but not";
        return false;
      }
    return true;
  }

  std::vector<NodeId> old_close_nodes_, new_close_nodes_, expect_affected_;
  std::atomic<bool> close_nodes_change_check_, dropping_node_, adding_node_;
  NodeId node_on_operation_;
  std::set<NodeId> affected_nodes_;
  std::mutex checking_mutex_;

 private:
  void DoDroppingCheck(std::shared_ptr<routing::CloseNodesChange> close_nodes_change,
                       const NodeId& affected_node) {
    auto lost_node(close_nodes_change->lost_node());

    LOG(kVerbose) << "close_nodes_change of affected node " << HexSubstr(affected_node.string())
                  << " containing following lost nodes :";
    //     bool not_found(true);
    if (lost_node.IsValid()) {
      LOG(kVerbose) << "    lost node : " << lost_node;
      //       if (node_id == node_on_operation_)
      //         not_found = false;
    }
    if (lost_node == node_on_operation_) {
      LOG(kVerbose) << "dropping node " << HexSubstr(node_on_operation_.string())
                    << " not find in the close_nodes_change of lost_node ";
      return;
    }
    std::lock_guard<std::mutex> lock(checking_mutex_);
    LOG(kVerbose) << "Affected node " << HexSubstr(affected_node.string())
                  << " inserted into affected_nodes_";
    affected_nodes_.insert(affected_node);
  }

  void DoAddingCheck(std::shared_ptr<routing::CloseNodesChange> close_nodes_change,
                     const NodeId& affected_node) {
    auto new_node(close_nodes_change->new_node());
    LOG(kVerbose) << "close_nodes_change of affected node " << HexSubstr(affected_node.string())
                  << " containing following new nodes :";
    //     bool not_found(true);
    if (new_node.IsValid()) {
      LOG(kVerbose) << "    new node : " << new_node;
      //       if (node_id == node_on_operation_)
      //         not_found = false;
    }
    if (new_node == node_on_operation_) {
      //     if (not_found) {
      LOG(kVerbose) << "new node " << HexSubstr(node_on_operation_.string())
                    << " not find in the close_nodes_change of new_node ";
      return;
    }
    std::lock_guard<std::mutex> lock(checking_mutex_);
    LOG(kVerbose) << "Affected node " << HexSubstr(affected_node.string())
                  << " inserted into affected_nodes_";
    affected_nodes_.insert(affected_node);
  }
};

TEST_F(RoutingChurnTest, FUNC_BasicNetworkChurn) {
  size_t random(RandomUint32());
  const size_t vault_network_size(10 + random % 10);
  const size_t clients_in_network(2 + random % 3);
  this->SetUpNetwork(vault_network_size, clients_in_network);
  // Existing vault node ids
  std::vector<NodeId> existing_client_node_ids, existing_vault_node_ids;
  for (size_t i(1); i < this->nodes_.size(); ++i) {
    if (this->nodes_[i]->IsClient())
      existing_client_node_ids.push_back(this->nodes_[i]->node_id());
    else
      existing_vault_node_ids.push_back(this->nodes_[i]->node_id());
  }

  for (int n(1); n < 51; ++n) {
    if (n % 2 == 0) {
      auto pmid(passport::CreatePmidAndSigner().first);
      this->AddNode(pmid);
      existing_vault_node_ids.push_back(NodeId(pmid.name()));
      Sleep(std::chrono::milliseconds(500 + RandomUint32() % 200));
    }

    if (n % 3 == 0) {
      std::random_shuffle(existing_vault_node_ids.begin(), existing_vault_node_ids.end());
      this->RemoveNode(existing_vault_node_ids.back());
      existing_vault_node_ids.pop_back();
      Sleep(std::chrono::milliseconds(500 + RandomUint32() % 200));
    }
  }
}

TEST_F(RoutingChurnTest, DISABLED_FUNC_MessagingNetworkChurn) {
  size_t random(RandomUint32());
  const size_t vault_network_size(20 + random % 10);
  const size_t clients_in_network(5 + random % 3);
  this->SetUpNetwork(vault_network_size, clients_in_network);
  LOG(kInfo) << "Finished setting up network\n\n\n\n";

  std::vector<NodeId> existing_node_ids;
  for (const auto& node : this->nodes_)
    existing_node_ids.push_back(node->node_id());
  LOG(kInfo) << "After harvesting node ids\n\n\n\n";

  std::vector<passport::Pmid> new_nodes;
  const size_t up_count(vault_network_size / 3), down_count(vault_network_size / 5);
  size_t downed(0);
  while (new_nodes.size() < up_count)
    new_nodes.emplace_back(passport::CreatePmidAndSigner().first);
  LOG(kInfo) << "After generating new ids\n\n\n\n";

  // Start thread for messaging between clients and clients to groups
  std::string message(RandomString(4096));
  volatile bool run(true);
  auto messaging_handle = std::async(std::launch::async, [=, &run] {
    LOG(kInfo) << "Before messaging loop";
    while (run) {
      GenericNetwork::NodePtr sender_client(this->RandomClientNode());
      GenericNetwork::NodePtr receiver_client(this->RandomClientNode());
      GenericNetwork::NodePtr vault_node(this->RandomVaultNode());
      // Choose random client nodes for direct message
      // TODO(Alison) - use result?
      sender_client->SendDirect(receiver_client->node_id(), message, false, [](std::string) {});
      // Choose random client for group message to random env
      // TODO(Alison) - use result?
      sender_client->SendGroup(NodeId(RandomString(NodeId::kSize)), message, false,
                               [](std::string) {});
      // Choose random vault for group message to random env
      // TODO(Alison) - use result?
      vault_node->SendGroup(NodeId(RandomString(NodeId::kSize)), message, false,
                            [](std::string) {});
      // Wait before going again
      Sleep(std::chrono::milliseconds(900 + RandomUint32() % 200));
      LOG(kInfo) << "Ran messaging iteration";
    }
    LOG(kInfo) << "After messaging loop";
  });
  LOG(kInfo) << "Started messaging thread\n\n\n\n";

  // Start thread to bring down nodes
  auto down_handle = std::async(std::launch::async, [=, &run, &down_count, &downed] {
    while (run && downed < down_count) {
      //                                    if (RandomUint32() % 5 == 0)
      //                                      this->RemoveRandomClient();
      //                                    else
      this->RemoveRandomVault();
      ++downed;
      Sleep(std::chrono::seconds(10));
    }
  });

  // Start thread to bring up nodes
  auto up_handle = std::async(std::launch::async, [=, &run, &new_nodes] {
    while (run) {
      if (new_nodes.empty())
        return;
      //                                  if (RandomUint32() % 5 == 0)
      //                                    this->AddNode(true, new_node_ids.back());
      //                                  else
      this->AddNode(new_nodes.back());
      new_nodes.pop_back();
      Sleep(std::chrono::seconds(3));
    }
  });

  // Let stuff run for a while
  down_handle.get();
  up_handle.get();

  // Stop all threads
  run = false;
  messaging_handle.get();

  LOG(kInfo) << "\n\t Initial count of Vault nodes : " << vault_network_size
             << "\n\t Initial count of client nodes : " << clients_in_network
             << "\n\t Current count of nodes : " << this->nodes_.size()
             << "\n\t Up count of nodes : " << up_count
             << "\n\t down_count count of nodes : " << down_count;
  auto expected_current_size = vault_network_size + clients_in_network + up_count - down_count;
  EXPECT_EQ(expected_current_size, this->nodes_.size());
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
