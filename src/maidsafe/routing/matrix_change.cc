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

#include "maidsafe/routing/matrix_change.h"

#include <limits>
#include <utility>

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

MatrixChange::MatrixChange()
    : node_id_(),
      old_matrix_(),
      new_matrix_(),
      lost_nodes_(),
      new_nodes_(),
      radius_() {}

MatrixChange::MatrixChange(const MatrixChange& other)
    : node_id_(other.node_id_),
      old_matrix_(other.old_matrix_),
      new_matrix_(other.new_matrix_),
      lost_nodes_(other.lost_nodes_),
      new_nodes_(other.new_nodes_),
      radius_(other.radius_) {}

MatrixChange::MatrixChange(MatrixChange&& other)
    : node_id_(std::move(other.node_id_)),
      old_matrix_(std::move(other.old_matrix_)),
      new_matrix_(std::move(other.new_matrix_)),
      lost_nodes_(std::move(other.lost_nodes_)),
      new_nodes_(std::move(other.new_nodes_)),
      radius_(std::move(other.radius_)) {}

MatrixChange& MatrixChange::operator=(MatrixChange other) {
  swap(*this, other);
  return *this;
}

MatrixChange::MatrixChange(NodeId this_node_id, const std::vector<NodeId>& old_matrix,
                           const std::vector<NodeId>& new_matrix)
    : node_id_(std::move(this_node_id)),
      old_matrix_([this](std::vector<NodeId> old_matrix_in)->std::vector<NodeId> {
        std::sort(std::begin(old_matrix_in), std::end(old_matrix_in),
                  [this](const NodeId & lhs, const NodeId & rhs) {
          return NodeId::CloserToTarget(lhs, rhs, node_id_);
        });
        return old_matrix_in;
      }(old_matrix)),
      new_matrix_([this](std::vector<NodeId> new_matrix_in)->std::vector<NodeId> {
        std::sort(std::begin(new_matrix_in), std::end(new_matrix_in),
                  [this](const NodeId & lhs, const NodeId & rhs) {
          return NodeId::CloserToTarget(lhs, rhs, node_id_);
        });
        return new_matrix_in;
      }(new_matrix)),
      lost_nodes_([this]()->std::vector<NodeId> {
        std::vector<NodeId> lost_nodes;
        std::set_difference(std::begin(old_matrix_), std::end(old_matrix_), std::begin(new_matrix_),
                            std::end(new_matrix_), std::back_inserter(lost_nodes),
                            [this](const NodeId & lhs, const NodeId & rhs) {
          return NodeId::CloserToTarget(lhs, rhs, node_id_);
        });
        return lost_nodes;
      }()),
      new_nodes_([this]()->std::vector<NodeId> {
        std::vector<NodeId> new_nodes;
        std::set_difference(std::begin(new_matrix_), std::end(new_matrix_), std::begin(old_matrix_),
                            std::end(old_matrix_), std::back_inserter(new_nodes),
                            [this](const NodeId & lhs, const NodeId & rhs) {
          return NodeId::CloserToTarget(lhs, rhs, node_id_);
        });
        return new_nodes;
      }()),
      radius_([this]()->crypto::BigInt {
        NodeId fcn_distance;
        if (new_matrix_.size() >= Parameters::closest_nodes_size)
          fcn_distance = node_id_ ^ new_matrix_[Parameters::closest_nodes_size - 1];
        else
          fcn_distance = node_id_ ^ (NodeId(NodeId::kMaxId));  // FIXME
        return (crypto::BigInt(
                    (fcn_distance.ToStringEncoded(NodeId::EncodingType::kHex) + 'h').c_str()) *
                Parameters::proximity_factor);
      }()) {}

CheckHoldersResult MatrixChange::CheckHolders(const NodeId& target) const {
  // Handle cases of lower number of group matrix nodes
  size_t node_group_size_adjust(Parameters::node_group_size + 1U);
  size_t old_holders_size = std::min(old_matrix_.size(), node_group_size_adjust);
  size_t new_holders_size = std::min(new_matrix_.size(), node_group_size_adjust);

  std::vector<NodeId> old_holders(old_holders_size), new_holders(new_holders_size),
      lost_nodes(lost_nodes_);
  std::partial_sort_copy(std::begin(old_matrix_), std::end(old_matrix_), std::begin(old_holders),
                         std::end(old_holders), [target](const NodeId & lhs, const NodeId & rhs) {
    return NodeId::CloserToTarget(lhs, rhs, target);
  });
  std::partial_sort_copy(std::begin(new_matrix_), std::end(new_matrix_), std::begin(new_holders),
                         std::end(new_holders), [target](const NodeId & lhs, const NodeId & rhs) {
    return NodeId::CloserToTarget(lhs, rhs, target);
  });
  std::sort(std::begin(lost_nodes), std::end(lost_nodes),
            [target](const NodeId & lhs, const NodeId & rhs) {
    return NodeId::CloserToTarget(lhs, rhs, target);
  });

  // Remove target == node ids and adjust holder size
  old_holders.erase(std::remove(std::begin(old_holders), std::end(old_holders), target),
                    std::end(old_holders));
  if (old_holders.size() > Parameters::node_group_size) {
    old_holders.resize(Parameters::node_group_size);
    assert(old_holders.size() == Parameters::node_group_size);
  }
  new_holders.erase(std::remove(std::begin(new_holders), std::end(new_holders), target),
                    std::end(new_holders));
  if (new_holders.size() > Parameters::node_group_size) {
    new_holders.resize(Parameters::node_group_size);
    assert(new_holders.size() == Parameters::node_group_size);
  }
  lost_nodes.erase(std::remove(std::begin(lost_nodes), std::end(lost_nodes), target),
                   std::end(lost_nodes));

  CheckHoldersResult holders_result;
  holders_result.proximity_status =
      GetProximalRange(target, node_id_, node_id_, radius_, new_holders);
  // Only return holders if this node is part of target group
  if (GroupRangeStatus::kInRange != holders_result.proximity_status)
    return holders_result;

  // Old holders = Old holder ∩ Lost nodes
  std::set_intersection(std::begin(old_holders), std::end(old_holders), std::begin(lost_nodes),
                        std::end(lost_nodes), std::back_inserter(holders_result.old_holders),
                        [target](const NodeId & lhs, const NodeId & rhs) {
    return NodeId::CloserToTarget(lhs, rhs, target);
  });

  // New holders = All new holders - Old holders
  std::set_difference(std::begin(new_holders), std::end(new_holders), std::begin(old_holders),
                      std::end(old_holders), std::back_inserter(holders_result.new_holders),
                      [target](const NodeId & lhs, const NodeId & rhs) {
    return NodeId::CloserToTarget(lhs, rhs, target);
  });
  return holders_result;
}

NodeId MatrixChange::ChoosePmidNode(const std::set<NodeId>& online_pmids,
                                    const NodeId& target) const {
  if (online_pmids.empty())
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::invalid_parameter));

  LOG(kInfo) << "MatrixChange::ChoosePmidNode having following new_matrix_ : ";
  for (auto id : new_matrix_)
    LOG(kInfo) << "       new_matrix_ ids     ---  " << HexSubstr(id.string());
  LOG(kInfo) << "MatrixChange::ChoosePmidNode having target : "
                << HexSubstr(target.string()) << " and following online_pmids : ";
  for (auto pmid : online_pmids)
    LOG(kInfo) << "       online_pmids        ---  " << HexSubstr(pmid.string());

  // In case storing to PublicPmid, the data shall not be stored on the Vault itself
  // However, the vault will appear in DM's routing table and affect result
  std::vector<NodeId> temp(Parameters::node_group_size + 1);
  std::partial_sort_copy(std::begin(new_matrix_), std::end(new_matrix_), std::begin(temp),
                         std::end(temp), [&target](const NodeId& lhs, const NodeId& rhs) {
    return NodeId::CloserToTarget(lhs, rhs, target);
  });

  LOG(kInfo) << "MatrixChange::ChoosePmidNode own id : "
                << HexSubstr(node_id_.string()) << " and closest+1 to the target are : ";
  for (auto node : temp)
    LOG(kInfo) << "       sorted_neighbours   ---  " << HexSubstr(node.string());

  auto temp_itr(std::begin(temp));
  auto pmids_itr(std::begin(online_pmids));
  while (*temp_itr != node_id_) {
    ++temp_itr;
    assert(temp_itr != std::end(temp));
    if (++pmids_itr == std::end(online_pmids))
      pmids_itr = std::begin(online_pmids);
  }

  return *pmids_itr;
}

bool MatrixChange::OldEqualsToNew() const {
  return old_matrix_ == new_matrix_;
}

void swap(MatrixChange& lhs, MatrixChange& rhs) MAIDSAFE_NOEXCEPT {
  using std::swap;
  swap(lhs.node_id_, rhs.node_id_);
  swap(lhs.old_matrix_, rhs.old_matrix_);
  swap(lhs.new_matrix_, rhs.new_matrix_);
  swap(lhs.lost_nodes_, rhs.lost_nodes_);
  swap(lhs.radius_, rhs.radius_);
}

}  // namespace routing

}  // namespace maidsafe
