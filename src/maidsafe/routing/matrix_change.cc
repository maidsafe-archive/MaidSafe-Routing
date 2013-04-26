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


namespace maidsafe {

namespace routing {


MatrixChange::MatrixChange(const NodeId& /*this_node_id*/, std::vector<NodeId> /*old_matrix*/,
                           std::vector<NodeId> /*new_matrix*/) {
}

CheckHoldersResult MatrixChange::CheckHolders(const NodeId& /*target*/) {
  CheckHoldersResult holders_result;
  return holders_result;
}

bool MatrixChange::OldEqualsToNew() const {
  if (old_matrix_.size() != new_matrix_.size())
    return false;
  return std::equal(new_matrix_.begin(), new_matrix_.end(), old_matrix_.begin());
}

}  // namespace routing

}  // namespace maidsafe
