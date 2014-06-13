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
#include <functional>
#include <iterator>
#include <memory>
#include <set>
#include <vector>

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {
namespace routing {
namespace test {

struct RTNode {
 private:
  NodeId id;

 public:
  explicit RTNode(const NodeId& node_id)
      : id(node_id),
        close_nodes(new CloseNodes([node_id](const NodeId & id1, const NodeId & id2) {
          return NodeId::CloserToTarget(id1, id2, node_id);
        })),
        accounts(),
        group_matrix() {}

  RTNode(const RTNode& other)
      : id(other.id),
        close_nodes(other.close_nodes),
        accounts(other.accounts),
        group_matrix(other.group_matrix) {}

  RTNode& operator=(RTNode&& other) {
    id = std::move(other.id);
    close_nodes = std::move(other.close_nodes);
    accounts = std::move(other.accounts);
    group_matrix = std::move(other.group_matrix);
    return *this;
  }

  typedef std::set<NodeId, std::function<bool(const NodeId&, const NodeId&)>> CloseNodes;
  typedef std::set<RTNode, std::function<bool(const RTNode&, const RTNode&)>> CloseRTNodes;
  std::vector<NodeId> GetGroupMatrix() const;
  NodeId node_id() const { return id; }

  std::shared_ptr<CloseNodes> close_nodes;
  std::vector<NodeId> accounts;
  std::map<NodeId, std::shared_ptr<CloseNodes>> group_matrix;
};

std::vector<NodeId> RTNode::GetGroupMatrix() const {
  std::set<NodeId> matrix_set;
  for (auto itr(close_nodes->begin()); itr != close_nodes->end(); ++itr) {
    matrix_set.insert(*itr);
    matrix_set.insert(group_matrix.at(*itr)->begin(), group_matrix.at(*itr)->end());
  }
  std::vector<NodeId> matrix_vector;
  for (auto& element : matrix_set)
    matrix_vector.push_back(element);
  return matrix_vector;
}

class Network {
 public:
  Network() { out_file.open("log.txt"); }

  void Add(const NodeId& node_id);
  void AddAccount(const NodeId& account);
  void RoutingAdd(const NodeId& node_id);

  void PruneNetwork();
  void PruneAllAccounts();
  bool RemovePeer(const NodeId& node_id, const NodeId& requester);
  RTNode MakeNode(const NodeId& node_id);
  void UpdateNetwork(RTNode& new_node);
  void UpdateAccounts(RTNode& new_node);
  bool IsResponsibleForAccount(const RTNode& node, const NodeId& account);
  bool IsResponsibleForAccountMatrix(const RTNode& node, const NodeId& account,
                                     std::vector<NodeId>& node_ids);
  bool Validate();
  uint16_t PartialSortFromTarget(const NodeId& target, uint16_t number);
  uint16_t PartialSortFromTarget(const NodeId& target, uint16_t number, std::vector<RTNode>& nodes);
  void RemoveAccount(const RTNode& node, const NodeId& account);
  void PrintNetworkInfo();
  std::vector<size_t> CheckGroupMatrixReliablity();
  void CheckReliability(const NodeId& target, std::vector<size_t>& close_nodes_results,
                        std::vector<size_t>& matrix_results);
  void TransferAndDeleteAccountFromInformedNodes(const NodeId& informed_node_id,
                                                 const NodeId& new_node_id,
                                                 std::vector<NodeId>& informed_node_group_matrix);
  void IdealUpdateAccounts(RTNode& new_node);
  void IdealRemoveAccount(const NodeId& account);
  void TransferAccount(const NodeId& new_node_id, const NodeId& account);
  void DeleteAccount(const NodeId& node_id, const NodeId& account);
  void UpdateAccountsOfNewAndInformedNodes(const RTNode& new_node);
  RTNode::CloseRTNodes GetClosestRTNodes(RTNode::CloseRTNodes& closest_nodes,
                                         const NodeId& node_id);
  std::vector<NodeId> GetClosestNodes(const NodeId& node_id);

  std::fstream out_file;
  std::vector<RTNode> nodes_;
  std::vector<NodeId> accounts_;
};

void Network::PruneAllAccounts() {
  std::vector<NodeId> matrix;
  for (auto& node : nodes_) {
    matrix = node.GetGroupMatrix();
    std::vector<NodeId> accounts_to_delete;
    for (auto& account : node.accounts) {
      std::partial_sort(matrix.begin(), matrix.begin() + 4, matrix.end(),
                        [account](const NodeId & lhs, const NodeId & rhs) {
        return NodeId::CloserToTarget(lhs, rhs, account);
      });
      if (NodeId::CloserToTarget(matrix[3], node.node_id(), account))
        accounts_to_delete.push_back(account);
    }
    for (auto account : accounts_to_delete) {
      LOG(kInfo) << DebugId(node.node_id()) << " PruneAllAccounts " << DebugId(account);
      node.accounts.erase(std::remove(node.accounts.begin(), node.accounts.end(), account),
                          node.accounts.end());
    }
  }
}

std::vector<NodeId> Network::GetClosestNodes(const NodeId& node_id) {
  RTNode::CloseRTNodes close_rt_nodes([node_id](const RTNode & lhs, const RTNode & rhs) {
    return NodeId::CloserToTarget(lhs.node_id(), rhs.node_id(), node_id);
  });
  auto return_closest_rts(GetClosestRTNodes(close_rt_nodes, node_id));
  std::vector<NodeId> closest_node_ids;
  auto eight_itr(return_closest_rts.begin());
  std::advance(eight_itr, std::min(size_t(8), nodes_.size()));
  for (auto itr(return_closest_rts.begin()); itr != eight_itr; ++itr)
    closest_node_ids.push_back(itr->node_id());
  return closest_node_ids;
}

RTNode::CloseRTNodes Network::GetClosestRTNodes(RTNode::CloseRTNodes& closest_nodes,
                                                const NodeId& node_id) {
  if (nodes_.empty())
    return RTNode::CloseRTNodes();
  if (closest_nodes.empty()) {
    PartialSortFromTarget(node_id, 1);
    closest_nodes.insert(nodes_[0]);
    return GetClosestRTNodes(closest_nodes, node_id);
  }
  if (closest_nodes.size() == 1) {
    std::vector<NodeId> node_ids;
    node_ids = closest_nodes.begin()->GetGroupMatrix();
    if (!node_ids.empty()) {
      std::partial_sort(node_ids.begin(), node_ids.begin() + std::min(size_t(8), nodes_.size()),
                        node_ids.end(), [node_id](const NodeId & lhs, const NodeId & rhs) {
        return NodeId::CloserToTarget(lhs, rhs, node_id);
      });
      for (auto outer_iter(node_ids.begin() + 1);
           outer_iter != node_ids.begin() + std::min(size_t(8), nodes_.size()); ++outer_iter) {
        NodeId id(*outer_iter);
        auto node_itr(std::find_if(nodes_.begin(), nodes_.end(),
                                   [id](const RTNode & node) { return node.node_id() == id; }));
        assert(node_itr != nodes_.end());
        closest_nodes.insert(*node_itr);
      }
    }
    if ((closest_nodes.size() == 1) || (nodes_.size() <= 8))
      return closest_nodes;
    return GetClosestRTNodes(closest_nodes, node_id);
  }
  size_t initial_size(closest_nodes.size());
  auto furthest_node(closest_nodes.rbegin());
  std::vector<NodeId> matrix;
  for (auto& close_node : closest_nodes) {
    matrix = close_node.GetGroupMatrix();
    for (auto& element : matrix) {
      if (NodeId::CloserToTarget(element, furthest_node->node_id(), node_id)) {
        auto node_itr(std::find_if(nodes_.begin(), nodes_.end(), [element](const RTNode & node) {
          return node.node_id() == element;
        }));
        assert(node_itr != nodes_.end());
        closest_nodes.insert(*node_itr);
      }
    }
  }
  if (closest_nodes.size() == initial_size)
    return closest_nodes;
  return GetClosestRTNodes(closest_nodes, node_id);
}

void Network::Add(const NodeId& node_id) {
  auto node = MakeNode(node_id);
  UpdateNetwork(node);
  nodes_.push_back(node);
  PruneNetwork();
  LOG(kInfo) << "Added NodeId : " << DebugId(node_id);
  // add/remove accounts to new node from nodes who noticed new node
  UpdateAccountsOfNewAndInformedNodes(node);
}

void Network::RoutingAdd(const NodeId& node_id) {
  auto routing_closest_nodes(GetClosestNodes(node_id));
  auto node = MakeNode(node_id);
  for (auto routing_closest_node : routing_closest_nodes)
    EXPECT_NE(std::find(node.close_nodes->begin(), node.close_nodes->end(), routing_closest_node),
              node.close_nodes->end());
  UpdateNetwork(node);
  nodes_.push_back(node);
  PruneNetwork();
}

RTNode Network::MakeNode(const NodeId& node_id) {
  PartialSortFromTarget(node_id, 8);
  RTNode node(node_id);
  std::string message(DebugId(node.node_id()) + " added");
  for (size_t i = 0; (i < 8) && i < nodes_.size(); ++i) {
    node.close_nodes->insert(nodes_[i].node_id());
    node.group_matrix[nodes_[i].node_id()] = nodes_[i].close_nodes;
    message += "   " + DebugId(nodes_[i].node_id());
    nodes_[i].close_nodes->insert(node.node_id());
    nodes_[i].group_matrix[node.node_id()] = node.close_nodes;
  }
  LOG(kInfo) << message;
  return node;
}

void Network::UpdateNetwork(RTNode& new_node) {
  for (size_t index(8); index < nodes_.size(); ++index) {
    const NodeId& node_id = nodes_[index].node_id();
    auto eighth_closest(nodes_[index].close_nodes->begin());
    std::advance(eighth_closest, 7);
    if (NodeId::CloserToTarget(new_node.node_id(), *eighth_closest, node_id)) {
      LOG(kVerbose) << DebugId(node_id) << " network added " << DebugId(new_node.node_id());
      new_node.close_nodes->insert(node_id);
      new_node.group_matrix[nodes_[index].node_id()] = nodes_[index].close_nodes;
      nodes_[index].close_nodes->insert(new_node.node_id());
      nodes_[index].group_matrix[new_node.node_id()] = new_node.close_nodes;
    }
  }
}

void Network::UpdateAccountsOfNewAndInformedNodes(const RTNode& new_node) {
  // find nodes who noticed new node
  for (const auto& node : nodes_) {
    auto informed_node_group_matrix(node.GetGroupMatrix());

    if (std::find(informed_node_group_matrix.begin(), informed_node_group_matrix.end(),
                  new_node.node_id()) != informed_node_group_matrix.end()) {
      // transfer account
      if (node.node_id() != new_node.node_id())
        TransferAndDeleteAccountFromInformedNodes(node.node_id(), new_node.node_id(),
                                                  informed_node_group_matrix);
    }
  }
}

void Network::TransferAndDeleteAccountFromInformedNodes(
    const NodeId& informed_node_id, const NodeId& new_node_id,
    std::vector<NodeId>& informed_node_group_matrix) {
  // find informed node
  auto informed_node =
      std::find_if(nodes_.begin(), nodes_.end(), [&informed_node_id](const RTNode & rt_node) {
        return (rt_node.node_id() == informed_node_id);
      });
  assert(informed_node != nodes_.end());

  std::vector<NodeId> accounts_to_delete;
  // Sort matrix per account
  for (const auto& account : informed_node->accounts) {
    std::sort(informed_node_group_matrix.begin(), informed_node_group_matrix.end(),
              [&account](const NodeId & lhs,
                         const NodeId & rhs) { return NodeId::CloserToTarget(lhs, rhs, account); });
    bool delete_account(true);
    std::vector<NodeId> closest_holders;
    for (auto i(0U); (i < informed_node_group_matrix.size() && i < 4); ++i) {
      // transfer account if needed
      if (informed_node_group_matrix.at(i) == new_node_id) {
        TransferAccount(new_node_id, account);
      }
      if (informed_node_group_matrix.at(i) == informed_node_id) {
        delete_account = false;
      }
      closest_holders.push_back(informed_node_group_matrix.at(i));
    }

    // Delete account if not responsible any more
    if (delete_account) {
      std::string holders_string;
      for (const auto& closest_holder : closest_holders)
        holders_string = holders_string + "  " + DebugId(closest_holder) + "  ";
      accounts_to_delete.push_back(account);
      LOG(kInfo) << "DeleteAccount needed for " << DebugId(informed_node_id) << ",  account "
                 << DebugId(account) << "  Holders : \n" << holders_string;
    }
  }

  for (const auto& account : accounts_to_delete) {
    DeleteAccount(informed_node_id, account);
  }
}

void Network::TransferAccount(const NodeId& new_node_id, const NodeId& account) {
  LOG(kInfo) << "TransferAccount called for " << DebugId(new_node_id)
             << " for account : " << DebugId(account);
  auto new_node(std::find_if(nodes_.begin(), nodes_.end(), [&new_node_id](const RTNode & rt_node) {
    return (rt_node.node_id() == new_node_id);
  }));
  assert(new_node != nodes_.end());
  if (std::find(new_node->accounts.begin(), new_node->accounts.end(), account) ==
      new_node->accounts.end()) {
    new_node->accounts.push_back(account);
  }
}

void Network::DeleteAccount(const NodeId& node_id, const NodeId& account) {
  auto node(std::find_if(nodes_.begin(), nodes_.end(), [&node_id](const RTNode & rt_node) {
    return (rt_node.node_id() == node_id);
  }));
  assert(node != nodes_.end());
  node->accounts.erase(std::remove(node->accounts.begin(), node->accounts.end(), account));
}

void Network::AddAccount(const NodeId& account) {
  uint16_t count(PartialSortFromTarget(account, 4));
  for (uint16_t index(0); index != count; ++index) {
    nodes_[index].accounts.push_back(account);
    LOG(kInfo) << "Adding account < " << DebugId(account)
               << " > to Node : " << DebugId(nodes_[index].node_id());
  }
  accounts_.push_back(account);
}

void Network::PruneNetwork() {
  for (auto& node : nodes_) {
    if (node.close_nodes->size() <= 8)
      continue;
    auto itr(node.close_nodes->begin());
    std::advance(itr, 8);
    while (itr != node.close_nodes->end()) {
      if (RemovePeer(*itr, node.node_id())) {
        LOG(kInfo) << DebugId(*itr) << " and " << DebugId(node.node_id()) << " removed each other";
        itr = node.close_nodes->erase(itr);
      } else {
        ++itr;
      }
    }
  }
}

bool Network::RemovePeer(const NodeId& node_id, const NodeId& requester) {
  auto node(std::find_if(nodes_.begin(), nodes_.end(), [&node_id](const RTNode & rt_node) {
    return (rt_node.node_id() == node_id);
  }));
  auto peer_itr(node->close_nodes->find(requester));
  if (std::distance(node->close_nodes->begin(), peer_itr) > 8) {
    LOG(kVerbose) << DebugId(node->node_id()) << " removes peer " << DebugId(*peer_itr);
    node->close_nodes->erase(peer_itr);
    return true;
  }
  return false;
}

uint16_t Network::PartialSortFromTarget(const NodeId& target, uint16_t number) {
  return PartialSortFromTarget(target, number, nodes_);
}

uint16_t Network::PartialSortFromTarget(const NodeId& target, uint16_t number,
                                        std::vector<RTNode>& nodes) {
  uint16_t count = std::min(number, static_cast<uint16_t>(nodes.size()));
  std::partial_sort(nodes.begin(), nodes.begin() + count, nodes.end(),
                    [&target](const RTNode & lhs, const RTNode & rhs) {
    return NodeId::CloserToTarget(lhs.node_id(), rhs.node_id(), target);
  });
  return count;
}

bool Network::Validate() {
  size_t index(0);
  for (const auto& account : accounts_) {
    ++index;
    volatile size_t count(std::count_if(nodes_.begin(), nodes_.end(),
                                        [&account](const RTNode & node) {
      return std::find(node.accounts.begin(), node.accounts.end(), account) != node.accounts.end();
    }));

    std::sort(nodes_.begin(), nodes_.end(), [&account](const RTNode & lhs, const RTNode & rhs) {
      return NodeId::CloserToTarget(lhs.node_id(), rhs.node_id(), account);
    });
    for (auto itr(nodes_.begin()); itr != nodes_.begin() + std::min(size_t(4), nodes_.size());
         ++itr) {
      EXPECT_NE(std::find(itr->accounts.begin(), itr->accounts.end(), account), itr->accounts.end())
          << "Node: " << DebugId(itr->node_id()) << " does not have " << DebugId(account);
    }

    if (count > 4) {
      LOG(kError) << "Account " << DebugId(account) << " # of holders: " << count
                  << "  index: " << index;
      std::vector<NodeId> matrix;
      std::string matrix_string, close_nodes_string;
      for (size_t index(0); index < count; ++index) {
        matrix_string.clear();
        close_nodes_string.clear();
        if (index > 3) {
          matrix_string =
              "Matrix for invalid holder " + DebugId(nodes_[index].node_id()) + " are:\n";
          close_nodes_string =
              "Close nodes for invalid holder " + DebugId(nodes_[index].node_id()) + " are:\n";
        } else {
          matrix_string = "Matrix for valid holder " + DebugId(nodes_[index].node_id()) + " are:\n";
          close_nodes_string =
              "Close nodes for valid holder " + DebugId(nodes_[index].node_id()) + " are:\n";
        }
        matrix = nodes_[index].GetGroupMatrix();
        for (auto& element : matrix)
          matrix_string += DebugId(element) + ",";
        for (const auto& elem : *nodes_[index].close_nodes)
          close_nodes_string += DebugId(elem) + ", ";
        LOG(kInfo) << matrix_string;
        LOG(kInfo) << close_nodes_string;
      }
    }
  }
  return true;
}

void Network::PrintNetworkInfo() {
  size_t max_close_nodes_size(0), min_close_nodes_size(6400), max_accounts_size(0),
      min_matrix_size(100), max_matrix_size(0), avg_matrix_size(0);
  std::vector<size_t> group_matrix_miss;
  std::vector<NodeId> matrix;
  std::vector<RTNode> rt_nodes(nodes_);

  for (const auto& node : rt_nodes) {
    matrix = node.GetGroupMatrix();
    LOG(kInfo) << "Size of matrix for: " << DebugId(node.node_id()) << " is " << matrix.size();
    min_matrix_size = std::min(min_matrix_size, matrix.size());
    max_matrix_size = std::max(max_matrix_size, matrix.size());
    avg_matrix_size += matrix.size();
    LOG(kInfo) << DebugId(node.node_id()) << ", closests: " << node.close_nodes->size()
               << ", accounts: " << node.accounts.size();
    max_close_nodes_size = std::max(max_close_nodes_size, node.close_nodes->size());
    min_close_nodes_size = std::min(min_close_nodes_size, node.close_nodes->size());
    max_accounts_size = std::max(max_accounts_size, node.accounts.size());
  }
  group_matrix_miss = CheckGroupMatrixReliablity();
  LOG(kInfo) << "Maximum close nodes size: " << max_close_nodes_size;
  LOG(kInfo) << "Minimum close nodes size: " << min_close_nodes_size;
  LOG(kInfo) << "Maximum account size: " << max_accounts_size;
  LOG(kInfo) << "Maximum matrix size: " << max_matrix_size;
  LOG(kInfo) << "Minimum matrix size: " << min_matrix_size;
  LOG(kInfo) << "Average matrix size: " << avg_matrix_size / nodes_.size();
  for (size_t index(0); index < 4; ++index) {
    LOG(kInfo) << "Number of times matrix missing required holders for existing accounts on "
               << index << "th closest node: " << group_matrix_miss.at(index);
  }
  LOG(kInfo) << "Number of accounts in the network " << accounts_.size();
}

std::vector<size_t> Network::CheckGroupMatrixReliablity() {
  std::vector<size_t> little_matrix(4, 0);
  std::vector<NodeId> matrix;
  for (const auto& account : accounts_) {
    PartialSortFromTarget(account, 5);
    for (size_t node_index(0); node_index < 4; ++node_index) {
      matrix = nodes_[node_index].GetGroupMatrix();
      for (size_t index(0); index < std::min(size_t(4), matrix.size()); ++index) {
        if (index == node_index)
          continue;
        if (std::find(matrix.begin(), matrix.end(), nodes_[index].node_id()) == matrix.end()) {
          LOG(kInfo) << "Matrix of " << DebugId(nodes_[node_index].node_id()) << " does not have "
                     << DebugId(nodes_[index].node_id()) << " as a holder of account "
                     << DebugId(account);
          ++little_matrix[index];
        }
      }
    }
  }
  return little_matrix;
}

void Network::CheckReliability(const NodeId& target,
                               std::vector<size_t>& close_nodes_results,
                               std::vector<size_t>& matrix_results) {
  PartialSortFromTarget(target, 5);
  for (size_t node_index(0); node_index < 4; ++node_index) {
    auto close_nodes(*nodes_[node_index].close_nodes);
    auto matrix(nodes_[node_index].GetGroupMatrix());
    for (size_t index(0); index < std::min(size_t(4), close_nodes.size()); ++index) {
      if (index == node_index)
        continue;
      if (std::find(std::begin(close_nodes), std::end(close_nodes),
                    nodes_[index].node_id()) == std::end(close_nodes)) {
        LOG(kInfo) << "Close nodes of " << DebugId(nodes_[node_index].node_id())
                   << " do not have " << DebugId(nodes_[index].node_id())
                   << " as a holder of account " << DebugId(target);
        ++close_nodes_results[index];
      }
      if (std::find(matrix.begin(), matrix.end(), nodes_[index].node_id()) == matrix.end()) {
        LOG(kInfo) << "Matrix of " << DebugId(nodes_[node_index].node_id()) << " does not have "
                   << DebugId(nodes_[index].node_id()) << " as a holder of account "
                   << DebugId(target);
        ++matrix_results[index];
      }
    }
  }
}

TEST(RoutingTableTest, FUNC_GroupMatrixReliability) {
  Network network;
  for (auto i(0); i != 100; ++i) {
    LOG(kSuccess) << "Iteration # " << i << "  ===================================================";
    network.Add(NodeId(NodeId::kRandomId));
    for (auto i(0); i != 10; ++i)
      network.AddAccount(NodeId(NodeId::kRandomId));
  }
  network.PrintNetworkInfo();
  network.PruneAllAccounts();
  if (network.Validate())
    LOG(kSuccess) << "Validated.";
  else
    LOG(kError) << "Failed";
}

TEST(RoutingTableTest, FUNC_FindCloseNodes) {
  Network network;
  for (auto i(0); i != 100; ++i) {
    LOG(kSuccess) << "Iteration # " << i << "  ===================================================";
    network.RoutingAdd(NodeId(NodeId::kRandomId));
  }
}

TEST(RoutingTableTest, FUNC_RoutingTableVersusGroupMatrixReliability) {
  std::vector<size_t> matrix_results(4, 0), close_nodes_results(4, 0);
  Network network;
  for (auto i(0); i != 500; ++i) {
    LOG(kSuccess) << "Iteration # " << i << "  ===================================================";
    network.Add(NodeId(NodeId::kRandomId));
  }
  for (auto i(0); i != 1000; ++i)
    network.CheckReliability(NodeId(NodeId::kRandomId), close_nodes_results, matrix_results);

  for (auto index(0); index < 4; ++index) {
    LOG(kSuccess) << "Number of times matrix missing required holders for existing accounts on "
               << index << "th closest node: " << matrix_results.at(index);
    LOG(kSuccess) << "Number of times close nodes missing required holders "
                 << "for existing accounts on " << index << "th closest node: "
                 << close_nodes_results.at(index);
  }
}

}  // namespace test
}  // namespace routing
}  // namespace maidsafe
