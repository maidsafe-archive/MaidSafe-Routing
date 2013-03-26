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
//  std::vector<NodeId> GetMatrix(const NodeId& node_id);
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

//void Network::Add(const NodeId& node_id) {
//  auto node = MakeNode(node_id);
//  UpdateNetwork(node);
//  UpdateAccounts(node);
//  nodes_.push_back(node);
//  PruneNetwork();
//  PruneAccounts(node.node_id);
//  LOG(kInfo) << "Added NodeId : " << DebugId(node_id);
//}

void Network::Add(const NodeId& node_id) {
  auto node = MakeNode(node_id);
  UpdateNetwork(node);
  nodes_.push_back(node);
  PruneNetwork();
  LOG(kInfo) << "Added NodeId : " << DebugId(node_id);
  UpdateAccountsOfNewAndInformedNodes(node); // add/remove accounts to new node from nodes who noticed new node
}

RTNode Network::MakeNode(const NodeId &node_id) {
  PartialSortFromTarget(node_id, 8);
  RTNode node;
  node.node_id = node_id;
  for (size_t i = 0; (i < 8) && i < nodes_.size(); ++i) {
    node.close_nodes.push_back(nodes_[i].node_id);
    LOG(kInfo) << DebugId(node.node_id) << " added " << DebugId(nodes_[i].node_id);
  }
  return node;
}

void Network::UpdateNetwork(RTNode& new_node) {
  PartialSortFromTarget(new_node.node_id, 8);
  for (size_t i = 0; (i < 8) && i < nodes_.size(); ++i) {
    nodes_[i].close_nodes.push_back(new_node.node_id);
    LOG(kVerbose) << DebugId(nodes_[i].node_id) << " network added " << DebugId(new_node.node_id);
  }

  NodeId node_id;
  for (size_t index(8); index < nodes_.size(); ++index) {
    node_id = nodes_[index].node_id;
    std::sort(nodes_[index].close_nodes.begin(),
              nodes_[index].close_nodes.end(),
              [node_id](const NodeId& lhs, const NodeId& rhs) {
                return NodeId::CloserToTarget(lhs, rhs, node_id);
              });
    if (NodeId::CloserToTarget(new_node.node_id,
                               nodes_[index].close_nodes[7],
                               nodes_[index].node_id)) {
      LOG(kVerbose) << DebugId(nodes_[index].node_id)
                    << " network added "
                    << DebugId(new_node.node_id);
      new_node.close_nodes.push_back(nodes_[index].node_id);
      nodes_[index].close_nodes.push_back(new_node.node_id);
    }
  }
}

void Network::UpdateAccountsOfNewAndInformedNodes(const RTNode& new_node) {
  // find nodes who noticed new node
  for (auto node : nodes_) {
    auto informed_node_group_matrix(GetGroupMatrix(node.node_id));

    if(std::find(informed_node_group_matrix.begin(),
                 informed_node_group_matrix.end(),
                 new_node.node_id) != informed_node_group_matrix.end()) {
      // transfer account
      if (node.node_id != new_node.node_id)
        TransferAndDeleteAccountFromInformedNodes(node.node_id, new_node.node_id, informed_node_group_matrix);
    }
  }
}

void Network::TransferAndDeleteAccountFromInformedNodes(
    const NodeId& informed_node_id,
    const NodeId& new_node_id,
    std::vector<NodeId>& informed_node_group_matrix) {
// find informed node
  auto informed_node = std::find_if(nodes_.begin(), nodes_.end(),
                                  [informed_node_id](const RTNode& rt_node) {
                                    return (rt_node.node_id == informed_node_id);
                                  });
  assert(informed_node != nodes_.end());

  std::vector<NodeId> accounts_to_delete;
// Sort matrix per account
  for (auto& account : informed_node->accounts) {
    std::sort(informed_node_group_matrix.begin(), informed_node_group_matrix.end(),
             [account](const NodeId& lhs, const NodeId& rhs) {
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
      for (auto i : closest_holders)
        holders_string = holders_string + "  " + DebugId(i) + "  ";
      accounts_to_delete.push_back(account);
      LOG(kInfo) << "DeleteAccount needed for " << DebugId(informed_node_id)
                 << ",  account " << DebugId(account) << "Holders : \n" << holders_string;
    }
  }

  for (auto account : accounts_to_delete) {
    DeleteAccount(informed_node_id, account);
  }
}

void Network::TransferAccount(const NodeId& new_node_id, const NodeId& account) {
  LOG(kInfo) << "TransferAccount called for " << DebugId(new_node_id) << "for account : "
             << DebugId(account);
  auto new_node(std::find_if(nodes_.begin(), nodes_.end(), [new_node_id](const RTNode& rt_node) {
                                               return (rt_node.node_id == new_node_id);
                                            }));
  assert(new_node != nodes_.end());
  if (std::find(new_node->accounts.begin(), new_node->accounts.end(), account)
      == new_node->accounts.end()) {
    new_node->accounts.push_back(account);
  }
}

void Network::DeleteAccount(const NodeId& node_id, const NodeId& account) {
  auto node(std::find_if(nodes_.begin(), nodes_.end(), [node_id](const RTNode& rt_node) {
                                               return (rt_node.node_id == node_id);
                                            }));
  assert(node != nodes_.end());
  node->accounts.erase(std::remove(node->accounts.begin(), node->accounts.end(), account));
}

std::vector<NodeId> Network::GetGroupMatrix(const NodeId& node_id) {

  std::set<NodeId> return_set;

  auto node(std::find_if(nodes_.begin(),
                         nodes_.end(),
                         [node_id] (const RTNode& rt_node) {
                           return rt_node.node_id == node_id;
                         }));
  for (auto& close_node : node->close_nodes)
    return_set.insert(close_node);

  std::set<NodeId> tmp_set;
  for (auto& close_node : return_set) {
    auto tmp_node(std::find_if(nodes_.begin(),
                               nodes_.end(),
                               [close_node] (const RTNode& rt_node) {
                                 return rt_node.node_id == close_node;
                               }));
    tmp_set.insert(tmp_node->close_nodes.begin(), tmp_node->close_nodes.end());
  }
  return_set.insert(tmp_set.begin(), tmp_set.end());
  return_set.insert(node_id);
  std::vector<NodeId> return_nodes;
  for (auto& set_node : return_set)
    return_nodes.push_back(set_node);
  return return_nodes;
}


//std::vector<NodeId> Network::GetGroupMatrix(const NodeId& node_id) {
//  std::vector<NodeId> return_nodes;
//  std::vector<RTNode> nodes_copy = nodes_;
//  auto itr(std::find_if(nodes_copy.begin(), nodes_copy.end(), [node_id](const RTNode& rt_node) {
//                                                         return (rt_node.node_id == node_id);
//                                                      }));
//  assert(itr != nodes_copy.end());
//  RTNode node = *itr;
//  std::sort(node.close_nodes.begin(), node.close_nodes.end(),
//            [node](const NodeId& lhs, const NodeId& rhs) {
//              return NodeId::CloserToTarget(lhs, rhs, node.node_id);
//            });
//  auto count = std::min(size_t(8), node.close_nodes.size());
//  for (auto i(0U); i != count; ++i) {
//    return_nodes.push_back(node.close_nodes.at(i));
//  }

//  auto return_vector_copy(return_nodes);
//  for (auto i: return_vector_copy) {
//    PartialSortFromTarget(i, 9, nodes_copy);
//    for (size_t i(1); (i < 9) && i < nodes_copy.size(); ++i) {
//      return_nodes.push_back(nodes_copy.at(i).node_id);
//    }
//  }

//  std::set<NodeId> return_set;
//  for (auto i : return_nodes)
//    return_set.insert(i);
//  return_set.insert(node_id);

//  return_nodes.clear();
//  for (auto i : return_set)
//    return_nodes.push_back(i);
//  std::string str;
//  for (auto i : return_nodes)
//    str += " " + DebugId(i) + ", ";
////  LOG(kInfo) << "Group matrix of " << DebugId(node_id) << " size " <<  return_nodes.size()
////             << " elements : \n " << str;
//  return return_nodes;
//}

//void Network::UpdateAccounts(RTNode& new_node) {
//  std::vector<NodeId> node_ids;
//  std::vector<RTNode> rt_nodes;
//  size_t close_nodes_size;
//  PartialSortFromTarget(new_node.node_id, 8);
//  for (size_t i = 0; (i < 8) && i < nodes_.size(); ++i) {
//    if (std::find(node_ids.begin(), node_ids.end(), nodes_[i].node_id) == node_ids.end()) {
//      node_ids.push_back(nodes_[i].node_id);
//    }
//    close_nodes_size = std::min(size_t(8), nodes_[i].close_nodes.size());
//    for (auto itr(nodes_[i].close_nodes.begin()); itr != nodes_[i].close_nodes.begin() + close_nodes_size; ++itr) {
//      if (std::find(node_ids.begin(), node_ids.end(), *itr) == node_ids.end() &&
//          (*itr != new_node.node_id)) {
//        node_ids.push_back(*itr);
//      }
//    }
//  }

//  for (auto& node_id : node_ids) {
//    auto itr(std::find_if(nodes_.begin(),
//                          nodes_.end(),
//                          [node_id] (const RTNode& rt_node) {
//                            return rt_node.node_id == node_id;
//                          }));
//    assert(itr != nodes_.end());
//    rt_nodes.push_back(*itr);
//  }

//  std::vector<NodeId> accounts;
//  for (auto& node : rt_nodes) {
//    accounts = node.accounts;
//    for (auto& account : accounts) {
//      if (IsResponsibleForAccountMatrix(new_node, account, node_ids)) {
//         if (std::find(new_node.accounts.begin(),
//                       new_node.accounts.end(),
//                       account) == new_node.accounts.end()) {
//           new_node.accounts.push_back(account);
//           LOG(kInfo) << DebugId(new_node.node_id) << " << added account >>" << DebugId(account);
//         }
////         RemoveAccount(node, account);
//      }
//    }
//  }
//}


//bool Network::IsResponsibleForAccountMatrix(const RTNode& node, const NodeId& account,
//                                            std::vector<NodeId>& node_ids) {
//  if (node_ids.size() < 4)
//    return true;
//  std::sort(node_ids.begin(), node_ids.end(), [account](const NodeId& lhs, const NodeId& rhs) {
//                                                return NodeId::CloserToTarget(lhs, rhs, account);
//                                              });
//  return NodeId::CloserToTarget(node.node_id, node_ids[3], account);
//}

//bool Network::IsResponsibleForAccount(const RTNode& node, const NodeId& account) {
//  uint16_t count(PartialSortFromTarget(account, 4));
//  if (count < 4)
//    return true;
//  return NodeId::CloserToTarget(node.node_id, nodes_[3].node_id, account);
//}



void Network::AddAccount(const NodeId& account) {
  uint16_t count(PartialSortFromTarget(account, 4));
  for (uint16_t index(0); index != count; ++index) {
    nodes_.at(index).accounts.push_back(account);
    LOG(kInfo) << "Adding account < " << DebugId(account) << " > to Node : "
               << DebugId(nodes_.at(index).node_id);
  }
  accounts_.push_back(account);
}

//void Network::PruneAccounts(const NodeId& node_id) {
//  std::vector<NodeId> matrix, accounts, node_ids;
//  std::vector<RTNode> nodes;
//  std::string log;
//  node_ids = GetMatrix(node_id);

//  for (auto& node : nodes_) {
//    if (std::find(node_ids.begin(), node_ids.end(), node.node_id) == node_ids.end())
//      continue;
//    matrix = GetMatrix(node.node_id);
//    if (matrix.size() < 4) {
//      return;
//    }
////    if (nodes_.size() > 50 && matrix.size() <= 10) {
////      LOG(kInfo) <<   "Error nodes_.size() > 50 ";
////      log = DebugId(node.node_id);
////      log += ", " + std::to_string(matrix.size()) + "\n";
////      out_file << log;
////      continue;
////    }
//    accounts = node.accounts;
//    for (auto itr(accounts.begin()); itr != accounts.end(); ++itr) {
//      std::sort(matrix.begin(),
//                matrix.end(),
//                [itr] (const NodeId& lhs, const NodeId& rhs) {
//                  return NodeId::CloserToTarget(lhs, rhs, *itr);
//                });
//      if (NodeId::CloserToTarget(matrix[3], node.node_id, *itr)) {
//        auto remove(std::find(node.accounts.begin(),
//                              node.accounts.end(), *itr));
//        assert(remove != node.accounts.end());
//        node.accounts.erase(remove);
//      }
//    }
//  }
//}

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
    LOG(kInfo) << DebugId(node->node_id) << " removes peer " << DebugId(*peer_itr);
    node->close_nodes.erase(peer_itr);
    return true;
  }
  return false;
}

//void Network::IdealUpdateAccounts(RTNode& new_node) {
//  std::vector<NodeId> accounts;
//  std::vector<RTNode> nodes;
//  for (size_t i = 0; (i < 8) && i < nodes_.size(); ++i) {
//    nodes.push_back(nodes_[i]);
//  }
//  for (auto& node : nodes) {
//    accounts = node.accounts;
//    for (auto& account : accounts) {
//      if (IsResponsibleForAccount(new_node, account)) {
//         if (std::find(new_node.accounts.begin(),
//                  new_node.accounts.end(),
//                  account) == new_node.accounts.end())
//            new_node.accounts.push_back(account);
//          IdealRemoveAccount(account);
//      }
//    }
//  }
//}

//void Network::IdealRemoveAccount(const NodeId& account) {
//  if (nodes_.size() > 3)
//    nodes_[3].accounts.erase(std::remove_if(nodes_[3].accounts.begin(),
//                                          nodes_[3].accounts.end(),
//                                          [account] (const NodeId& node_id) {
//                                            return (node_id == account);
//                                          }), nodes_[3].accounts.end());
//}




//void Network::RemoveAccount(const RTNode& node, const NodeId& account) {
//  auto holder(std::find_if(nodes_.begin(),
//                           nodes_.end(),
//                           [node] (const RTNode& rt_node) {
//                             return (rt_node.node_id == node.node_id);
//                           }));
//  assert(holder != nodes_.end());
//  if (nodes_.size() > 3 && nodes_[3].node_id == holder->node_id) {
//    holder->accounts.erase(std::remove_if(holder->accounts.begin(),
//                                        holder->accounts.end(),
//                                        [account] (const NodeId& node_id) {
//                                          return (node_id == account);
//                                        }), holder->accounts.end());
//    LOG(kInfo) << DebugId(holder->node_id) << " removed account " << DebugId(account);
//  }
//}

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

uint16_t Network::PartialSortFromTarget(const NodeId& target, uint16_t number,
                                        std::vector<RTNode>& nodes) {
  uint16_t count = std::min(number, static_cast<uint16_t>(nodes.size()));
  std::partial_sort(nodes.begin(),
                    nodes.begin() + count,
                    nodes.end(),
                    [target](const RTNode& lhs, const RTNode& rhs) {
                      return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target);
                    });
  return count;
}

//std::vector<NodeId> Network::GetMatrix(const NodeId& node_id) {
//  auto nodes_copy = nodes_;

//  PartialSortFromTarget(node_id, 9, nodes_copy);
//  std::vector<NodeId> return_nodes;
//  NodeId local_id;
//  size_t size;
//  for (size_t i = 1; (i < 9) && i < nodes_copy.size(); ++i) {
//      if (std::find(return_nodes.begin(),
//                    return_nodes.end(),
//                    nodes_copy[i].node_id) == return_nodes.end()) {
//        return_nodes.push_back(nodes_copy[i].node_id);
//      }
//    local_id = (nodes_copy[i].node_id);
//    std::sort(nodes_copy[i].close_nodes.begin(),
//              nodes_copy[i].close_nodes.end(),
//              [local_id] (const NodeId& lhs, const NodeId& rhs) {
//                return NodeId::CloserToTarget(lhs, rhs, local_id);
//              });
//    size = std::min(size_t(8), nodes_copy[i].close_nodes.size());
//    for (auto itr(nodes_copy[i].close_nodes.begin());
//         itr != nodes_copy[i].close_nodes.begin() + size;
//         ++itr) {
////      if (*itr == node_id)
////        continue;
//      if (std::find(return_nodes.begin(), return_nodes.end(), *itr) == return_nodes.end()) {
//        return_nodes.push_back(*itr);
//      }
//    }
//  }

//  return_nodes.erase(std::remove(return_nodes.begin(), return_nodes.end(), node_id), return_nodes.end());
//  return return_nodes;
//}


bool Network::Validate() {
  size_t index(0);
  for (auto& account : accounts_) {
    index++;
    size_t count(std::count_if(nodes_.begin(),
                               nodes_.end(),
                               [account] (const RTNode& node) {
                                 return std::find(node.accounts.begin(),
                                                  node.accounts.end(),
                                                  account) != node.accounts.end();
                  }));
    if (count > 4)
      LOG(kInfo) << "Account " << DebugId(account) << " # of holders: " << count << " index: " << index;
    std::sort(nodes_.begin(),
              nodes_.end(),
              [account](const RTNode& lhs, const RTNode& rhs) {
                return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, account);
              });
    if (count == 5)
    LOG(kInfo) << DebugId(nodes_.at(4).node_id) <<  " Wrong holder of : " << DebugId(account);
    for (auto itr(nodes_.begin()); itr != nodes_.begin() + 4; ++itr) {
      EXPECT_NE(std::find(itr->accounts.begin(), itr->accounts.end(), account),
                itr->accounts.end()) << "Node: " << DebugId(itr->node_id)
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

  for (auto& node : rt_nodes) {
    matrix = GetGroupMatrix(node.node_id);
    LOG(kInfo) << "Size of matrix for: " << DebugId(node.node_id) << " is " << matrix.size();
    min_matrix_size = std::min(min_matrix_size, matrix.size());
    max_matrix_size = std::max(max_matrix_size, matrix.size());
    avg_matrix_size += matrix.size();
    LOG(kInfo) <<  DebugId(node.node_id)
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
               << index << "th closet node " << group_matrix_miss.at(index);
  }
  LOG(kInfo) <<  "Number of accounts in the network " << accounts_.size();
}

std::vector<size_t> Network::CheckGroupMatrixReliablity() {
  std::vector<size_t> little_matrix(4, 0);
  std::vector<NodeId> matrix;
  for (auto& account : accounts_) {
    std::partial_sort(nodes_.begin(),
                      nodes_.begin() + 5,
                      nodes_.end(),
                     [account] (const RTNode& lhs, const RTNode& rhs) {
                       return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, account);
                     });
    for (size_t node_index(0); node_index < 4; ++node_index) {
      matrix = GetGroupMatrix(nodes_[node_index].node_id);
      for (size_t index(0); index < 4; ++index) {
        if (index == node_index)
          continue;
        if (std::find(matrix.begin(),
                      matrix.end(),
                      nodes_[index].node_id) == matrix.end()) {
          LOG(kInfo) << "Matrix of " << DebugId(nodes_[0].node_id) << " does not have "
                     << DebugId(nodes_[index].node_id) << " as a holder of account "
                     << DebugId(account);
          little_matrix[index]++;
        }
      }
    }
  }
  return little_matrix;
}

TEST(RoutingTableTest, BEH_RT) {
  Network network;
  for (auto i(0); i != 2500; ++i) {
    LOG(kInfo) << "Iteration # " << i;
    network.Add(NodeId(NodeId::kRandomId));
    if (i % 5 == 0)
      network.AddAccount(NodeId(NodeId::kRandomId));
  }
  network.PrintNetworkInfo();
  network.Validate();
}

}  // namespace test
}  // namespace routing
}  // namespace maidsafe
