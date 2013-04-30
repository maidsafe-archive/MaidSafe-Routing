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

MatrixChange::MatrixChange(const NodeId& this_node_id, std::vector<NodeId> old_matrix,
                           std::vector<NodeId> new_matrix)
    : kNodeId_(this_node_id),
      radius_(),
      old_matrix_(old_matrix),
      new_matrix_(new_matrix),
      lost_nodes_() {
  std::sort(old_matrix_.begin(),
            old_matrix_.end(),
            [this](const NodeId& lhs, const NodeId& rhs) {
              return NodeId::CloserToTarget(lhs, rhs, kNodeId_);
            });
  std::sort(new_matrix_.begin(),
            new_matrix_.end(),
            [this](const NodeId& lhs, const NodeId& rhs) {
              return NodeId::CloserToTarget(lhs, rhs, kNodeId_);
            });

  std::set_difference(old_matrix_.begin(),
                      old_matrix_.end(),
                      new_matrix_.begin(),
                      new_matrix_.end(),
                      std::back_inserter(lost_nodes_),
                      [this](const NodeId& lhs, const NodeId& rhs) {
                        return NodeId::CloserToTarget(lhs, rhs, kNodeId_);
                      });
  NodeId furthest_closest_distance;
  if (new_matrix_.size() >= Parameters::closest_nodes_size)
    furthest_closest_distance = kNodeId_ ^ new_matrix_[Parameters::closest_nodes_size -1];
  else
    furthest_closest_distance = kNodeId_ ^ (NodeId(NodeId::kMaxId));  //FIXME
  radius_ = crypto::BigInt((furthest_closest_distance.ToStringEncoded(NodeId::kHex) + 'h').c_str())
              * Parameters::proximity_factor;
//  LOG(kInfo) << " old_matrix_ exiting Constructor" << "this_node_id : " << DebugId(kNodeId_);
//  for (auto i : old_matrix_)
//    LOG(kInfo) << DebugId(i);

}

CheckHoldersResult MatrixChange::CheckHolders(const NodeId& target) {
  // throw / handle cases of lower number of group matrix nodes
  auto old_matrix(old_matrix_), new_matrix(new_matrix_), lost_nodes(lost_nodes_);

  // remove taget == node ids
  std::remove(old_matrix.begin(), old_matrix.end(), target);
  std::remove(new_matrix.begin(), new_matrix.end(), target);
  std::remove(lost_nodes.begin(), lost_nodes.end(), target);

  std::partial_sort(old_matrix.begin(),
                    old_matrix.end() + Parameters::node_group_size,
                    old_matrix.end(),
                    [target](const NodeId& lhs, const NodeId& rhs) {
                      return NodeId::CloserToTarget(lhs, rhs, target);
                    });
  std::partial_sort(new_matrix.begin(),
                    new_matrix.end() + Parameters::node_group_size,
                    new_matrix.end(),
                    [target](const NodeId& lhs, const NodeId& rhs) {
                      return NodeId::CloserToTarget(lhs, rhs, target);
                    });

  std::vector<NodeId> old_holders(old_matrix.begin(),
                                  old_matrix.end() + Parameters::node_group_size);

  std::vector<NodeId> new_holders(new_matrix.begin(),
                                  new_matrix.end() + Parameters::node_group_size);

  std::sort(lost_nodes.begin(),
            lost_nodes.end(),
            [target](const NodeId& lhs, const NodeId& rhs) {
              return NodeId::CloserToTarget(lhs, rhs, target);
            });

  CheckHoldersResult holders_result;
  // old holders = Old 4 holder intersection lost nodes
  std::set_intersection(old_holders.begin(),
                        old_holders.end(),
                        lost_nodes.begin(),
                        lost_nodes.end(),
                        std::back_inserter(holders_result.old_holders),
                        [target](const NodeId& lhs, const NodeId& rhs) {
                          return NodeId::CloserToTarget(lhs, rhs, target);
                        });
 // new holders = new 4 holders - old 4 holders
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

CheckHoldersResult MatrixChange::CheckHolders2(const NodeId& target) const {
  // throw / handle cases of lower number of group matrix nodes
  assert(old_matrix_.size() >= Parameters::node_group_size + 1U);  // FIXME
  assert(new_matrix_.size() >= Parameters::node_group_size + 1U);  // FIXME

  std::vector<NodeId> old_holders(Parameters::node_group_size + 1),
                      new_holders(Parameters::node_group_size + 1),
                      lost_nodes(lost_nodes_);
  std::partial_sort_copy(old_matrix_.begin(),
                         old_matrix_.end(),
                         old_holders.begin(),
                         old_holders.end(),
                         [target](const NodeId& lhs, const NodeId& rhs) {
                           return NodeId::CloserToTarget(lhs, rhs, target);
                         });
LOG(kInfo) << "after partial_sort_copy";
  for (auto i : old_holders)
    LOG(kInfo) << DebugId(i);


  std::partial_sort_copy(new_matrix_.begin(),
                         new_matrix_.end(),
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

  // remove taget == node ids
  std::remove(old_holders.begin(), old_holders.end(), target);
  old_holders.resize(Parameters::node_group_size);
  std::remove(new_holders.begin(), new_holders.end(), target);
  new_holders.resize(Parameters::node_group_size);
  std::remove(lost_nodes.begin(), lost_nodes.end(), target);

  CheckHoldersResult holders_result;
  // Old holders = Old 4 holder intersection lost nodes
  std::set_intersection(old_holders.begin(),
                        old_holders.end(),
                        lost_nodes.begin(),
                        lost_nodes.end(),
                        std::back_inserter(holders_result.old_holders),
                        [target](const NodeId& lhs, const NodeId& rhs) {
                          return NodeId::CloserToTarget(lhs, rhs, target);
                        });
 // new holders = new 4 holders - old 4 holders
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

  if (distance < radius_)
    return GroupRangeStatus::kInProximalRange;
  else
    return GroupRangeStatus::kOutwithRange;
}

bool MatrixChange::OldEqualsToNew() const {
  if (old_matrix_.size() != new_matrix_.size())
    return false;
  return std::equal(new_matrix_.begin(), new_matrix_.end(), old_matrix_.begin());
}

} // namespace testing

}  // namespace routing

}  // namespace maidsafe
