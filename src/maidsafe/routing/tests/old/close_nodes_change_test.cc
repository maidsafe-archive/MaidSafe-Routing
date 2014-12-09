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

#include <bitset>
#include <map>
#include <memory>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <vector>

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"

#include "maidsafe/common/log.h"
#include "maidsafe/common/Address.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/close_nodes_change.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {
namespace routing {
namespace test {

std::pair<Address, NodeId> ParseString(std::string input) {
  Address old_Address, new_node_id;
  {
    std::istringstream istringstream{input};
    cereal::JSONInputArchive archive{istringstream};
    std::string node_str;
    try {
      archive(cereal::make_nvp("vaultRemoved", node_str));
      old_Address = Address(node_str, NodeId::EncodingType::kHex);
    } catch (const std::exception& e) {
    }
    try {
      archive(cereal::make_nvp("vaultAdded", node_str));
      new_Address = Address(node_str, NodeId::EncodingType::kHex);
    } catch (const std::exception& e) {
    }
  }
  return std::make_pair(old_Address, new_node_id);
}

class CloseNodesChangeTest : public testing::Test {
 protected:
  CloseNodesChangeTest()
      : old_close_nodes_(), new_close_nodes_(), kAddress_(RandomString(Address::kSize)) {
    int old_close_nodes_size(20);

    //    old_close_nodes_.push_back(kAddress_);
    //    new_close_nodes_.push_back(kAddress_);
    for (auto i(0); i != old_close_nodes_size - 1; ++i) {
      old_close_nodes_.push_back(Address(RandomString(Address::kSize)));
      new_close_nodes_.push_back(old_close_nodes_.back());
    }
  }

 public:
  CheckHoldersResult CheckHolders(const Address& target, std::vector<Address>& old_close_nodes,
                                  std::vector<Address>& new_close_nodes) {
    std::sort(new_close_nodes.begin(), new_close_nodes.end(),
              [&](const Address& lhs, const Address& rhs) {
      return Address::CloserToTarget(lhs, rhs, target);
    });
    std::sort(old_close_nodes.begin(), old_close_nodes.end(),
              [&](const Address& lhs, const Address& rhs) {
      return Address::CloserToTarget(lhs, rhs, target);
    });
    unsigned int new_node_tail(Parameters::group_size), old_node_tail(Parameters::group_size);
    if (new_close_nodes.front() != target)
      --new_node_tail;
    if (old_close_nodes.front() != target)
      --old_node_tail;
    bool now_in_range(
        Address::CloserToTarget(this->kNodeId_, new_close_nodes[new_node_tail], target));
    bool was_in_range(
        Address::CloserToTarget(this->kNodeId_, old_close_nodes[old_node_tail], target));
    LOG(kVerbose) << "now_in_range : " << std::boolalpha << now_in_range
                  << " was_in_range : " << was_in_range;
    /*    // kAddress_ is included in the new_close_nodes_
        if (this->kAddress_ == new_close_nodes[new_node_tail])
          now_in_range = true;
    */
    CheckHoldersResult holders_result;
    holders_result.proximity_status = GroupRangeStatus::kOutwithRange;
    if (now_in_range)
      holders_result.proximity_status = GroupRangeStatus::kInRange;

    if (now_in_range)
      --new_node_tail;
    if (was_in_range)
      --old_node_tail;
    std::vector<Address> diff_new_holders;
    std::for_each(std::begin(new_close_nodes), new_close_nodes.begin() + new_node_tail + 1,
                  [&](const Address& new_holder) {
      if (std::find(std::begin(old_close_nodes), old_close_nodes.begin() + old_node_tail + 1,
                    new_holder) == (old_close_nodes.begin() + old_node_tail + 1))
        diff_new_holders.push_back(new_holder);
    });
    if (diff_new_holders.size() > 0)
      holders_result.new_holder = diff_new_holders.front();
    return holders_result;
    /*
        // Radius
        std::sort(new_close_nodes.begin(), new_close_nodes.end(),
                  [this](const Address& lhs, const Address& rhs) {
          return Address::CloserToTarget(lhs, rhs, this->kNodeId_);
        });
        Address fcn_distance;
        if (new_close_nodes.size() >= Parameters::closest_nodes_size)
          fcn_distance = kAddress_ ^ new_close_nodes[Parameters::closest_nodes_size - 1];
        else
          fcn_distance = kAddress_ ^ NodeInNthBucket(kNodeId_, Parameters::closest_nodes_size);
        crypto::BigInt radius(
            crypto::BigInt((fcn_distance.ToStringEncoded(Address::EncodingType::kHex) +
       'h').c_str())
       *
            Parameters::proximity_factor);

        // sort by target
        std::sort(old_close_nodes.begin(), old_close_nodes.end(),
                  [target](const Address& lhs,
                           const Address& rhs) { return NodeId::CloserToTarget(lhs, rhs, target);
       });

        std::sort(new_close_nodes.begin(), new_close_nodes.end(),
                  [target](const Address& lhs,
                           const Address& rhs) { return NodeId::CloserToTarget(lhs, rhs, target);
       });

        // Remove taget == node ids and adjust holder size

        size_t group_size_adjust(Parameters::group_size + 1U);
        size_t old_holders_size = std::min(old_close_nodes_.size(), group_size_adjust);
        size_t new_holders_size = std::min(new_close_nodes_.size(), group_size_adjust);

        std::vector<Address> all_old_holders(old_close_nodes_.begin(),
                                            old_close_nodes_.begin() + old_holders_size);
        std::vector<Address> all_new_holders(new_close_nodes_.begin(),
                                            new_close_nodes_.begin() + new_holders_size);
        std::vector<Address> all_lost_nodes;

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
        if (target == kAddress_)
          holders_result.proximity_status = GroupRangeStatus::kOutwithRange;
        if (std::find(all_new_holders.begin(), all_new_holders.end(), kAddress_) !=
            all_new_holders.end()) {
          holders_result.proximity_status = GroupRangeStatus::kInRange;
        } else {
          Address distance_id(kNodeId_ ^ target);
          crypto::BigInt distance(
              (distance_id.ToStringEncoded(Address::EncodingType::kHex) + 'h').c_str());
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
    */
    /*
        if (GroupRangeStatus::kInRange != holders_result.proximity_status) {
          holders_result.new_holders.clear();
          holders_result.old_holders.clear();
        }
    */
  }

  void DoCheckHoldersTest(const CloseNodesChange& close_nodes_change) {
    Address target_id(RandomString(Address::kSize));
    auto result(close_nodes_change.CheckHolders(target_id));
    auto test_result(CheckHolders(target_id, old_close_nodes_, new_close_nodes_));
    if ((result.new_holder != test_result.new_holder) ||
        (result.proximity_status != test_result.proximity_status)) {
      std::stringstream stream;
      stream << "kAddress_ is " << DebugId(kNodeId_) << " target is " << DebugId(target_id);
      stream << "\nnew_close_nodes :";
      std::for_each(std::begin(new_close_nodes_), std::end(new_close_nodes_),
                    [&](const Address& new_holder) { stream << "\t" << DebugId(new_holder); });
      stream << "\nold_close_nodes :";
      std::for_each(std::begin(old_close_nodes_), std::end(old_close_nodes_),
                    [&](const Address& old_holder) { stream << "\t" << DebugId(old_holder); });
    }
    ASSERT_EQ(result.new_holder, test_result.new_holder);
    ASSERT_EQ(result.proximity_status, test_result.proximity_status);
    // shall check whether reported as leaving range
    //    ASSERT_EQ(result.old_holders, test_result.old_holders);
    //     if ((result.proximity_status != test_result.proximity_status) ||
    //         (result.new_holders != test_result.new_holders) ||
    //         (result.old_holders != test_result.old_holders)) {
    // //
    //
    // //
    //
    //       for (auto& holder : result.new_holders)
    //
    //
    //       for (auto& holder : test_result.new_holders)
    //
    //
    //       for (auto& holder : result.old_holders)
    //
    //
    //       for (auto& holder : test_result.old_holders)
    //
    //
    //       for (auto& holder : new_close_nodes_)
    //
    //
    //       for (auto& holder : old_close_nodes_)
    //
    //       ASSERT_EQ(0, 1);
    //     }
  }

 protected:
  std::vector<Address> old_close_nodes_, new_close_nodes_;
  const Address kNodeId_;
};

TEST_F(CloseNodesChangeTest, BEH_CheckHolders) {
  {
    CloseNodesChange close_nodes_change(kAddress_, old_close_nodes_, new_close_nodes_);
    for (auto i(0); i != 1000; ++i) {
      DoCheckHoldersTest(close_nodes_change);
    }
  }
  {
    for (auto i(0); i != 1000; ++i) {
      Address target_node(RandomString(Address::kSize));
      if (i % 2 == 0)
        new_close_nodes_.push_back(target_node);
      else
        old_close_nodes_.push_back(target_node);
      CloseNodesChange close_nodes_change(kAddress_, old_close_nodes_, new_close_nodes_);
      DoCheckHoldersTest(close_nodes_change);
      if (i % 2 == 0)
        new_close_nodes_.erase(
            std::find(new_close_nodes_.begin(), new_close_nodes_.end(), target_node));
      else
        old_close_nodes_.erase(
            std::find(old_close_nodes_.begin(), old_close_nodes_.end(), target_node));
    }
  }
}

void Choose(const std::set<Address>& online_pmids, const Address& kTarget,
            const std::vector<CloseNodesChange>& owners, int owner_count, int online_pmid_count) {
  // This test is only valid where 'owner_count' <= 'Parameters::group_size'.
  ASSERT_LE(static_cast<unsigned>(owner_count), Parameters::group_size);

  // Create a map of chosen nodes with a count of how many times each was selected by the various
  // owning nodes.
  std::map<Address, int> chosens;
  for (int i(0); i != owner_count; ++i) {
    std::set<Address> copied_online_pmids;
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
  std::vector<Address> old_close_nodes, new_close_nodes;
  const auto kGroupSize(Parameters::group_size);
  for (unsigned int i(0); i != kGroupSize * 5; ++i)
    new_close_nodes.emplace_back(Address::IdType::kRandomId);
  const Address kTarget(RandomString(Address::kSize));

  // Get the 5 closest to 'kTarget' as the owners.
  std::sort(std::begin(new_close_nodes), std::end(new_close_nodes),
            [&kTarget](const Address& lhs, const Address& rhs) {
    return Address::CloserToTarget(lhs, rhs, kTarget);
  });
  old_close_nodes = new_close_nodes;
  old_close_nodes.erase(std::begin(old_close_nodes) + 2);
  std::vector<CloseNodesChange> owners;
  for (unsigned int i(0); i != kGroupSize + 1; ++i)
    owners.push_back(CloseNodesChange(new_close_nodes[i], old_close_nodes, new_close_nodes));

  // Shuffle 'new_close_nodes'.
  std::random_device random_device;
  std::mt19937 random_functor(random_device());
  std::shuffle(std::begin(new_close_nodes), std::end(new_close_nodes), random_functor);

  // 0 online_pmids should throw.
  std::set<Address> online_pmids;
  EXPECT_THROW(owners[0].ChoosePmidNode(online_pmids, kTarget), maidsafe_error);

  for (unsigned int i(0); i != kGroupSize + 2; ++i)
    online_pmids.insert(Address(RandomString(Address::kSize)));

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
  Address Address(RandomString(Address::kSize));
  RoutingTableChangeFunctor routing_table_change_functor(
      [&](const RoutingTableChange& routing_table_change) {
        EXPECT_TRUE(routing_table_change.close_nodes_change != nullptr);
        EXPECT_TRUE(routing_table_change.close_nodes_change->new_node().IsValid());
        EXPECT_FALSE(routing_table_change.close_nodes_change->lost_node().IsValid());
      });
  RoutingTable routing_table(false, Address, asymm::GenerateKeyPair());
  routing_table.InitialiseFunctors(routing_table_change_functor);
  while (routing_table.size() < Parameters::closest_nodes_size) {
    auto node(MakeNode());
    EXPECT_TRUE(routing_table.AddNode(node));
  }
}

TEST_F(CloseNodesChangeTest, BEH_FullSizeRoutingTable) {
  Address Address(RandomString(Address::kSize));
  std::set<Address, std::function<bool(const Address& lhs, const NodeId& rhs)>> new_ids(
      [Address](const Address& lhs, const Address& rhs) {
        return NodeId::CloserToTarget(lhs, rhs, Address);
      });
  NodeInfo new_node;
  Address removed;
  RoutingTable routing_table(false, Address, asymm::GenerateKeyPair());
  RoutingTableChangeFunctor routing_table_change_functor(
      [&](const RoutingTableChange& routing_table_change) {
        if (routing_table_change.close_nodes_change) {
          auto reported(ParseString(routing_table_change.close_nodes_change->ReportConnection()));
          EXPECT_EQ(reported.first, routing_table_change.close_nodes_change->lost_node());
          EXPECT_EQ(reported.second, routing_table_change.close_nodes_change->new_node());
        }
        if (routing_table.size() <= Parameters::closest_nodes_size) {
          bool special_case(
              (routing_table.size() == Parameters::closest_nodes_size) &&
              std::none_of(std::begin(new_ids), std::end(new_ids), [&](const Address& new_Address) {
                return Address::CloserToTarget(removed, new_Address, node_id);
              }));
          if (!special_case)
            EXPECT_NE(routing_table_change.close_nodes_change, nullptr);
          if (routing_table_change.insertion) {
            EXPECT_TRUE(routing_table_change.close_nodes_change->new_node().IsValid());
            EXPECT_FALSE(routing_table_change.close_nodes_change->lost_node().IsValid());
          } else {
            if (routing_table.size() != Parameters::closest_nodes_size)
              EXPECT_FALSE(routing_table_change.close_nodes_change->new_node().IsValid());
            if (!special_case) {
              EXPECT_TRUE(routing_table_change.close_nodes_change->lost_node().IsValid());
              EXPECT_EQ(routing_table_change.close_nodes_change->lost_node(), removed);
            }
            new_ids.erase(removed);
          }
        } else {
          auto iter(new_ids.begin());
          std::advance(iter, Parameters::closest_nodes_size - 1);
          if (routing_table_change.insertion) {
            if (Address::CloserToTarget(new_node.id, *iter, Address)) {
              EXPECT_NE(routing_table_change.close_nodes_change, nullptr);
              EXPECT_TRUE(routing_table_change.close_nodes_change->new_node().IsValid());
              EXPECT_TRUE(routing_table_change.close_nodes_change->lost_node().IsValid());
              std::advance(iter, 1);
              EXPECT_EQ(routing_table_change.close_nodes_change->lost_node(), *iter);
            }
          } else {
            if (!Address::CloserToTarget(*iter, removed, Address)) {
              EXPECT_NE(routing_table_change.close_nodes_change, nullptr);
              EXPECT_TRUE(routing_table_change.close_nodes_change->new_node().IsValid());
              std::advance(iter, 1);
              EXPECT_EQ(routing_table_change.close_nodes_change->new_node(), *iter);
              EXPECT_TRUE(routing_table_change.close_nodes_change->lost_node().IsValid());
              EXPECT_EQ(routing_table_change.close_nodes_change->lost_node(), removed);
            }
          }
          if (routing_table_change.removed.node.id.IsValid())
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
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
