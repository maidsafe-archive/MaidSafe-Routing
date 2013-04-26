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

#ifndef MAIDSAFE_ROUTING_MATRIX_CHANGE_H_
#define MAIDSAFE_ROUTING_MATRIX_CHANGE_H_



#include "maidsafe/common/node_id.h"

namespace maidsafe {

namespace routing {

class GroupMatrix;

enum class GroupRangeStatus {
  kInRange,
  kInProximalRange,
  kOutwithRange
};

struct CheckHoldersResult {
  std::vector<NodeId> new_holders;
  std::vector<NodeId> old_holders;
  routing::GroupRangeStatus proximity_status;
};

class MatrixChange {
 public:
  MatrixChange(const MatrixChange&);
  MatrixChange& operator=(const MatrixChange&);
  MatrixChange(MatrixChange&& other);
  MatrixChange& operator=(MatrixChange&& other);

  CheckHoldersResult CheckHolders(const NodeId& target);
  bool OldEqualsToNew() const;
  friend class GroupMatrix;

 private:
  MatrixChange(const NodeId& this_node_id, std::vector<NodeId> old_matrix,
               const std::vector<NodeId> new_matrix);

  static const uint16_t close_count_, proximal_count_;
  const NodeId kNodeId_;
  std::vector<NodeId> old_matrix_, new_matrix_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MATRIX_CHANGE_H_
