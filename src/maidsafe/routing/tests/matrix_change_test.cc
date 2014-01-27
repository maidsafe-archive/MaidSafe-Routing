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
#include <map>
#include <memory>
#include <numeric>
#include <random>
#include <set>
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
  MatrixChangeTest() : old_matrix_(), new_matrix_(), lost_nodes_(), kNodeId_(NodeId::kRandomId) {
    int old_matrix_size(20);

    old_matrix_.push_back(kNodeId_);
    new_matrix_.push_back(kNodeId_);
    for (auto i(0); i != old_matrix_size - 1; ++i) {
      old_matrix_.push_back(NodeId(NodeId::kRandomId));
      new_matrix_.push_back(old_matrix_.back());
    }
    // lost 1 node, new 1 node  FIXME(Prakash)
    lost_nodes_.push_back(new_matrix_.back());
    new_matrix_.pop_back();
    new_matrix_.push_back(NodeId(NodeId::kRandomId));
    new_nodes_.push_back(new_matrix_.back());
  }

 public:
  CheckHoldersResult CheckHolders(const NodeId& target) {
    CheckHoldersResult holders_result;
    // Radius
    std::sort(new_matrix_.begin(), new_matrix_.end(),
              [this](const NodeId & lhs, const NodeId & rhs) {
      return NodeId::CloserToTarget(lhs, rhs, this->kNodeId_);
    });
    NodeId fcn_distance;
    if (new_matrix_.size() >= Parameters::closest_nodes_size)
      fcn_distance = kNodeId_ ^ new_matrix_[Parameters::closest_nodes_size - 1];
    else
      fcn_distance = kNodeId_ ^ (NodeId(NodeId::kMaxId));
    crypto::BigInt radius(
        crypto::BigInt((fcn_distance.ToStringEncoded(NodeId::EncodingType::kHex) + 'h').c_str()) *
        Parameters::proximity_factor);

    // sort by target
    std::sort(old_matrix_.begin(), old_matrix_.end(),
              [target](const NodeId & lhs,
                       const NodeId & rhs) { return NodeId::CloserToTarget(lhs, rhs, target); });

    std::sort(new_matrix_.begin(), new_matrix_.end(),
              [target](const NodeId & lhs,
                       const NodeId & rhs) { return NodeId::CloserToTarget(lhs, rhs, target); });

    // Remove taget == node ids and adjust holder size
    size_t node_group_size_adjust(Parameters::node_group_size + 1U);
    size_t old_holders_size = std::min(old_matrix_.size(), node_group_size_adjust);
    size_t new_holders_size = std::min(new_matrix_.size(), node_group_size_adjust);

    std::vector<NodeId> all_old_holders(old_matrix_.begin(),
                                        old_matrix_.begin() + old_holders_size);
    std::vector<NodeId> all_new_holders(new_matrix_.begin(),
                                        new_matrix_.begin() + new_holders_size);
    std::vector<NodeId> all_lost_nodes(lost_nodes_);

    all_old_holders.erase(std::remove(all_old_holders.begin(), all_old_holders.end(), target),
                          all_old_holders.end());
    if (all_old_holders.size() > Parameters::node_group_size) {
      all_old_holders.pop_back();
      assert(all_old_holders.size() == Parameters::node_group_size);
    }
    all_new_holders.erase(std::remove(all_new_holders.begin(), all_new_holders.end(), target),
                          all_new_holders.end());
    if (all_new_holders.size() > Parameters::node_group_size) {
      all_new_holders.pop_back();
      assert(all_new_holders.size() == Parameters::node_group_size);
    }
    all_lost_nodes.erase(std::remove(all_lost_nodes.begin(), all_lost_nodes.end(), target),
                         all_lost_nodes.end());
    // Range
    if (target == kNodeId_)
      holders_result.proximity_status = GroupRangeStatus::kOutwithRange;
    if (std::find(all_new_holders.begin(), all_new_holders.end(), kNodeId_) !=
        all_new_holders.end()) {
      holders_result.proximity_status = GroupRangeStatus::kInRange;
    } else {
      NodeId distance_id(kNodeId_ ^ target);
      crypto::BigInt distance(
          (distance_id.ToStringEncoded(NodeId::EncodingType::kHex) + 'h').c_str());
      holders_result.proximity_status = (distance < radius) ? GroupRangeStatus::kInProximalRange
                                                            : GroupRangeStatus::kOutwithRange;
    }
    // Old holders = All Old holder ∩ Lost nodes
    for (const auto& i : all_lost_nodes)
      if (std::find(all_old_holders.begin(), all_old_holders.end(), i) != all_old_holders.end())
        holders_result.old_holders.push_back(i);

    // New holders = All New holders - Old holders
    for (const auto& i : all_old_holders) {
      all_new_holders.erase(std::remove(all_new_holders.begin(), all_new_holders.end(), i),
                            all_new_holders.end());
    }

    holders_result.new_holders = all_new_holders;

    if (GroupRangeStatus::kInRange != holders_result.proximity_status) {
      holders_result.new_holders.clear();
      holders_result.old_holders.clear();
    }
    return holders_result;
  }

 protected:
  std::vector<NodeId> old_matrix_, new_matrix_, lost_nodes_, new_nodes_;
  const NodeId kNodeId_;
};

TEST_F(MatrixChangeTest, BEH_CheckHolders) {
  MatrixChange matrix_change(kNodeId_, old_matrix_, new_matrix_);

  for (auto i(0); i != 1000; ++i) {
    NodeId target_id(NodeId::kRandomId);
    auto result(matrix_change.CheckHolders(target_id));
    auto test_result(CheckHolders(target_id));
    ASSERT_EQ(result.proximity_status, test_result.proximity_status);
    ASSERT_EQ(result.new_holders, test_result.new_holders);
    ASSERT_EQ(result.old_holders, test_result.old_holders);
  }
}

void Choose(const std::set<NodeId>& online_pmids,
            const NodeId& kTarget,
            const std::vector<MatrixChange>& owners,
            int owner_count,
            int online_pmid_count) {
  // This test is only valid where 'owner_count' <= 'Parameters::node_group_size'.
  ASSERT_LE(owner_count, Parameters::node_group_size);

  // Create a map of chosen nodes with a count of how many times each was selected by the various
  // owning nodes.
  std::map<NodeId, int> chosens;
  for (int i(0); i != owner_count; ++i) {
    std::set<NodeId> copied_online_pmids;
    auto last_itr(std::begin(online_pmids));
    std::advance(last_itr, online_pmid_count);
    copied_online_pmids.insert(std::begin(online_pmids), last_itr);
    auto chosen(owners[i].ChoosePmidNode(copied_online_pmids, kTarget));
    ++chosens[chosen];
  }

  // Calculate the maximum and minimum values in 'chosen'
  int max_value(0), min_value(std::begin(chosens)->second);
  for (const auto& chosen : chosens) {
    if (chosen.second < min_value)
      min_value = chosen.second;
    if (chosen.second > max_value)
      max_value = chosen.second;
  }

  // If 'owner_count' > 'online_pmid_count' there should be 'online_pmid_count' entries in 'chosens'
  // and the maximum value should be within 1 of the minimum value.  If 'owner_count' <=
  // 'online_pmid_count' there should be 'owner_count' entries in 'chosens', each with a value of 1.
  if (owner_count > online_pmid_count) {
    EXPECT_EQ(chosens.size(), static_cast<size_t>(online_pmid_count));
    ASSERT_LE(min_value, max_value);
    EXPECT_LE(max_value - min_value, 1);
  } else {
    EXPECT_EQ(chosens.size(), static_cast<size_t>(owner_count));
    EXPECT_EQ(min_value, 1);
    EXPECT_EQ(max_value, 1);
  }
}

TEST(SingleMatrixChangeTest, BEH_ChoosePmidNode) {
  std::vector<NodeId> old_matrix, new_matrix;
  const auto kGroupSize(Parameters::node_group_size);
  for (int i(0); i != kGroupSize * 5; ++i)
    new_matrix.emplace_back(NodeId::kRandomId);
  const NodeId kTarget(NodeId::kRandomId);

  // Get the 5 closest to 'kTarget' as the owners.
  std::sort(std::begin(new_matrix), std::end(new_matrix),
            [&kTarget](const NodeId& lhs, const NodeId& rhs) {
    return NodeId::CloserToTarget(lhs, rhs, kTarget);
  });
  std::vector<MatrixChange> owners;
  for (int i(0); i != kGroupSize + 1; ++i)
    owners.push_back(MatrixChange(new_matrix[i], old_matrix, new_matrix));

  // Shuffle 'new_matrix'.
  std::random_device random_device;
  std::mt19937 random_functor(random_device());
  std::shuffle(std::begin(new_matrix), std::end(new_matrix), random_functor);

  // 0 online_pmids should throw.
  std::set<NodeId> online_pmids;
  EXPECT_THROW(owners[0].ChoosePmidNode(online_pmids, kTarget), maidsafe_error);

  for (int i(0); i != kGroupSize + 2; ++i)
    online_pmids.insert(NodeId(NodeId::kRandomId));

  // Run tests.
  Choose(online_pmids, kTarget, owners, kGroupSize - 1, kGroupSize - 2);
  Choose(online_pmids, kTarget, owners, kGroupSize - 1, kGroupSize - 1);
  Choose(online_pmids, kTarget, owners, kGroupSize - 1, kGroupSize);
  Choose(online_pmids, kTarget, owners, kGroupSize - 1, kGroupSize + 1);
  Choose(online_pmids, kTarget, owners, kGroupSize - 1, kGroupSize + 2);
  Choose(online_pmids, kTarget, owners, kGroupSize, kGroupSize - 2);
  Choose(online_pmids, kTarget, owners, kGroupSize, kGroupSize - 1);
  Choose(online_pmids, kTarget, owners, kGroupSize, kGroupSize);
  Choose(online_pmids, kTarget, owners, kGroupSize, kGroupSize + 1);
  Choose(online_pmids, kTarget, owners, kGroupSize, kGroupSize + 2);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
