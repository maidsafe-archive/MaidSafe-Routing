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

#ifndef MAIDSAFE_ROUTING_MATRIX_CHANGE_H_
#define MAIDSAFE_ROUTING_MATRIX_CHANGE_H_

#include <set>
#include <string>
#include <vector>

#include "maidsafe/common/config.h"
#include "maidsafe/common/crypto.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"

namespace maidsafe {

namespace routing {

class RoutingTable;
class GroupMatrix;

namespace test {
class MatrixChangeTest_BEH_CheckHolders_Test;
class SingleMatrixChangeTest_BEH_ChoosePmidNode_Test;
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
  MatrixChange();
  MatrixChange(const MatrixChange& other);
  MatrixChange(MatrixChange&& other);
  MatrixChange& operator=(MatrixChange other);
  MatrixChange(NodeId this_node_id, const std::vector<NodeId>& old_matrix,
               const std::vector<NodeId>& new_matrix);
  bool OldEqualsToNew() const;

  CheckHoldersResult CheckHolders(const NodeId& target) const;
  NodeId ChoosePmidNode(const std::set<NodeId>& online_pmids, const NodeId& target) const;
  std::vector<NodeId> lost_nodes() const { return lost_nodes_; }
  std::vector<NodeId> new_nodes() const { return new_nodes_; }
  void Print();

  friend void swap(MatrixChange& lhs, MatrixChange& rhs) MAIDSAFE_NOEXCEPT;
  friend class GroupMatrix;
  friend class RoutingTable;
  friend class test::MatrixChangeTest_BEH_CheckHolders_Test;
  friend class test::SingleMatrixChangeTest_BEH_ChoosePmidNode_Test;
  friend class test::GroupMatrixTest_BEH_EmptyMatrix_Test;

 private:

  NodeId node_id_;
  std::vector<NodeId> old_matrix_, new_matrix_, lost_nodes_, new_nodes_;
  crypto::BigInt radius_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MATRIX_CHANGE_H_
