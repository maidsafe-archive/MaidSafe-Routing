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

#include <bitset>
#include <memory>
#include <numeric>
#include <vector>

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/test.h"

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/matrix_change.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {
namespace routing {
namespace test {

class MatrixChangeTest : public testing::Test {
 protected:
  MatrixChangeTest()
    : old_matrix_(),
      new_matrix_(),
      lost_nodes_(),
      kNodeId_(NodeId::kRandomId) {
    int old_matrix_size(20);

    old_matrix_.push_back(kNodeId_);
    new_matrix_.push_back(kNodeId_);
    for (auto i(0); i != old_matrix_size - 1; ++i) {
      old_matrix_.push_back(NodeId(NodeId::kRandomId));
      new_matrix_.push_back(old_matrix_.back());
    }

    lost_nodes_.push_back(new_matrix_.back());
    new_matrix_.pop_back();

    new_matrix_.push_back(NodeId(NodeId::kRandomId));
    new_nodes_.push_back(new_matrix_.back());
    new_matrix_.push_back(NodeId(NodeId::kRandomId));
    new_nodes_.push_back(new_matrix_.back());
  }

 protected:
  std::vector<NodeId> old_matrix_, new_matrix_, lost_nodes_, new_nodes_;
  const NodeId kNodeId_;
};

TEST_F(MatrixChangeTest, BEH_Constructor) {
  MatrixChange matrix_change(kNodeId_, old_matrix_, new_matrix_);

  for (auto i(0); i != 100; ++i) {
    auto target_id(GenerateUniqueRandomId(kNodeId_, 100));
    auto result(matrix_change.CheckHolders(target_id));
    EXPECT_EQ(result.proximity_status, GroupRangeStatus::kInRange);
  }

  for (auto i(0); i != 100; ++i) {
    NodeId target_id;
    bool unique(false);
    while (!unique) {
      target_id = NodeId(NodeId::kRandomId);
      unique = (std::find(old_matrix_.begin(), old_matrix_.end(), target_id) == old_matrix_.end())
                    && (std::find(new_matrix_.begin(), new_matrix_.end(), target_id)
                        == new_matrix_.end());
    }

    LOG(kInfo) << "test target id : " << DebugId(target_id);
    auto result(matrix_change.CheckHolders(target_id));
    std::partial_sort(new_matrix_.begin(),
                      new_matrix_.begin() + Parameters::node_group_size,
                      new_matrix_.end(),
                      [target_id](const NodeId& lhs, const NodeId& rhs) {
                        return NodeId::CloserToTarget(lhs, rhs, target_id);
                      });
    std::partial_sort(old_matrix_.begin(),
                      old_matrix_.begin() + Parameters::node_group_size,
                      old_matrix_.end(),
                      [target_id](const NodeId& lhs, const NodeId& rhs) {
                        return NodeId::CloserToTarget(lhs, rhs, target_id);
                      });

    std::vector<NodeId> new_holders(new_matrix_.begin(), new_matrix_.begin() +
                                        Parameters::node_group_size);
    std::vector<NodeId> old_holders(old_matrix_.begin(), old_matrix_.begin() +
                                        Parameters::node_group_size);

    LOG(kInfo) << "Old holders ";
    for (auto& i : old_holders)
      LOG(kInfo) << DebugId(i);

    LOG(kInfo) << "New holders ";
    for (auto& i : new_holders)
      LOG(kInfo) << DebugId(i);

    LOG(kInfo) << "lost nodes ";
    for (auto& i : lost_nodes_)
      LOG(kInfo) << DebugId(i);

    LOG(kInfo) << "Result new holders :  ";
    for (auto& i : result.new_holders) {
      LOG(kInfo) << DebugId(i);
      EXPECT_NE(std::find(new_holders.begin(), new_holders.end(), i), new_holders.end());
    }

    LOG(kInfo) << "Result old holders :  ";
    for (auto& i : result.old_holders) {
      LOG(kInfo) << DebugId(i);
      EXPECT_NE(std::find(old_holders.begin(), old_holders.end(), i), old_holders.end());
    }
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

