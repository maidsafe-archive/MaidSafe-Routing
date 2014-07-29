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

#ifndef MAIDSAFE_ROUTING_CLOSE_NODES_CHANGE_H_
#define MAIDSAFE_ROUTING_CLOSE_NODES_CHANGE_H_

#include <set>
#include <string>
#include <vector>

#include "maidsafe/common/config.h"
#include "maidsafe/common/crypto.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"

namespace maidsafe {

namespace routing {

namespace test {
class CloseNodesChangeTest_BEH_CheckHolders_Test;
class SingleCloseNodesChangeTest_BEH_ChoosePmidNode_Test;
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

class CloseNodesChange {
 public:
  CloseNodesChange();
  CloseNodesChange(const CloseNodesChange& other);
  CloseNodesChange(CloseNodesChange&& other);
  CloseNodesChange& operator=(CloseNodesChange other);

  CheckHoldersResult CheckHolders(const NodeId& target) const;
  NodeId ChoosePmidNode(const std::set<NodeId>& online_pmids, const NodeId& target) const;
  std::vector<NodeId> lost_nodes() const;
  std::vector<NodeId> new_nodes() const;
  void Print();

  friend void swap(CloseNodesChange& lhs, CloseNodesChange& rhs) MAIDSAFE_NOEXCEPT;
  template <typename NodeType>
  friend class RoutingTable;
  friend class test::CloseNodesChangeTest_BEH_CheckHolders_Test;
  friend class test::SingleCloseNodesChangeTest_BEH_ChoosePmidNode_Test;

 private:
  CloseNodesChange(NodeId this_node_id, const std::vector<NodeId>& old_close_nodes,
                   const std::vector<NodeId>& new_close_nodes);
  bool OldEqualsToNew() const;

  NodeId node_id_;
  std::vector<NodeId> old_close_nodes_, new_close_nodes_, lost_nodes_, new_nodes_;
  crypto::BigInt radius_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_CLOSE_NODES_CHANGE_H_
