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

#include "maidsafe/routing/close_nodes_change.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {
namespace routing {
namespace test {

class CloseNodesChangeTest : public testing::Test {
 protected:
  CloseNodesChangeTest() : old_close_nodes_(), new_close_nodes_(),
      kNodeId_(NodeId::IdType::kRandomId) {
    int old_close_nodes_size(20);

    old_close_nodes_.push_back(kNodeId_);
    new_close_nodes_.push_back(kNodeId_);
    for (auto i(0); i != old_close_nodes_size - 1; ++i) {
      old_close_nodes_.push_back(NodeId(NodeId::IdType::kRandomId));
      new_close_nodes_.push_back(old_close_nodes_.back());
    }
  }

 public:
  CheckHoldersResult CheckHolders(const NodeId& target,
      std::vector<NodeId>& old_close_nodes, std::vector<NodeId>& new_close_nodes) {
    CheckHoldersResult holders_result;
    // Radius
    std::sort(new_close_nodes.begin(), new_close_nodes.end(),
              [this](const NodeId & lhs, const NodeId & rhs) {
      return NodeId::CloserToTarget(lhs, rhs, this->kNodeId_);
    });
    NodeId fcn_distance;
    if (new_close_nodes.size() >= Parameters::closest_nodes_size)
      fcn_distance = kNodeId_ ^ new_close_nodes[Parameters::closest_nodes_size - 1];
    else
      fcn_distance = kNodeId_ ^ (NodeId(NodeId::IdType::kMaxId));
    crypto::BigInt radius(
        crypto::BigInt((fcn_distance.ToStringEncoded(NodeId::EncodingType::kHex) + 'h').c_str()) *
        Parameters::proximity_factor);

    // sort by target
    std::sort(old_close_nodes.begin(), old_close_nodes.end(),
              [target](const NodeId & lhs,
                       const NodeId & rhs) { return NodeId::CloserToTarget(lhs, rhs, target); });

    std::sort(new_close_nodes.begin(), new_close_nodes.end(),
              [target](const NodeId & lhs,
                       const NodeId & rhs) { return NodeId::CloserToTarget(lhs, rhs, target); });

    // Remove taget == node ids and adjust holder size

    size_t group_size_adjust(Parameters::group_size + 1U);
    size_t old_holders_size = std::min(old_close_nodes_.size(), group_size_adjust);
    size_t new_holders_size = std::min(new_close_nodes_.size(), group_size_adjust);

    std::vector<NodeId> all_old_holders(old_close_nodes_.begin(),
                                        old_close_nodes_.begin() + old_holders_size);
    std::vector<NodeId> all_new_holders(new_close_nodes_.begin(),
                                        new_close_nodes_.begin() + new_holders_size);
    std::vector<NodeId> all_lost_nodes;

    for (auto& drop_node : all_old_holders)
      if (std::find(all_new_holders.begin(), all_new_holders.end(), drop_node) ==
          all_new_holders.end())
        all_lost_nodes.push_back(drop_node);

    all_old_holders.erase(std::remove(all_old_holders.begin(), all_old_holders.end(), target),
                          all_old_holders.end());
    if (all_old_holders.size() > Parameters::group_size) {
      all_old_holders.pop_back();
      assert(all_old_holders.size() == Parameters::group_size);
    }
    all_new_holders.erase(std::remove(all_new_holders.begin(), all_new_holders.end(), target),
                          all_new_holders.end());
    if (all_new_holders.size() > Parameters::group_size) {
      all_new_holders.pop_back();
      assert(all_new_holders.size() == Parameters::group_size);
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

  void DoCheckHoldersTest(const CloseNodesChange& close_nodes_change) {
    NodeId target_id(NodeId::IdType::kRandomId);
    auto result(close_nodes_change.CheckHolders(target_id));
    auto test_result(CheckHolders(target_id, old_close_nodes_, new_close_nodes_));
    ASSERT_EQ(result.proximity_status, test_result.proximity_status);
    ASSERT_EQ(result.new_holders, test_result.new_holders);
    ASSERT_EQ(result.old_holders, test_result.old_holders);
//     if ((result.proximity_status != test_result.proximity_status) ||
//         (result.new_holders != test_result.new_holders) ||
//         (result.old_holders != test_result.old_holders)) {
// //         LOG(kVerbose) << result.proximity_status << " , " << test_result.proximity_status;
//       LOG(kVerbose) << "target : " << HexSubstr(target_id.string());
// //       LOG(kVerbose) << "dropping : " << HexSubstr(drop_node.string());
//       LOG(kVerbose) << "result.new_holders : ";
//       for (auto& holder : result.new_holders)
//         LOG(kVerbose) << "    ----    " << HexSubstr(holder.string());
//       LOG(kVerbose) << "test_result.new_holders : ";
//       for (auto& holder : test_result.new_holders)
//         LOG(kVerbose) << "    ----    " << HexSubstr(holder.string());
//       LOG(kVerbose) << "result.old_holders : ";
//       for (auto& holder : result.old_holders)
//         LOG(kVerbose) << "    ----    " << HexSubstr(holder.string());
//       LOG(kVerbose) << "test_result.old_holders : ";
//       for (auto& holder : test_result.old_holders)
//         LOG(kVerbose) << "    ----    " << HexSubstr(holder.string());
//       LOG(kVerbose) << "new_close_nodes containing : ";
//       for (auto& holder : new_close_nodes_)
//         LOG(kVerbose) << "    ----    " << HexSubstr(holder.string());
//       LOG(kVerbose) << "old_close_nodes containing : ";
//       for (auto& holder : old_close_nodes_)
//         LOG(kVerbose) << "    ----    " << HexSubstr(holder.string());
//       ASSERT_EQ(0, 1);
//     }
  }

 protected:
  std::vector<NodeId> old_close_nodes_, new_close_nodes_;
  const NodeId kNodeId_;
};

TEST_F(CloseNodesChangeTest, BEH_CheckHolders) {
  CloseNodesChange close_nodes_change(kNodeId_, old_close_nodes_, new_close_nodes_);
  for (auto i(0); i != 1000; ++i)
    DoCheckHoldersTest(close_nodes_change);
}

void Choose(const std::set<NodeId>& online_pmids,
            const NodeId& kTarget,
            const std::vector<CloseNodesChange>& owners,
            int owner_count,
            int online_pmid_count) {
  // This test is only valid where 'owner_count' <= 'Parameters::group_size'.
  ASSERT_LE(owner_count, Parameters::group_size);

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

TEST(SingleCloseNodesChangeTest, BEH_ChoosePmidNode) {
  std::vector<NodeId> old_close_nodes, new_close_nodes;
  const auto kGroupSize(Parameters::group_size);
  for (int i(0); i != kGroupSize * 5; ++i)
    new_close_nodes.emplace_back(NodeId::IdType::kRandomId);
  const NodeId kTarget(NodeId::IdType::kRandomId);

  // Get the 5 closest to 'kTarget' as the owners.
  std::sort(std::begin(new_close_nodes), std::end(new_close_nodes),
            [&kTarget](const NodeId& lhs, const NodeId& rhs) {
    return NodeId::CloserToTarget(lhs, rhs, kTarget);
  });
  std::vector<CloseNodesChange> owners;
  for (int i(0); i != kGroupSize + 1; ++i)
    owners.push_back(CloseNodesChange(new_close_nodes[i], old_close_nodes, new_close_nodes));

  // Shuffle 'new_close_nodes'.
  std::random_device random_device;
  std::mt19937 random_functor(random_device());
  std::shuffle(std::begin(new_close_nodes), std::end(new_close_nodes), random_functor);

  // 0 online_pmids should throw.
  std::set<NodeId> online_pmids;
  EXPECT_THROW(owners[0].ChoosePmidNode(online_pmids, kTarget), maidsafe_error);

  for (int i(0); i != kGroupSize + 2; ++i)
    online_pmids.insert(NodeId(NodeId::IdType::kRandomId));

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

TEST_F(CloseNodesChangeTest, BEH_SmallSizeRoutingTable) {
  NodeId node_id(NodeId::IdType::kRandomId);
  RoutingTableChangeFunctor routing_table_change_functor(
      [&](const RoutingTableChange& routing_table_change) {
        EXPECT_TRUE(routing_table_change.close_nodes_change != nullptr);
        EXPECT_EQ(routing_table_change.close_nodes_change->new_nodes().size(), 1);
        EXPECT_TRUE(routing_table_change.close_nodes_change->lost_nodes().empty());
      });
  RoutingTable<VaultNode> routing_table(node_id, asymm::GenerateKeyPair());
  routing_table.InitialiseFunctors(routing_table_change_functor);
  while (routing_table.size() < Parameters::closest_nodes_size) {
    auto node(MakeNode());
    EXPECT_TRUE(routing_table.AddNode(node));
  }
}

TEST_F(CloseNodesChangeTest, BEH_FullSizeRoutingTable) {
  NodeId node_id(NodeId::IdType::kRandomId);
  std::set<NodeId, std::function<bool(const NodeId& lhs, const NodeId& rhs)>>
      new_ids([node_id](const NodeId& lhs, const NodeId& rhs) {
        return NodeId::CloserToTarget(lhs, rhs, node_id);
      });
  NodeInfo new_node;
  NodeId removed;
  RoutingTable<VaultNode> routing_table(node_id, asymm::GenerateKeyPair());
  RoutingTableChangeFunctor routing_table_change_functor(
      [&](const RoutingTableChange& routing_table_change) {
        if (routing_table.size() <= Parameters::closest_nodes_size) {
          EXPECT_TRUE(routing_table_change.close_nodes_change != nullptr);
          if (routing_table_change.insertion) {
            EXPECT_EQ(routing_table_change.close_nodes_change->new_nodes().size(), 1);
            EXPECT_TRUE(routing_table_change.close_nodes_change->lost_nodes().empty());
          } else {
            EXPECT_TRUE(routing_table_change.close_nodes_change->new_nodes().empty());
            EXPECT_EQ(routing_table_change.close_nodes_change->lost_nodes().size(), 1);
            EXPECT_EQ(routing_table_change.close_nodes_change->lost_nodes().at(0), removed);
            new_ids.erase(removed);
          }
        } else {
          auto iter(new_ids.begin());
          std::advance(iter, Parameters::closest_nodes_size - 1);
          if (routing_table_change.insertion) {
            if (NodeId::CloserToTarget(new_node.id, *iter, node_id)) {
              EXPECT_TRUE(routing_table_change.close_nodes_change != nullptr);
              EXPECT_EQ(routing_table_change.close_nodes_change->new_nodes().size(), 1);
              EXPECT_EQ(routing_table_change.close_nodes_change->lost_nodes().size(), 1);
              std::advance(iter, 1);
              EXPECT_EQ(routing_table_change.close_nodes_change->lost_nodes().at(0), *iter);
            }
          } else {
            if (!NodeId::CloserToTarget(*iter, removed, node_id)) {
              EXPECT_TRUE(routing_table_change.close_nodes_change != nullptr);
              EXPECT_EQ(routing_table_change.close_nodes_change->new_nodes().size(), 1);
              std::advance(iter, 1);
              EXPECT_EQ(routing_table_change.close_nodes_change->new_nodes().at(0), *iter);
              EXPECT_EQ(routing_table_change.close_nodes_change->lost_nodes().size(), 1);
              EXPECT_EQ(routing_table_change.close_nodes_change->lost_nodes().at(0), removed);
            }
          }
          if (!routing_table_change.removed.node.id.IsZero())
            new_ids.erase(routing_table_change.removed.node.id);
        }
        EXPECT_LE(new_ids.size(), Parameters::max_routing_table_size);
      });
  routing_table.InitialiseFunctors(routing_table_change_functor);
  auto iterations(200);
  while (iterations-- != 0) {
    new_node = MakeNode();
    if (routing_table.CheckNode(new_node)) {
      new_ids.insert(new_node.id);
      EXPECT_TRUE(routing_table.AddNode(new_node));
    }
  }
  while (routing_table.size() > 0) {
    auto random_index(RandomUint32() % new_ids.size());
    auto iter(std::begin(new_ids));
    std::advance(iter, random_index);
    removed = *iter;
    routing_table.DropNode(*iter, true);
    new_ids.erase(iter);
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
