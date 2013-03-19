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
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {
namespace routing {
namespace test {

struct RTNode {
  NodeId node_id;
  std::vector<NodeId> close_nodes;
  std::vector<NodeId> accounts;
};

class Network {
 public:
  void Add(const NodeId& node_id);
  void AddAccount(const NodeId& account);

  void PruneNetwork();
  bool RemovePeer(const NodeId& node_id, const NodeId& requester);
  RTNode MakeNode(const NodeId& node_id);
  void UpdateNetwork(const RTNode& new_node);
  void UpdateAccounts(RTNode& new_node);
  bool IsResponsibleForAccount(const RTNode& node, const NodeId& account);
  bool Validate();
  uint16_t PartialSortFromTarget(const NodeId& target, uint16_t number);
  void RemoveAccount(const RTNode& node, const NodeId& account);
  void PrintNetworkInfo();

  void IdealUpdateAccounts(RTNode& new_node);
  void IdealRemoveAccount(const NodeId& account);


  std::vector<RTNode> nodes_;
  std::vector<NodeId> accounts_;
};

void Network::Add(const NodeId& node_id) {
  auto node = MakeNode(node_id);
  UpdateNetwork(node);
  IdealUpdateAccounts(node);
  nodes_.push_back(node);
  PruneNetwork();
  LOG(kInfo) << "Added NodeId : " << DebugId(node_id);
}

bool Network::Validate() {
  for (auto& account : accounts_) {
    size_t count(std::count_if(nodes_.begin(),
                               nodes_.end(),
                               [account] (const RTNode& node) {
                                 return std::find(node.accounts.begin(),
                                                  node.accounts.end(),
                                                  account) != node.accounts.end();
                  }));
    LOG(kInfo) << "Account " << DebugId(account) << " # of holders: " << count;
  }
  return true;
}

void Network::AddAccount(const NodeId& account) {
  uint16_t count(PartialSortFromTarget(account, 4));
  for (uint16_t index(0); index != count; ++index) {
    nodes_.at(index).accounts.push_back(account);
    LOG(kInfo) << "Added AccountId : " << DebugId(account);
  }
  accounts_.push_back(account);
}

void Network::PruneNetwork() {
  for (auto& node : nodes_) {
    if (node.close_nodes.size() <= 8)
      continue;
    std::sort(node.close_nodes.begin(),
              node.close_nodes.end(),
              [node](const NodeId& lhs, const NodeId& rhs) {
                return NodeId::CloserToTarget(lhs, rhs, node.node_id);
              });
    for (size_t index(8); index < node.close_nodes.size(); ++index) {
      if (RemovePeer(node.close_nodes[index], node.node_id)) {
          node.close_nodes.erase(std::remove(node.close_nodes.begin(),
                                             node.close_nodes.end(),
                                             node.close_nodes[index]), node.close_nodes.end());
          LOG(kInfo) << DebugId(node.close_nodes[index])
                     << " and " << DebugId(node.node_id) << " removed each other ";
      }
    }
  }
}

bool Network::RemovePeer(const NodeId& node_id, const NodeId& requester) {
  auto node(std::find_if(nodes_.begin(),
                         nodes_.end(),
                         [node_id] (const RTNode& rt_node) {
                           return (rt_node.node_id == node_id);
                         }));
  std::sort(node->close_nodes.begin(),
            node->close_nodes.end(),
            [node](const NodeId& lhs, const NodeId& rhs) {
              return NodeId::CloserToTarget(lhs, rhs, node->node_id);
            });
  auto peer_itr(std::find(node->close_nodes.begin(),
                          node->close_nodes.end(),
                          requester));
  if (std::distance(node->close_nodes.begin(), peer_itr) > 8) {
    node->close_nodes.erase(peer_itr);
    return true;
  }
  return false;
}

RTNode Network::MakeNode(const NodeId &node_id) {
  PartialSortFromTarget(node_id, 8);
  RTNode node;
  node.node_id = node_id;
  for (size_t i = 0; (i < 8) && i < nodes_.size(); ++i)
    node.close_nodes.push_back(nodes_[i].node_id);
  return node;
}

void Network::UpdateNetwork(const RTNode& new_node) {
  for (size_t i = 0; (i < 8) && i < nodes_.size(); ++i) {
    nodes_[i].close_nodes.push_back(new_node.node_id);
  }
}



void Network::UpdateAccounts(RTNode& new_node) {
  std::vector<NodeId> accounts;
  std::vector<RTNode> nodes;
  for (size_t i = 0; (i < 8) && i < nodes_.size(); ++i) {
    nodes.push_back(nodes_[i]);
  }
  for (auto& node : nodes) {
    accounts = node.accounts;
    for (auto& account : accounts) {
      if (IsResponsibleForAccount(new_node, account)) {
         if (std::find(new_node.accounts.begin(),
                  new_node.accounts.end(),
                  account) == new_node.accounts.end())
            new_node.accounts.push_back(account);
          RemoveAccount(node, account);
      }
    }
  }
}

void Network::IdealUpdateAccounts(RTNode& new_node) {
  std::vector<NodeId> accounts;
  std::vector<RTNode> nodes;
  for (size_t i = 0; (i < 8) && i < nodes_.size(); ++i) {
    nodes.push_back(nodes_[i]);
  }
  for (auto& node : nodes) {
    accounts = node.accounts;
    for (auto& account : accounts) {
      if (IsResponsibleForAccount(new_node, account)) {
         if (std::find(new_node.accounts.begin(),
                  new_node.accounts.end(),
                  account) == new_node.accounts.end())
            new_node.accounts.push_back(account);
          IdealRemoveAccount(account);
      }
    }
  }
}

void Network::IdealRemoveAccount(const NodeId& account) {
  if (nodes_.size() > 3)
    nodes_[3].accounts.erase(std::remove_if(nodes_[3].accounts.begin(),
                                          nodes_[3].accounts.end(),
                                          [account] (const NodeId& node_id) {
                                            return (node_id == account);
                                          }), nodes_[3].accounts.end());
}


bool Network::IsResponsibleForAccount(const RTNode& node, const NodeId& account) {
  uint16_t count(PartialSortFromTarget(account, 4));
  if (count < 4)
    return true;
  return NodeId::CloserToTarget(node.node_id, nodes_[3].node_id, account);
}

void Network::RemoveAccount(const RTNode& node, const NodeId& account) {
  auto holder(std::find_if(nodes_.begin(),
                           nodes_.end(),
                           [node] (const RTNode& rt_node) {
                             return (rt_node.node_id == node.node_id);
                           }));
  assert(holder != nodes_.end());

  if (nodes_.size() > 3 && nodes_[3].node_id == holder->node_id)
    holder->accounts.erase(std::remove_if(holder->accounts.begin(),
                                        holder->accounts.end(),
                                        [account] (const NodeId& node_id) {
                                          return (node_id == account);
                                        }), holder->accounts.end());
}

uint16_t Network::PartialSortFromTarget(const NodeId& target, uint16_t number) {
  uint16_t count = std::min(number, static_cast<uint16_t>(nodes_.size()));
  std::partial_sort(nodes_.begin(),
                    nodes_.begin() + count,
                    nodes_.end(),
                    [target](const RTNode& lhs, const RTNode& rhs) {
                      return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target);
                    });
  return count;
}

void Network::PrintNetworkInfo() {
  size_t max_close_nodes_size(0), min_close_nodes_size(6400), max_accounts_size(0);
  for (auto& node : nodes_) {
    LOG(kInfo) <<  DebugId(node.node_id)
                << ", closests: " << node.close_nodes.size()
                << ", accounts: " << node.accounts.size();
    max_close_nodes_size = std::max(max_close_nodes_size, node.close_nodes.size());
    min_close_nodes_size = std::min(min_close_nodes_size, node.close_nodes.size());
    max_accounts_size = std::max(max_accounts_size, node.accounts.size());
  }
  LOG(kInfo) <<  "Maximum close nodes size: " <<  max_close_nodes_size;
  LOG(kInfo) <<  "Minimum close nodes size: " <<  min_close_nodes_size;
  LOG(kInfo) <<  "Maximum account size: " <<  max_accounts_size;
}

TEST(RoutingTableTest, BEH_RT) {
  Network network;
  for (auto i(0); i != 100; ++i) {
    network.Add(NodeId(NodeId::kRandomId));
    if (i % 2 == 0)
      network.AddAccount(NodeId(NodeId::kRandomId));
    LOG(kInfo) << "Iteration # " << i;
  }
  network.PrintNetworkInfo();
  network.Validate();
}

}  // namespace test
}  // namespace routing
}  // namespace maidsafe
