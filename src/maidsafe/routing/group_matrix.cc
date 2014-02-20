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

#include "maidsafe/routing/group_matrix.h"

#include <algorithm>
#include <bitset>
#include <cstdint>
#include <set>

#include "maidsafe/common/log.h"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

GroupMatrix::GroupMatrix(const NodeId& this_node_id, bool client_mode)
    : kNodeId_(this_node_id),
      unique_nodes_(),
      radius_(crypto::BigInt::Zero()),
      client_mode_(client_mode),
      matrix_() {
  UpdateUniqueNodeList();
}

std::shared_ptr<MatrixChange> GroupMatrix::AddConnectedPeer(
    const NodeInfo& node_info, const std::vector<NodeInfo>& matrix_update) {
  std::vector<NodeId> old_unique_ids(GetUniqueNodeIds());
  LOG(kVerbose) << DebugId(kNodeId_) << " AddConnectedPeer : " << DebugId(node_info.node_id);
  auto node_id(node_info.node_id);
  auto found(std::find_if(std::begin(matrix_), std::end(matrix_),
                          [node_id](const std::vector<NodeInfo>& info) {
                            return info.begin()->node_id == node_id;
                          }));
  if (found != std::end(matrix_)) {
    LOG(kWarning) << "Already Added in matrix";
    return std::make_shared<MatrixChange>(MatrixChange(kNodeId_, old_unique_ids, old_unique_ids));
  }

  std::vector<NodeInfo> nodes_info(std::vector<NodeInfo>(1, node_info));
  std::copy(std::begin(matrix_update), std::end(matrix_update), std::back_inserter(nodes_info));
  matrix_.push_back(nodes_info);
  Prune();
  UpdateUniqueNodeList();
  return std::make_shared<MatrixChange>(MatrixChange(kNodeId_, old_unique_ids, GetUniqueNodeIds()));
}

std::shared_ptr<MatrixChange> GroupMatrix::RemoveConnectedPeer(const NodeInfo& node_info) {
  std::vector<NodeId> old_unique_ids(GetUniqueNodeIds());
  matrix_.erase(std::remove_if(std::begin(matrix_), std::end(matrix_),
                               [node_info](const std::vector<NodeInfo>& nodes) {
                                 return (node_info.node_id == nodes.begin()->node_id);
                               }),
                std::end(matrix_));
  Prune();
  UpdateUniqueNodeList();
  return std::make_shared<MatrixChange>(MatrixChange(kNodeId_, old_unique_ids, GetUniqueNodeIds()));
}

std::vector<NodeInfo> GroupMatrix::GetConnectedPeers() const {
  std::vector<NodeInfo> connected_peers;
  for (const auto& nodes : matrix_) {
    if (nodes.begin()->node_id != kNodeId_)
      connected_peers.push_back(nodes.at(0));
  }
  return connected_peers;
}

NodeInfo GroupMatrix::GetConnectedPeerFor(const NodeId& target_node_id) {
  /*
    for (const auto& nodes : matrix_) {
      if (nodes.at(0).node_id == target_node_id) {
        assert(false && "Shouldn't request connected peer for node in first column of group
    matrix.");
        return NodeInfo();
      }
    }*/
  for (const auto& nodes : matrix_) {
    if (std::find_if(std::begin(nodes), std::end(nodes),
                     [target_node_id](const NodeInfo& node_info) {
                       return (node_info.node_id == target_node_id);
                     }) != std::end(nodes)) {
      return nodes.at(0);
    }
  }
  return NodeInfo();
}

void GroupMatrix::GetBetterNodeForSendingMessage(const NodeId& target_node_id,
                                                 const std::vector<std::string>& exclude,
                                                 bool ignore_exact_match,
                                                 NodeInfo& current_closest_peer) {
  NodeId closest_id(current_closest_peer.node_id);

  for (const auto& row : matrix_) {
    if (ignore_exact_match && row.at(0).node_id == target_node_id)
      continue;
    if (std::find(exclude.begin(), exclude.end(), row.at(0).node_id.string()) != exclude.end())
      continue;

    for (const auto& node : row) {
      if (node.node_id == kNodeId_)
        continue;
      if (ignore_exact_match && node.node_id == target_node_id)
        continue;
      if (std::find(exclude.begin(), exclude.end(), node.node_id.string()) != exclude.end())
        continue;
      if (NodeId::CloserToTarget(node.node_id, closest_id, target_node_id)) {
        PrintGroupMatrix();
        LOG(kVerbose) << DebugId(closest_id) << ", peer to send: "
                      << DebugId(current_closest_peer.node_id) << ", "
                      << DebugId(row.at(0).node_id);
        closest_id = node.node_id;
        current_closest_peer = row.at(0);
        LOG(kVerbose) << DebugId(closest_id) << ", peer to send: "
                      << DebugId(current_closest_peer.node_id);
      }
    }
  }
  LOG(kVerbose) << "[" << DebugId(kNodeId_) << "]\ttarget: " << DebugId(target_node_id)
                << "\tfound node in matrix: " << DebugId(closest_id)
                << "\treccommend sending to: " << DebugId(current_closest_peer.node_id);
}

void GroupMatrix::GetBetterNodeForSendingMessage(const NodeId& target_node_id,
                                                 bool ignore_exact_match,
                                                 NodeId& current_closest_peer_id) {
  NodeId closest_id(current_closest_peer_id);

  for (const auto& row : matrix_) {
    if (ignore_exact_match && row.at(0).node_id == target_node_id)
      continue;

    for (const auto& node : row) {
      if (ignore_exact_match && node.node_id == target_node_id)
        continue;
      if (NodeId::CloserToTarget(node.node_id, closest_id, target_node_id)) {
        closest_id = node.node_id;
        current_closest_peer_id = row.at(0).node_id;
      }
    }
  }
  LOG(kVerbose) << "[" << DebugId(kNodeId_) << "]\ttarget: " << DebugId(target_node_id)
                << "\tfound node in matrix: " << DebugId(closest_id)
                << "\treccommend sending to: " << DebugId(current_closest_peer_id);
}

std::vector<NodeInfo> GroupMatrix::GetAllConnectedPeersFor(const NodeId& target_id) {
  std::vector<NodeInfo> connected_nodes;
  for (const auto& row : matrix_) {
    if (std::find_if(row.begin(), row.end(), [&target_id](const NodeInfo & node_info) {
          return target_id == node_info.node_id;
        }) != row.end()) {
      connected_nodes.push_back(row.at(0));
    }
  }
  return connected_nodes;
}

bool GroupMatrix::IsThisNodeGroupLeader(const NodeId& target_id, NodeId& connected_peer) {
  assert(!client_mode_ && "Client should not call IsThisNodeGroupLeader.");
  if (client_mode_)
    return false;

  LOG(kVerbose) << " Destination " << DebugId(target_id) << " kNodeId " << DebugId(kNodeId_);
  bool is_group_leader = true;
  if (unique_nodes_.empty()) {
    is_group_leader = true;
    return true;
  }

  std::string log("unique_nodes_ for " + DebugId(kNodeId_) + " are ");
  for (const auto& node : unique_nodes_) {
    log += DebugId(node.node_id) + ", ";
  }
  LOG(kVerbose) << log;

  for (const auto& node : unique_nodes_) {
    if (node.node_id == target_id)
      continue;
    if (NodeId::CloserToTarget(node.node_id, kNodeId_, target_id)) {
      LOG(kVerbose) << DebugId(node.node_id) << " could be leader";
      is_group_leader = false;
      break;
    }
  }
  if (!is_group_leader) {
    NodeId better_id(kNodeId_);
    GetBetterNodeForSendingMessage(target_id, true, better_id);
    connected_peer = better_id;
    assert(connected_peer != target_id);
  }
  return is_group_leader;
}

bool GroupMatrix::ClosestToId(const NodeId& target_id) {
  if (unique_nodes_.size() == 0)
    return true;

  PartialSortFromTarget(target_id, 2, unique_nodes_);
  if (unique_nodes_.at(0).node_id == kNodeId_)
    return true;

  if (unique_nodes_.at(0).node_id == target_id) {
    if (unique_nodes_.at(1).node_id == kNodeId_)
      return true;
    else
      return NodeId::CloserToTarget(kNodeId_, unique_nodes_.at(1).node_id, target_id);
  }

  return NodeId::CloserToTarget(kNodeId_, unique_nodes_.at(0).node_id, target_id);
}

// bool GroupMatrix::IsNodeIdInGroupRange(const NodeId& group_id, const NodeId& node_id) {
//  if (group_id == node_id)
//    return false;
//  if (unique_nodes_.size() < Parameters::group_size) {
//    if (node_id == kNodeId_)
//      return true;
//    else  // TODO(Prakash) throw not_connected here
//      BOOST_THROW_EXCEPTION(MakeError(RoutingErrors::not_in_group));
//  }
//  PartialSortFromTarget(group_id, Parameters::group_size, unique_nodes_);
//  NodeId furthest_group_node(unique_nodes_.at(Parameters::group_size - 1).node_id);
//  bool this_node_in_group(!NodeId::CloserToTarget(furthest_group_node, kNodeId_, group_id));
//  if (node_id == kNodeId_)
//    return this_node_in_group;
//  else if (!this_node_in_group)
//    BOOST_THROW_EXCEPTION(MakeError(RoutingErrors::not_in_group));
//  return !NodeId::CloserToTarget(furthest_group_node, node_id, group_id);
// }

GroupRangeStatus GroupMatrix::IsNodeIdInGroupRange(const NodeId& group_id,
                                                   const NodeId& node_id) const {
  size_t group_size_adjust(Parameters::group_size + 1U);
  size_t new_holders_size = std::min(unique_nodes_.size(), group_size_adjust);
  std::vector<NodeInfo> new_holders_info(new_holders_size);
  std::partial_sort_copy(unique_nodes_.begin(), unique_nodes_.end(), new_holders_info.begin(),
                         new_holders_info.end(),
                         [group_id](const NodeInfo & lhs, const NodeInfo & rhs) {
    return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, group_id);
  });

  std::vector<NodeId> new_holders;
  for (auto i : new_holders_info)
    new_holders.push_back(i.node_id);

  new_holders.erase(std::remove(new_holders.begin(), new_holders.end(), group_id),
                    new_holders.end());
  if (new_holders.size() > Parameters::group_size) {
    new_holders.resize(Parameters::group_size);
    assert(new_holders.size() == Parameters::group_size);
  }
  if (!client_mode_) {
    auto this_node_range(GetProximalRange(group_id, kNodeId_, kNodeId_, radius_, new_holders));
    if (node_id == kNodeId_)
      return this_node_range;
    else if (this_node_range != GroupRangeStatus::kInRange)
      BOOST_THROW_EXCEPTION(MakeError(RoutingErrors::not_in_range));  // not_in_group
  } else {
    if (node_id == kNodeId_)
      return GroupRangeStatus::kInProximalRange;
  }
  return GetProximalRange(group_id, node_id, kNodeId_, radius_, new_holders);
}

std::shared_ptr<MatrixChange> GroupMatrix::UpdateFromConnectedPeer(
    const NodeId& peer, const std::vector<NodeInfo>& nodes,
    const std::vector<NodeId>& old_unique_ids) {
  assert(nodes.size() < Parameters::max_routing_table_size);
  if (peer.IsZero()) {
    assert(false && "Invalid peer node id.");
    return std::make_shared<MatrixChange>(MatrixChange(kNodeId_, old_unique_ids, old_unique_ids));
  }
  // If peer is in my group
  auto group_itr(std::begin(matrix_));
  for (; group_itr != std::end(matrix_); ++group_itr) {
    if (group_itr->begin()->node_id == peer)
      break;
  }

  if (group_itr == std::end(matrix_)) {
    LOG(kWarning) << "Peer Node : " << DebugId(peer) << " is not in closest group of this node.";
    return std::make_shared<MatrixChange>(MatrixChange(kNodeId_, old_unique_ids, old_unique_ids));
  }

  // Update peer's row
  if (group_itr->size() > 1) {
    group_itr->erase(group_itr->begin() + 1, group_itr->end());
  }
  for (const auto& i : nodes)
    group_itr->push_back(i);

  // Update unique node vector
  Prune();
  UpdateUniqueNodeList();
  return std::make_shared<MatrixChange>(MatrixChange(kNodeId_, old_unique_ids, GetUniqueNodeIds()));
}

bool GroupMatrix::GetRow(const NodeId& row_id, std::vector<NodeInfo>& row_entries) {
  if (row_id.IsZero()) {
    assert(false && "Invalid node id.");
    return false;
  }
  auto group_itr(std::begin(matrix_));
  for (group_itr = std::begin(matrix_); group_itr != std::end(matrix_); ++group_itr) {
    if ((*group_itr).at(0).node_id == row_id)
      break;
  }

  if (group_itr == std::end(matrix_))
    return false;

  row_entries.clear();
  for (uint32_t i(0); i < (*group_itr).size(); ++i) {
    if (i != 0) {
      row_entries.push_back((*group_itr).at(i));
    }
  }
  return true;
}

std::vector<NodeInfo> GroupMatrix::GetUniqueNodes() const { return unique_nodes_; }

std::vector<NodeId> GroupMatrix::GetUniqueNodeIds() const {
  std::vector<NodeId> unique_node_ids;
  for (auto& node_info : unique_nodes_)
    unique_node_ids.push_back(node_info.node_id);
  return unique_node_ids;
}

bool GroupMatrix::IsRowEmpty(const NodeInfo& node_info) {
  auto group_itr(std::begin(matrix_));
  for (; group_itr != std::end(matrix_); ++group_itr) {
    if ((*group_itr).at(0).node_id == node_info.node_id)
      break;
  }
  assert(group_itr != std::end(matrix_));
  if (group_itr == std::end(matrix_))
    return false;

  return (group_itr->size() < 2);
}

std::vector<NodeInfo> GroupMatrix::GetClosestNodes(uint16_t size) {
  uint16_t size_to_sort(std::min(size, static_cast<uint16_t>(unique_nodes_.size())));
  PartialSortFromTarget(kNodeId_, size_to_sort, unique_nodes_);
  return std::vector<NodeInfo>(unique_nodes_.begin(), unique_nodes_.begin() + size_to_sort);
}

bool GroupMatrix::Contains(const NodeId& node_id) {
  return std::find_if(unique_nodes_.begin(), unique_nodes_.end(),
                      [&node_id](const NodeInfo & node_info) {
           return node_info.node_id == node_id;
         }) != unique_nodes_.end();
}

void GroupMatrix::UpdateUniqueNodeList() {
  std::set<NodeInfo, std::function<bool(const NodeInfo&, const NodeInfo&)>> sorted_to_owner([&](
      const NodeInfo & lhs,
      const NodeInfo & rhs) { return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, kNodeId_); });
  auto closest_nodes_size_adjust = Parameters::closest_nodes_size;
  if (!client_mode_) {
    NodeInfo node_info;
    node_info.node_id = kNodeId_;
    sorted_to_owner.insert(node_info);
    ++closest_nodes_size_adjust;
  }
  for (const auto& node_ids : matrix_) {
    for (const auto& node_id : node_ids)
      sorted_to_owner.insert(node_id);
  }
  unique_nodes_.assign(std::begin(sorted_to_owner), std::end(sorted_to_owner));

  // Updating radius
  NodeId fcn_distance;
  if (unique_nodes_.size() >= closest_nodes_size_adjust) {
    fcn_distance = kNodeId_ ^ unique_nodes_[closest_nodes_size_adjust - 1].node_id;

    radius_ =
        (crypto::BigInt((fcn_distance.ToStringEncoded(NodeId::EncodingType::kHex) + 'h').c_str()) *
         Parameters::proximity_factor);
  } else {
    fcn_distance = NodeId(NodeId::kMaxId);  // FIXME Prakash
    radius_ =
        (crypto::BigInt((fcn_distance.ToStringEncoded(NodeId::EncodingType::kHex) + 'h').c_str()));
  }
}

void GroupMatrix::PartialSortFromTarget(const NodeId& target, uint16_t number,
                                        std::vector<NodeInfo>& nodes) {
  uint16_t count = std::min(number, static_cast<uint16_t>(nodes.size()));
  std::partial_sort(nodes.begin(), nodes.begin() + count, nodes.end(),
                    [target](const NodeInfo & lhs, const NodeInfo & rhs) {
    return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target);
  });
}

void GroupMatrix::Prune() {
  if (matrix_.size() <= Parameters::closest_nodes_size)
    return;
  NodeId node_id;
  std::partial_sort(std::begin(matrix_), std::begin(matrix_) + Parameters::closest_nodes_size,
                    std::end(matrix_), [this](const std::vector<NodeInfo>& lhs,
                                              const std::vector<NodeInfo>& rhs) {
                                         return NodeId::CloserToTarget(lhs.begin()->node_id,
                                                                       rhs.begin()->node_id,
                                                                       kNodeId_);
                                       });
  auto itr(std::begin(matrix_));
  std::advance(itr, Parameters::closest_nodes_size);
  while (itr != std::end(matrix_)) {
    if (client_mode_) {
      LOG(kInfo) << DebugId(kNodeId_) << " matrix conected removes "
                 << DebugId(itr->begin()->node_id);
      itr = matrix_.erase(itr);
      continue;
    }
    node_id = itr->begin()->node_id;
    if (itr->size() <= Parameters::closest_nodes_size) {
      if (itr->size() > 1) {  // avoids removing the recently added node
        LOG(kInfo) << DebugId(kNodeId_) << " matrix conected removes " << DebugId(node_id);
        itr = matrix_.erase(itr);
      } else {
        itr++;
      }
      continue;
    }
    std::sort(itr->begin() + 1, itr->end(), [node_id](const NodeInfo& lhs, const NodeInfo& rhs) {
                                              return NodeId::CloserToTarget(lhs.node_id,
                                                                            rhs.node_id, node_id);
                                            });
    if (NodeId::CloserToTarget(itr->at(Parameters::closest_nodes_size).node_id, kNodeId_,
                               node_id) ||
        (std::find_if(std::begin(*itr), std::end(*itr), [&](const NodeInfo& node_info) {
                                                          return node_info.node_id == kNodeId_;
                                                        }) == std::end(*itr))) {
      LOG(kInfo) << DebugId(kNodeId_) << " matrix conected removes "
                 << DebugId(itr->begin()->node_id);
      itr = matrix_.erase(itr);
    } else {
      itr++;
    }
  }
  PrintGroupMatrix();
}

void GroupMatrix::PrintGroupMatrix() {
  auto group_itr(std::begin(matrix_));
  std::string tab("\t");
  std::string output("Group matrix of node with NodeID: " + DebugId(kNodeId_));
  for (group_itr = std::begin(matrix_); group_itr != std::end(matrix_); ++group_itr) {
    output.append("\nGroup matrix row:");
    for (const auto& elem : (*group_itr)) {
      output.append(tab);
      output.append(DebugId(elem.node_id));
    }
  }
  LOG(kVerbose) << output;
}

}  // namespace routing

}  // namespace maidsafe
