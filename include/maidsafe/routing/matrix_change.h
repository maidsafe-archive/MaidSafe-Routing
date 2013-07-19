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
  class MatrixChangeTest_BEH_CheckHolders_Test;
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

struct PmidNodeStatus {
  std::vector<NodeId> nodes_up, nodes_down;
};

class MatrixChange {
 public:
  MatrixChange(const MatrixChange&);
  MatrixChange& operator=(const MatrixChange&);
  MatrixChange(MatrixChange&& other);

  CheckHoldersResult CheckHolders(const NodeId& target) const;
  PmidNodeStatus CheckPmidNodeStatus(const std::vector<NodeId>& pmid_nodes) const;

  friend class GroupMatrix;
  friend class RoutingTable;
  friend class test::MatrixChangeTest_BEH_CheckHolders_Test;
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
