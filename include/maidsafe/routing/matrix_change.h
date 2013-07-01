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


#include <vector>

#include "maidsafe/common/crypto.h"
#include "maidsafe/common/node_id.h"

namespace maidsafe {

namespace routing {

class RoutingTable;
class GroupMatrix;

namespace test {
  class MatrixChangeTest_BEH_Constructor_Test;
  class GroupMatrixTest_BEH_EmptyMatrix_Test;
}

enum class GroupRangeStatus {
  kInRange,
  kInProximalRange,
  kOutwithRange
};

struct CheckHoldersResult {
  std::vector<NodeId> new_holders;  // New holders = All 4 New holders - All 4 Old holders
  std::vector<NodeId> old_holders;  // Old holders = All 4 Old holder ∩ All Lost nodes
  routing::GroupRangeStatus proximity_status;
};

class MatrixChange {
 public:
  MatrixChange(const MatrixChange&);
  MatrixChange& operator=(const MatrixChange&);
  MatrixChange(MatrixChange&& other);

  CheckHoldersResult CheckHolders(const NodeId& target) const;

  friend class GroupMatrix;
  friend class RoutingTable;
  friend class test::MatrixChangeTest_BEH_Constructor_Test;
  friend class test::GroupMatrixTest_BEH_EmptyMatrix_Test;

 private:
  MatrixChange(const NodeId& this_node_id, const std::vector<NodeId>& old_matrix,
               const std::vector<NodeId>& new_matrix);
  bool OldEqualsToNew() const;

  static const uint16_t close_count_, proximal_count_;
  const NodeId kNodeId_;
  const std::vector<NodeId> kOldMatrix_, kNewMatrix_, kLostNodes_;
  const crypto::BigInt kRadius_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MATRIX_CHANGE_H_
