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
#include <utility>

#include "maidsafe/common/log.h"
#include "maidsafe/common/make_unique.h"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

namespace {

GroupMatrix::SortedGroup CreateSortedGroup(const NodeId& target) {
  return GroupMatrix::SortedGroup{
      [target](const NodeInfo& lhs, const NodeInfo& rhs) {
        return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target);
      } };
}

template<typename InputIterator>
GroupMatrix::SortedGroup CreateSortedGroup(const NodeId& target, InputIterator first,
                                           InputIterator last) {
  return GroupMatrix::SortedGroup{
      first, last, [target](const NodeInfo& lhs, const NodeInfo& rhs) {
                     return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target);
                   } };
}

}  // unnamed namespace

GroupMatrix::GroupMatrix(const NodeId& this_node_id, bool client_mode)
    : kNodeId_(this_node_id),
      unique_nodes_(CreateSortedGroup(kNodeId_)),
      radius_(crypto::BigInt::Zero()),
      kClientMode_(client_mode),
      matrix_([&](const NodeInfo& lhs, const NodeInfo& rhs) {
        return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, kNodeId_);
      }) {
  UpdateUniqueNodeList();
}

std::shared_ptr<MatrixChange> GroupMatrix::AddConnectedPeer(
    const NodeInfo& connected_peer, const std::vector<NodeInfo>& peers_close_connections) {
  ValidatePeersCloseConnections(connected_peer.node_id, peers_close_connections);
  std::vector<NodeId> old_unique_ids{ GetUniqueNodeIds() };
  assert(matrix_.find(connected_peer) == std::end(matrix_));
  matrix_.emplace(std::make_pair(connected_peer,
      CreateSortedGroup(connected_peer.node_id, std::begin(peers_close_connections),
                        std::end(peers_close_connections))));
  UpdateUniqueNodeList();
  return std::make_shared<MatrixChange>(kNodeId_, old_unique_ids, GetUniqueNodeIds());
}

std::shared_ptr<MatrixChange> GroupMatrix::UpdateConnectedPeer(const NodeId& connected_peer_id,
    const std::vector<NodeInfo>& peers_close_connections,
    const std::vector<NodeId>& old_unique_ids) {
  ValidatePeersCloseConnections(connected_peer_id, peers_close_connections);

  //Why?
  //// If connected_peer is in my group
  //auto group_itr(std::begin(matrix_));
  //for (; group_itr != std::end(matrix_); ++group_itr) {
  //  if (group_itr->begin()->node_id == connected_peer)
  //    break;
  //}
  //if (group_itr == std::end(matrix_)) {
  //  LOG(kWarning) << "Peer Node : " << connected_peer << " is not in closest group of this node.";
  //  return std::make_shared<MatrixChange>(MatrixChange(kNodeId_, old_unique_ids, old_unique_ids));
  //}

  NodeInfo connected_peer;
  connected_peer.node_id = connected_peer_id;
  auto itr(matrix_.find(connected_peer));
  assert(itr != std::end(matrix_));
  itr->second = CreateSortedGroup(connected_peer_id, std::begin(peers_close_connections),
                                  std::end(peers_close_connections));
  UpdateUniqueNodeList();
  return std::make_shared<MatrixChange>(kNodeId_, old_unique_ids, GetUniqueNodeIds());
}

std::shared_ptr<MatrixChange> GroupMatrix::RemoveConnectedPeer(const NodeInfo& node_info) {
  std::vector<NodeId> old_unique_ids(GetUniqueNodeIds());
  if (!matrix_.erase(node_info))
    LOG(kWarning) << kNodeId_ << " has already removed " << node_info.node_id << " from matrix.";
  UpdateUniqueNodeList();
  return std::make_shared<MatrixChange>(MatrixChange(kNodeId_, old_unique_ids, GetUniqueNodeIds()));
}

std::vector<NodeInfo> GroupMatrix::GetConnectedPeers() const {
  std::vector<NodeInfo> connected_peers;
  auto itr(std::begin(matrix_));
  if (!kClientMode_)
    ++itr;  // Avoids including this node's info.
  while (itr != std::end(matrix_)) {
    connected_peers.push_back(itr->first);
    ++itr;
  }
  return connected_peers;
}

std::vector<NodeInfo> GroupMatrix::GetUniqueNodes() const {
  return std::vector<NodeInfo>{ std::begin(unique_nodes_), std::end(unique_nodes_) };
}

std::vector<NodeId> GroupMatrix::GetUniqueNodeIds() const {
  std::vector<NodeId> unique_node_ids;
  for (const auto& node_info : unique_nodes_)
    unique_node_ids.push_back(node_info.node_id);
  return unique_node_ids;
}

std::unique_ptr<NodeInfo> GroupMatrix::GetBetterNodeForSendingMessage(const NodeId& target_id,
    bool ignore_exact_match, const NodeInfo& current_closest_peer,
    std::vector<NodeId> exclusions) const {
  // Exclude target_id if required, and exclude this node's own ID.
  if (ignore_exact_match)
    exclusions.push_back(target_id);
  exclusions.push_back(kNodeId_);

  // Find closest peer, whether connected or not.
  SortedGroup unique_nodes_copy{ CreateSortedGroup(target_id, std::begin(unique_nodes_),
                                                   std::end(unique_nodes_)) };
  SortedGroup::iterator unique_itr{ std::begin(unique_nodes_copy) };
  while (unique_itr != std::end(unique_nodes_copy)) {
    if (!NodeId::CloserToTarget(unique_itr->node_id, current_closest_peer.node_id, target_id)) {
      unique_itr = std::end(unique_nodes_copy);
      break;
    }
    if (std::none_of(std::begin(exclusions), std::end(exclusions),
                     [&](const NodeId& node_id) { return unique_itr->node_id == node_id; })) {
      break;  // Found closest peer.
    }
  }

  // Return failure if we don't have a closer peer
  if (unique_itr == std::end(unique_nodes_copy))
    return std::unique_ptr<NodeInfo>();

  // Try to find the peer in the connected group
  auto itr(matrix_.find(*unique_itr));
  if (itr == std::end(matrix_)) {
    // This node isn't connected to the closest peer; return a peer which *is* connected.
    itr = std::find_if(std::begin(matrix_), std::end(matrix_),
                       [unique_itr](const Matrix::value_type& row) {
                         return row.second.find(*unique_itr) != std::end(row.second);
                       });
    assert(itr != std::end(matrix_));
  }

  LOG(kVerbose) << "[" << kNodeId_ << "]  target: " << target_id << "  found " << itr->first.node_id
                << " in matrix which is better to send to than " << current_closest_peer.node_id;
  return maidsafe::make_unique<NodeInfo>(itr->first);
}

bool GroupMatrix::IsThisNodeGroupLeader(const NodeId& target_id, NodeId& connected_peer) const {
  assert(!kClientMode_ && "Client should not call IsThisNodeGroupLeader.");
  if (kClientMode_)
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

bool GroupMatrix::IsThisNodeClosestToId(const NodeId& target_id) const {

  for (const auto& node : unique_nodes_) {

  }

  if (unique_nodes_.empty())
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

GroupRangeStatus GroupMatrix::IsNodeIdInGroupRange(const NodeId& group_id,
                                                   const NodeId& node_id) const {
  auto connected_peers(GetConnectedPeers());
  if (connected_peers.empty())
    return GroupRangeStatus::kInRange;

  std::partial_sort(std::begin(connected_peers), std::begin(connected_peers) + 1,
                    std::end(connected_peers),
                    [node_id](const NodeInfo& lhs, const NodeInfo& rhs) {
                      return NodeId::CloserToTarget(rhs.node_id, lhs.node_id, node_id);
                    });

  if (connected_peers.size() >= Parameters::closest_nodes_size && node_id == kNodeId_ &&
      NodeId::CloserToTarget(connected_peers.front().node_id, group_id, kNodeId_)) {
    return GroupRangeStatus::kOutwithRange;
  }

  size_t group_size_adjust(Parameters::group_size + 1U);
  size_t new_holders_size = std::min(unique_nodes_.size(), group_size_adjust);
  std::vector<NodeInfo> new_holders_info(new_holders_size);
  std::partial_sort_copy(unique_nodes_.begin(), unique_nodes_.end(), new_holders_info.begin(),
                         new_holders_info.end(),
                         [group_id](const NodeInfo& lhs, const NodeInfo& rhs) {
    return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, group_id);
  });

  std::vector<NodeId> new_holders;
  for (const auto& new_holder_info : new_holders_info)
    new_holders.push_back(new_holder_info.node_id);

  new_holders.erase(std::remove(new_holders.begin(), new_holders.end(), group_id),
                    new_holders.end());
  if (new_holders.size() > Parameters::group_size) {
    new_holders.resize(Parameters::group_size);
    assert(new_holders.size() == Parameters::group_size);
  }
  if (!kClientMode_) {
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

void GroupMatrix::ValidatePeersCloseConnections(const NodeId& connected_peer_id,
    const std::vector<NodeInfo>& peers_close_connections) const {
  assert(!connected_peer_id.IsZero());
  assert(peers_close_connections.size() < Parameters::max_routing_table_size);
  // This node's ID should be in peer's close group
  assert(std::any_of(std::begin(peers_close_connections), std::end(peers_close_connections),
                     [&](const NodeInfo& node) { return node.node_id == kNodeId_; }));
}

void GroupMatrix::Prune() {
  if (matrix_.size() <= Parameters::closest_nodes_size)
    return;

  NodeId node_id;
  auto itr(std::begin(matrix_));
  // Skip ahead to the "forced connections" range.
  std::advance(itr, Parameters::closest_nodes_size);
  if (kClientMode_) {
    matrix_.erase(itr, std::end(matrix_));
    return;
  }

  while (itr != std::end(matrix_)) {
    if (itr->size() <= Parameters::closest_nodes_size) {
      ++itr;
      continue;
    }

    // If this node is not in peer's close group, drop peer from matrix.
    if (std::distance(std::begin(itr->second), itr->second.find(kNode_id_)) >
      Parameters::closest_nodes_size) {
      LOG(kInfo) << kNodeId_ << " removed " << itr->first.node_id
        << " from forced connections in matrix.";
      itr = matrix_.erase(itr);
    }
    else {
      ++itr;
    }
  }
  //  PrintGroupMatrix();
}

void GroupMatrix::UpdateUniqueNodeList() {
  Prune();

  uint16_t adjusted_closest_nodes_size{ Parameters::closest_nodes_size };
  if (!kClientMode_) {
    NodeInfo node_info;
    node_info.node_id = kNodeId_;
    unique_nodes_.emplace(std::move(node_info));
    ++adjusted_closest_nodes_size;
  }

  for (const auto& row : matrix_) {
    unique_nodes_.insert(row.first);
    for (const auto& node : row.second)
      unique_nodes_.insert(node);
  }

  // Updating radius
  if (unique_nodes_.size() >= closest_nodes_size_adjust) {
    NodeId fcn_distance{ kNodeId_ ^ unique_nodes_[closest_nodes_size_adjust - 1].node_id };
    radius_ =
        crypto::BigInt{ (fcn_distance.ToStringEncoded(NodeId::EncodingType::kHex) + 'h').c_str() } *
        Parameters::proximity_factor);
  } else {
    // FIXME
    radius_ = crypto::BigInt{
        (NodeId{ NodeId::kMaxId }.ToStringEncoded(NodeId::EncodingType::kHex) + 'h').c_str() };
  }
}

//#ifdef TESTING
//NodeInfo GroupMatrix::GetConnectedPeerFor(const NodeId& target_node_id) const {
//  /*
//  for (const auto& nodes : matrix_) {
//  if (nodes.at(0).node_id == target_node_id) {
//  assert(false && "Shouldn't request connected peer for node in first column of group
//  matrix.");
//  return NodeInfo();
//  }
//  }*/
//  for (const auto& nodes : matrix_) {
//    if (std::find_if(std::begin(nodes), std::end(nodes),
//      [target_node_id](const NodeInfo& node_info) {
//      return (node_info.node_id == target_node_id);
//    }) != std::end(nodes)) {
//      return nodes.at(0);
//    }
//  }
//  return NodeInfo();
//}
//
//bool GroupMatrix::GetRow(const NodeId& row_id, std::vector<NodeInfo>& row_entries) const {
//  if (row_id.IsZero()) {
//    assert(false && "Invalid node id.");
//    return false;
//  }
//  auto group_itr(std::begin(matrix_));
//  for (group_itr = std::begin(matrix_); group_itr != std::end(matrix_); ++group_itr) {
//    if ((*group_itr).at(0).node_id == row_id)
//      break;
//  }
//
//  if (group_itr == std::end(matrix_))
//    return false;
//
//  row_entries.clear();
//  for (uint32_t i(0); i < (*group_itr).size(); ++i) {
//    if (i != 0) {
//      row_entries.push_back((*group_itr).at(i));
//    }
//  }
//  return true;
//}
//#endif

void GroupMatrix::PrintGroupMatrix() const {
#if USE_LOGGING
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
#endif
}

}  // namespace routing

}  // namespace maidsafe
