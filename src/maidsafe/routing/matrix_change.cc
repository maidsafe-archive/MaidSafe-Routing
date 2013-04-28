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


MatrixChange::MatrixChange(const NodeId& this_node_id, std::vector<NodeId> old_matrix,
                           std::vector<NodeId> new_matrix)
    : kNodeId_(this_node_id),
      old_matrix_(old_matrix),
      new_matrix_(new_matrix),
      lost_nodes_() {
  std::sort(old_matrix_.begin(),
            old_matrix_.end(),
            [target](const NodeId& lhs, const NodeId& rhs) {
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
}

CheckHoldersResult MatrixChange::CheckHolders(const NodeId& target) {
  // throw / handle cases of lower number of group matrix nodes
  // old holders = Old 4 holder intersection lost nodes
  // new holders = new 4 holders - old 4 holders
  CheckHoldersResult holders_result;
  std::lock_guard<std::mutex> lock(mutex_);
  std::partial_sort(old_matrix_.begin(),
                    old_matrix_.end() + Paramaters::node_group_size,
                    old_matrix_.end(),
                    [target](const NodeId& lhs, const NodeId& rhs) {
                      return NodeId::CloserToTarget(lhs, rhs, target);
                    });
  std::partial_sort(new_matrix_.begin(),
                    new_matrix_.end() + Paramaters::node_group_size,
                    new_matrix_.end(),
                    [target](const NodeId& lhs, const NodeId& rhs) {
                      return NodeId::CloserToTarget(lhs, rhs, target);
                    });

  std::vector<NodeId> old_holders(old_matrix_.begin(),
                                  old_matrix_.end() + Paramaters::node_group_size);

  std::vector<NodeId> new_holders(new_matrix_.begin(),
                                  new_matrix_.end() + Paramaters::node_group_size);

  std::sort(lost_nodes_.begin(),
            lost_nodes_.end(),
            [target](const NodeId& lhs, const NodeId& rhs) {
              return NodeId::CloserToTarget(lhs, rhs, target);
            });

  std::set_intersection(old_holders.begin(),
                        old_holders.end(),
                        lost_nodes_.begin(),
                        lost_nodes_.end(),
                        std::back_inserter(holders_result.old_holders),
                        [target](const NodeId& lhs, const NodeId& rhs) {
                          return NodeId::CloserToTarget(lhs, rhs, target);
                        });

  std::set_difference(new_holders.begin(),
                      new_holders.end(),
                      old_holders.begin(),
                      old_holders.end(),
                      std::back_inserter(holders_result.new_holders),
                      [target](const NodeId& lhs, const NodeId& rhs) {
                        return NodeId::CloserToTarget(lhs, rhs, target);
                      });
  // handle range for this node

  return holders_result;
}

bool MatrixChange::OldEqualsToNew() const {
  if (old_matrix_.size() != new_matrix_.size())
    return false;
  return std::equal(new_matrix_.begin(), new_matrix_.end(), old_matrix_.begin());
}

}  // namespace routing

}  // namespace maidsafe
