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
  explicit RTNode(const NodeId& node_id)
      : kNodeId(node_id),
        close_nodes([node_id](const NodeId& id1, const NodeId& id2) {
                       return NodeId::CloserToTarget(id1, id2, node_id);
                    }),
        accounts(),
        group_matrix() {}
  RTNode& operator=(RTNode&& other) {
    const_cast<NodeId&>(kNodeId) = std::move(other.kNodeId);
    close_nodes = std::move(other.close_nodes);
    accounts = std::move(other.accounts);
    group_matrix = std::move(other.group_matrix);
    return *this;
  }
  const NodeId kNodeId;
  std::set<NodeId, std::function<bool(const NodeId&, const NodeId&)>> close_nodes;
  std::vector<NodeId> accounts;
  std::vector<NodeId> group_matrix;
};

class Network {
 public:
  Network() {
    out_file.open("log.txt");
  }

  void Add(const NodeId& node_id);
  void AddAccount(const NodeId& account);

  void PruneNetwork();
  void PruneAccounts(const NodeId& node_id);
  bool RemovePeer(const NodeId& node_id, const NodeId& requester);
  RTNode MakeNode(const NodeId& node_id);
  void UpdateNetwork(RTNode& new_node);
  void UpdateAccounts(RTNode& new_node);
  bool IsResponsibleForAccount(const RTNode& node, const NodeId& account);
  bool IsResponsibleForAccountMatrix(const RTNode& node, const NodeId& account,
                                     std::vector<NodeId>& node_ids);
  bool Validate();
  uint16_t PartialSortFromTarget(const NodeId& target, uint16_t number);
  uint16_t PartialSortFromTarget(const NodeId& target, uint16_t number,
                                          std::vector<RTNode>& nodes);
  void RemoveAccount(const RTNode& node, const NodeId& account);
  void PrintNetworkInfo();
  std::vector<size_t> CheckGroupMatrixReliablity();
  void TransferAndDeleteAccountFromInformedNodes(
      const NodeId& informed_node_id,
      const NodeId& new_node_id,
      std::vector<NodeId>& informed_node_group_matrix);
  void IdealUpdateAccounts(RTNode& new_node);
  void IdealRemoveAccount(const NodeId& account);
  void TransferAccount(const NodeId& new_node_id, const NodeId& account);
  void DeleteAccount(const NodeId& node_id, const NodeId& account);
  void UpdateAccountsOfNewAndInformedNodes(const RTNode& new_node);
  std::vector<NodeId> GetGroupMatrix(const NodeId& node_id);

  std::fstream out_file;
  std::vector<RTNode> nodes_;
  std::vector<NodeId> accounts_;
};

void Network::Add(const NodeId& node_id) {
  auto node = MakeNode(node_id);
  UpdateNetwork(node);
  nodes_.push_back(node);
  PruneNetwork();
  LOG(kInfo) << "Added NodeId : " << DebugId(node_id);
  // add/remove accounts to new node from nodes who noticed new node
  UpdateAccountsOfNewAndInformedNodes(node);
}

RTNode Network::MakeNode(const NodeId &node_id) {
  PartialSortFromTarget(node_id, 8);
  RTNode node(node_id);
  std::string message(DebugId(node.kNodeId) + " added");
  for (size_t i = 0; (i < 8) && i < nodes_.size(); ++i) {
    node.close_nodes.insert(nodes_[i].kNodeId);
    message += "   " + DebugId(nodes_[i].kNodeId);
    nodes_[i].close_nodes.insert(node.kNodeId);
  }
  LOG(kInfo) << message;
  return node;
}

void Network::UpdateNetwork(RTNode& new_node) {
  for (size_t index(8); index < nodes_.size(); ++index) {
    const NodeId& node_id = nodes_[index].kNodeId;
    auto eighth_closest(std::begin(nodes_[index].close_nodes));
    std::advance(eighth_closest, 7);
    if (NodeId::CloserToTarget(new_node.kNodeId, *eighth_closest, node_id)) {
      LOG(kVerbose) << DebugId(node_id) << " network added " << DebugId(new_node.kNodeId);
      new_node.close_nodes.insert(node_id);
      nodes_[index].close_nodes.insert(new_node.kNodeId);
    }
  }
}

void Network::UpdateAccountsOfNewAndInformedNodes(const RTNode& new_node) {
  // find nodes who noticed new node
  for (const auto& node : nodes_) {
    auto informed_node_group_matrix(GetGroupMatrix(node.kNodeId));

    if (std::find(informed_node_group_matrix.begin(),
                  informed_node_group_matrix.end(),
                  new_node.kNodeId) != informed_node_group_matrix.end()) {
      // transfer account
      if (node.kNodeId != new_node.kNodeId)
        TransferAndDeleteAccountFromInformedNodes(node.kNodeId, new_node.kNodeId,
                                                  informed_node_group_matrix);
    }
  }
}

void Network::TransferAndDeleteAccountFromInformedNodes(
    const NodeId& informed_node_id,
    const NodeId& new_node_id,
    std::vector<NodeId>& informed_node_group_matrix) {
// find informed node
  auto informed_node = std::find_if(nodes_.begin(), nodes_.end(),
                                    [&informed_node_id](const RTNode& rt_node) {
                                        return (rt_node.kNodeId == informed_node_id);
                                    });
  assert(informed_node != nodes_.end());

  std::vector<NodeId> accounts_to_delete;
// Sort matrix per account
  for (const auto& account : informed_node->accounts) {
    std::sort(informed_node_group_matrix.begin(), informed_node_group_matrix.end(),
              [&account](const NodeId& lhs, const NodeId& rhs) {
                return NodeId::CloserToTarget(lhs, rhs, account);
              });
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
      LOG(kInfo) << "DeleteAccount needed for " << DebugId(informed_node_id)
                 << ",  account " << DebugId(account) << "  Holders : \n" << holders_string;
    }
  }

  for (const auto& account : accounts_to_delete) {
    DeleteAccount(informed_node_id, account);
  }
}

void Network::TransferAccount(const NodeId& new_node_id, const NodeId& account) {
  LOG(kInfo) << "TransferAccount called for " << DebugId(new_node_id) << " for account : "
             << DebugId(account);
  auto new_node(std::find_if(nodes_.begin(), nodes_.end(), [&new_node_id](const RTNode& rt_node) {
                                               return (rt_node.kNodeId == new_node_id);
                                            }));
  assert(new_node != nodes_.end());
  if (std::find(new_node->accounts.begin(), new_node->accounts.end(), account)
      == new_node->accounts.end()) {
    new_node->accounts.push_back(account);
  }
}

void Network::DeleteAccount(const NodeId& node_id, const NodeId& account) {
  auto node(std::find_if(nodes_.begin(), nodes_.end(), [&node_id](const RTNode& rt_node) {
                                               return (rt_node.kNodeId == node_id);
                                            }));
  assert(node != nodes_.end());
  node->accounts.erase(std::remove(node->accounts.begin(), node->accounts.end(), account));
}

std::vector<NodeId> Network::GetGroupMatrix(const NodeId& node_id) {
  std::set<NodeId> return_set;
  auto node(std::find_if(nodes_.begin(),
                         nodes_.end(),
                         [&node_id] (const RTNode& rt_node) {
                           return rt_node.kNodeId == node_id;
                         }));
  for (const auto& close_node : node->close_nodes)
    return_set.insert(close_node);

  std::set<NodeId> tmp_set;
  for (const auto& close_node : return_set) {
    auto tmp_node(std::find_if(nodes_.begin(),
                               nodes_.end(),
                               [&close_node] (const RTNode& rt_node) {
                                 return rt_node.kNodeId == close_node;
                               }));
    tmp_set.insert(tmp_node->close_nodes.begin(), tmp_node->close_nodes.end());
  }
  return_set.insert(tmp_set.begin(), tmp_set.end());
  return_set.insert(node_id);
  std::vector<NodeId> return_nodes;
  for (const auto& set_node : return_set)
    return_nodes.push_back(set_node);
  return return_nodes;
}

void Network::AddAccount(const NodeId& account) {
  uint16_t count(PartialSortFromTarget(account, 4));
  for (uint16_t index(0); index != count; ++index) {
    nodes_[index].accounts.push_back(account);
    LOG(kInfo) << "Adding account < " << DebugId(account) << " > to Node : "
               << DebugId(nodes_[index].kNodeId);
  }
  accounts_.push_back(account);
}

void Network::PruneNetwork() {
  for (auto& node : nodes_) {
    if (node.close_nodes.size() <= 8)
      continue;
    auto itr(std::begin(node.close_nodes));
    std::advance(itr, 8);
    while (itr != std::end(node.close_nodes))  {
      if (RemovePeer(*itr, node.kNodeId)) {
        LOG(kInfo) << DebugId(*itr) << " and " << DebugId(node.kNodeId) << " removed each other";
        itr = node.close_nodes.erase(itr);
      } else {
        ++itr;
      }
    }
  }
}

bool Network::RemovePeer(const NodeId& node_id, const NodeId& requester) {
  auto node(std::find_if(nodes_.begin(),
                         nodes_.end(),
                         [&node_id] (const RTNode& rt_node) {
                           return (rt_node.kNodeId == node_id);
                         }));
  auto peer_itr(node->close_nodes.find(requester));
  if (std::distance(node->close_nodes.begin(), peer_itr) > 8) {
    LOG(kVerbose) << DebugId(node->kNodeId) << " removes peer " << DebugId(*peer_itr);
    node->close_nodes.erase(peer_itr);
    return true;
  }
  return false;
}

uint16_t Network::PartialSortFromTarget(const NodeId& target, uint16_t number) {
  return PartialSortFromTarget(target, number, nodes_);
}

uint16_t Network::PartialSortFromTarget(const NodeId& target,
                                        uint16_t number,
                                        std::vector<RTNode>& nodes) {
  uint16_t count = std::min(number, static_cast<uint16_t>(nodes.size()));
  std::partial_sort(nodes.begin(),
                    nodes.begin() + count,
                    nodes.end(),
                    [&target](const RTNode& lhs, const RTNode& rhs) {
                      return NodeId::CloserToTarget(lhs.kNodeId, rhs.kNodeId, target);
                    });
  return count;
}

bool Network::Validate() {
  size_t index(0);
  for (const auto& account : accounts_) {
    ++index;
    size_t count(std::count_if(nodes_.begin(),
                               nodes_.end(),
                               [&account] (const RTNode& node) {
                                 return std::find(node.accounts.begin(),
                                                  node.accounts.end(),
                                                  account) != node.accounts.end();
                  }));
    if (count > 4) {
      LOG(kError) << "Account " << DebugId(account) << " # of holders: " << count
                  << "  index: " << index;
    }
    std::sort(nodes_.begin(),
              nodes_.end(),
              [&account](const RTNode& lhs, const RTNode& rhs) {
                return NodeId::CloserToTarget(lhs.kNodeId, rhs.kNodeId, account);
              });
    if (count == 5)
    LOG(kError) << DebugId(nodes_.at(4).kNodeId) <<  " Wrong holder of : " << DebugId(account);
    for (auto itr(nodes_.begin()); itr != nodes_.begin() + 4; ++itr) {
      EXPECT_NE(std::find(itr->accounts.begin(), itr->accounts.end(), account),
                itr->accounts.end()) << "Node: " << DebugId(itr->kNodeId)
                                     << " does not have " << DebugId(account);
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
    matrix = GetGroupMatrix(node.kNodeId);
    LOG(kInfo) << "Size of matrix for: " << DebugId(node.kNodeId) << " is " << matrix.size();
    min_matrix_size = std::min(min_matrix_size, matrix.size());
    max_matrix_size = std::max(max_matrix_size, matrix.size());
    avg_matrix_size += matrix.size();
    LOG(kInfo) <<  DebugId(node.kNodeId)
                << ", closests: " << node.close_nodes.size()
                << ", accounts: " << node.accounts.size();
    max_close_nodes_size = std::max(max_close_nodes_size, node.close_nodes.size());
    min_close_nodes_size = std::min(min_close_nodes_size, node.close_nodes.size());
    max_accounts_size = std::max(max_accounts_size, node.accounts.size());
  }
  group_matrix_miss = CheckGroupMatrixReliablity();
  LOG(kInfo) <<  "Maximum close nodes size: " <<  max_close_nodes_size;
  LOG(kInfo) <<  "Minimum close nodes size: " <<  min_close_nodes_size;
  LOG(kInfo) <<  "Maximum account size: " <<  max_accounts_size;
  LOG(kInfo) <<  "Maximum matrix size: " <<  max_matrix_size;
  LOG(kInfo) <<  "Minimum matrix size: " <<  min_matrix_size;
  LOG(kInfo) <<  "Average matrix size: " <<  avg_matrix_size / nodes_.size();
  for (size_t index(0); index < 4; ++index) {
    LOG(kInfo) <<  "Number of times matrix missing required holders for existing accounts on "
               << index << "th closest node: " << group_matrix_miss.at(index);
  }
  LOG(kInfo) <<  "Number of accounts in the network " << accounts_.size();
}

std::vector<size_t> Network::CheckGroupMatrixReliablity() {
  std::vector<size_t> little_matrix(4, 0);
  std::vector<NodeId> matrix;
  for (const auto& account : accounts_) {
    PartialSortFromTarget(account, 5);
    for (size_t node_index(0); node_index < 4; ++node_index) {
      matrix = GetGroupMatrix(nodes_[node_index].kNodeId);
      for (size_t index(0); index < 4; ++index) {
        if (index == node_index)
          continue;
        if (std::find(matrix.begin(),
                      matrix.end(),
                      nodes_[index].kNodeId) == matrix.end()) {
          LOG(kInfo) << "Matrix of " << DebugId(nodes_[node_index].kNodeId) << " does not have "
                     << DebugId(nodes_[index].kNodeId) << " as a holder of account "
                     << DebugId(account);
          ++little_matrix[index];
        }
      }
    }
  }
  return little_matrix;
}

TEST(RoutingTableTest, BEH_RT) {
  Network network;
  for (auto i(0); i != 2500; ++i) {
    LOG(kSuccess) << "Iteration # " << i << "  ===================================================";
    network.Add(NodeId(NodeId::kRandomId));
//    if (i % 5 == 0)
      network.AddAccount(NodeId(NodeId::kRandomId));
  }
  network.PrintNetworkInfo();
  if (network.Validate())
    LOG(kSuccess) << "Validated.";
  else
    LOG(kError) << "Failed";
}

}  // namespace test
}  // namespace routing
}  // namespace maidsafe
