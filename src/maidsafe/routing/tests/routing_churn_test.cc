/* Copyright 2012 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

#include <vector>

#include "maidsafe/rudp/nat_type.h"

#include "maidsafe/routing/tests/routing_network.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

template <typename T>
typename std::vector<T>::const_iterator Find(const T& t, const std::vector<T>& v) {
  return std::find_if(v.begin(), v.end(), [&t] (const T& element) {
                                              return element == t;
                                            });
}

class RoutingChurnTest : public GenericNetwork, public testing::Test {
 public:
  RoutingChurnTest(void) : GenericNetwork() {}

  virtual void SetUp() {
    GenericNetwork::SetUp();
  }

  virtual void TearDown() {
    Sleep(std::chrono::microseconds(100));
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
      NodeId new_node(NodeId::kRandomId);
      while (std::find_if(existing_vault_node_ids.begin(),
                          existing_vault_node_ids.end(),
                          [&new_node] (const NodeId& element) { return element == new_node; }) !=
             existing_vault_node_ids.end()) {
        new_node = NodeId(NodeId::kRandomId);
      }
      this->AddNode(false, new_node);
      existing_vault_node_ids.push_back(new_node);
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

TEST_F(RoutingChurnTest, FUNC_MessagingNetworkChurn) {
  size_t random(RandomUint32());
  const size_t vault_network_size(20 + random % 10);
  const size_t clients_in_network(5 + random % 3);
  this->SetUpNetwork(vault_network_size, clients_in_network);
  LOG(kInfo) << "Finished setting up network\n\n\n\n";

  std::vector<NodeId> existing_node_ids;
  for (const auto& node : this->nodes_)
    existing_node_ids.push_back(node->node_id());
  LOG(kInfo) << "After harvesting node ids\n\n\n\n";

  std::vector<NodeId> new_node_ids;
  const size_t up_count(vault_network_size / 3), down_count(vault_network_size / 5);
  size_t downed(0);
  while (new_node_ids.size() < up_count) {
    NodeId new_id(NodeId::kRandomId);
    auto itr(Find(new_id, existing_node_ids));
    if (itr == existing_node_ids.end())
      new_node_ids.push_back(new_id);
  }
  LOG(kInfo) << "After generating new ids\n\n\n\n";

  // Start thread for messaging between clients and clients to groups
  std::string message(RandomString(4096));
  volatile bool run(true);
  auto messaging_handle = std::async(std::launch::async,
                                     [=, &run] {
                                       LOG(kInfo) << "Before messaging loop";
                                       while (run) {
                                         GenericNetwork::NodePtr sender_client(
                                            this->RandomClientNode());
                                         GenericNetwork::NodePtr receiver_client(
                                            this->RandomClientNode());
                                         GenericNetwork::NodePtr vault_node(
                                            this->RandomVaultNode());
                                         // Choose random client nodes for direct message
                                         // TODO(Alison) - use result?
                                         sender_client->SendDirect(receiver_client->node_id(),
                                                                   message,
                                                                   false,
                                                                   [](std::string /*str*/) {});
                                         // Choose random client for group message to random env
                                         // TODO(Alison) - use result?
                                         sender_client->SendGroup(NodeId(NodeId::kRandomId),
                                                                  message,
                                                                  false,
                                                                  [](std::string /*str*/) {});
                                         // Choose random vault for group message to random env
                                         // TODO(Alison) - use result?
                                         vault_node->SendGroup(NodeId(NodeId::kRandomId),
                                                               message,
                                                               false,
                                                               [](std::string /*str*/) {});
                                         // Wait before going again
                                         Sleep(std::chrono::milliseconds(900 +
                                                                               RandomUint32() %
                                                                               200));
                                         LOG(kInfo) << "Ran messaging iteration";
                                       }
                                       LOG(kInfo) << "After messaging loop";
                                     });
  LOG(kInfo) << "Started messaging thread\n\n\n\n";

  // Start thread to bring down nodes
  auto down_handle = std::async(std::launch::async,
                                [=, &run, &down_count, &downed] {
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
  auto up_handle = std::async(std::launch::async,
                              [=, &run, &new_node_ids] {
                                while (run) {
                                  if (new_node_ids.empty())
                                    return;
//                                  if (RandomUint32() % 5 == 0)
//                                    this->AddNode(true, new_node_ids.back());
//                                  else
                                    this->AddNode(false, new_node_ids.back());
                                  new_node_ids.pop_back();
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
