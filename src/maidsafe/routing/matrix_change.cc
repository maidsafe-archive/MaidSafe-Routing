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

#include "maidsafe/routing/matrix_change.h"

#include <limits>


#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/utils.h"


namespace maidsafe {

namespace routing {

MatrixChange::MatrixChange(const NodeId& this_node_id, const std::vector<NodeId>& old_matrix,
                           const std::vector<NodeId>& new_matrix)
    : kNodeId_(this_node_id),
      kOldMatrix_([this](std::vector<NodeId> old_matrix_in)->std::vector<NodeId> {
                    std::sort(old_matrix_in.begin(),
                              old_matrix_in.end(),
                              [this](const NodeId& lhs, const NodeId& rhs) {
                                return NodeId::CloserToTarget(lhs, rhs, kNodeId_);
                              });
                    return old_matrix_in;
                  } (old_matrix)),
      kNewMatrix_([this](std::vector<NodeId> new_matrix_in)->std::vector<NodeId> {
                    std::sort(new_matrix_in.begin(),
                              new_matrix_in.end(),
                              [this](const NodeId& lhs, const NodeId& rhs) {
                                return NodeId::CloserToTarget(lhs, rhs, kNodeId_);
                              });
                    return new_matrix_in;
                  } (new_matrix)),
      kLostNodes_([this]()->std::vector<NodeId> {
                    std::vector<NodeId> lost_nodes;
                    std::set_difference(kOldMatrix_.begin(),
                                        kOldMatrix_.end(),
                                        kNewMatrix_.begin(),
                                        kNewMatrix_.end(),
                                        std::back_inserter(lost_nodes),
                                        [this](const NodeId& lhs, const NodeId& rhs) {
                                          return NodeId::CloserToTarget(lhs, rhs, kNodeId_);
                                        });
                    return lost_nodes;
                  } ()),
      kRadius_([this]()->crypto::BigInt {
                 NodeId fcn_distance;
                 if (kNewMatrix_.size() >= Parameters::closest_nodes_size)
                   fcn_distance = kNodeId_ ^ kNewMatrix_[Parameters::closest_nodes_size -1];
                 else
                   fcn_distance = kNodeId_ ^ (NodeId(NodeId::kMaxId));  // FIXME
                 return (crypto::BigInt((fcn_distance.ToStringEncoded(NodeId::kHex) + 'h').c_str())
                             * Parameters::proximity_factor);
               } ()) {}

CheckHoldersResult MatrixChange::CheckHolders(const NodeId& target) const {
  // Handle cases of lower number of group matrix nodes
  size_t node_group_size_adjust(Parameters::node_group_size + 1U);
  size_t old_holders_size = std::min(kOldMatrix_.size(), node_group_size_adjust);
  size_t new_holders_size = std::min(kNewMatrix_.size(), node_group_size_adjust);

  std::vector<NodeId> old_holders(old_holders_size), new_holders(new_holders_size),
                      lost_nodes(kLostNodes_);
  std::partial_sort_copy(kOldMatrix_.begin(),
                         kOldMatrix_.end(),
                         old_holders.begin(),
                         old_holders.end(),
                         [target](const NodeId& lhs, const NodeId& rhs) {
                           return NodeId::CloserToTarget(lhs, rhs, target);
                         });
  std::partial_sort_copy(kNewMatrix_.begin(),
                         kNewMatrix_.end(),
                         new_holders.begin(),
                         new_holders.end(),
                         [target](const NodeId& lhs, const NodeId& rhs) {
                           return NodeId::CloserToTarget(lhs, rhs, target);
                         });
  std::sort(lost_nodes.begin(),
            lost_nodes.end(),
            [target](const NodeId& lhs, const NodeId& rhs) {
              return NodeId::CloserToTarget(lhs, rhs, target);
            });

  // Remove taget == node ids and adjust holder size
  old_holders.erase(std::remove(old_holders.begin(), old_holders.end(), target), old_holders.end());
  if (old_holders.size() > Parameters::node_group_size) {
    old_holders.resize(Parameters::node_group_size);
    assert(old_holders.size() == Parameters::node_group_size);
  }
  new_holders.erase(std::remove(new_holders.begin(), new_holders.end(), target), new_holders.end());
  if (new_holders.size() > Parameters::node_group_size) {
    new_holders.resize(Parameters::node_group_size);
    assert(new_holders.size() == Parameters::node_group_size);
  }
  lost_nodes.erase(std::remove(lost_nodes.begin(), lost_nodes.end(), target), lost_nodes.end());

  CheckHoldersResult holders_result;
  // Old holders = Old holder ∩ Lost nodes
  std::set_intersection(old_holders.begin(),
                        old_holders.end(),
                        lost_nodes.begin(),
                        lost_nodes.end(),
                        std::back_inserter(holders_result.old_holders),
                        [target](const NodeId& lhs, const NodeId& rhs) {
                          return NodeId::CloserToTarget(lhs, rhs, target);
                        });
  // New holders = New holders - Old holders
  std::set_difference(new_holders.begin(),
                      new_holders.end(),
                      old_holders.begin(),
                      old_holders.end(),
                      std::back_inserter(holders_result.new_holders),
                      [target](const NodeId& lhs, const NodeId& rhs) {
                        return NodeId::CloserToTarget(lhs, rhs, target);
                      });
  // handle range for this node
  holders_result.proximity_status =  GetProximalRange(target, kNodeId_, kNodeId_, kRadius_,
                                                      new_holders);
  if (GroupRangeStatus::kInRange != holders_result.proximity_status) {
    holders_result.new_holders.clear();
    holders_result.new_holders.shrink_to_fit();
    holders_result.old_holders.clear();
    holders_result.old_holders.shrink_to_fit();
  }

  return holders_result;
}

bool MatrixChange::OldEqualsToNew() const {
  if (kOldMatrix_.size() != kNewMatrix_.size())
    return false;
  return std::equal(kNewMatrix_.begin(), kNewMatrix_.end(), kOldMatrix_.begin());
}

MatrixChange::MatrixChange(MatrixChange&& other)
    :  kNodeId_(std::move(other.kNodeId_)),
       kOldMatrix_(std::move(other.kOldMatrix_)),
       kNewMatrix_(std::move(other.kNewMatrix_)),
       kLostNodes_(std::move(other.kLostNodes_)) {}

}  // namespace routing

}  // namespace maidsafe
