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

#include "maidsafe/routing/matrix_change.h"

#include <limits>


#include "maidsafe/routing/parameters.h"


namespace maidsafe {

namespace routing {

namespace test1 {

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
  // throw / handle cases of lower number of group matrix nodes
  assert(kOldMatrix_.size() >= Parameters::node_group_size + 1U);  // FIXME
  assert(kNewMatrix_.size() >= Parameters::node_group_size + 1U);  // FIXME

  std::vector<NodeId> old_holders(Parameters::node_group_size + 1),
                      new_holders(Parameters::node_group_size + 1),
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

  // Remove taget == node ids
  std::remove(old_holders.begin(), old_holders.end(), target);
  old_holders.resize(Parameters::node_group_size);
  std::remove(new_holders.begin(), new_holders.end(), target);
  new_holders.resize(Parameters::node_group_size);
  std::remove(lost_nodes.begin(), lost_nodes.end(), target);

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
  holders_result.proximity_status =  GetProximalRange(new_holders, target);
  return holders_result;
}

GroupRangeStatus MatrixChange::GetProximalRange(const std::vector<NodeId>& new_holders,
                                                const NodeId& target) const {
  if ((target == kNodeId_))
    return GroupRangeStatus::kOutwithRange;

  if (std::find(new_holders.begin(), new_holders.end(), kNodeId_) != new_holders.end())
    return GroupRangeStatus::kInRange;

  NodeId distance_id(kNodeId_ ^ target);
  crypto::BigInt distance((distance_id.ToStringEncoded(NodeId::kHex) + 'h').c_str());

  return (distance < kRadius_) ? GroupRangeStatus::kInProximalRange
                               : GroupRangeStatus::kOutwithRange;
}

bool MatrixChange::OldEqualsToNew() const {
  if (kOldMatrix_.size() != kNewMatrix_.size())
    return false;
  return std::equal(kNewMatrix_.begin(), kNewMatrix_.end(), kOldMatrix_.begin());
}

}  // namespace testing

}  // namespace routing

}  // namespace maidsafe
