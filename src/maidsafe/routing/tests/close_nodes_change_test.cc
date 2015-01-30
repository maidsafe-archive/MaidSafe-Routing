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
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/close_nodes_change.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {
namespace routing {
namespace test {

std::pair<NodeId, NodeId> ParseString(std::string input) {
  NodeId old_node_id, new_node_id;
  {
    std::istringstream istringstream{input};
    cereal::JSONInputArchive archive{istringstream};
    std::string node_str;
    try {
      archive(cereal::make_nvp("vaultRemoved", node_str));
      old_node_id = NodeId(node_str, NodeId::EncodingType::kHex);
    } catch (const std::exception& e) {
      LOG(kVerbose) << e.what();
    }
    try {
      archive(cereal::make_nvp("vaultAdded", node_str));
      new_node_id = NodeId(node_str, NodeId::EncodingType::kHex);
    } catch (const std::exception& e) {
      LOG(kVerbose) << e.what();
    }
  }
  return std::make_pair(old_node_id, new_node_id);
}

class CloseNodesChangeTest : public testing::Test {
 protected:
  CloseNodesChangeTest()
      : old_close_nodes_(), new_close_nodes_(), kNodeId_(RandomString(NodeId::kSize)) {
    int old_close_nodes_size(Parameters::closest_nodes_size);

    for (auto i(0); i != old_close_nodes_size - 1; ++i) {
      old_close_nodes_.push_back(NodeId(RandomString(NodeId::kSize)));
      new_close_nodes_.push_back(old_close_nodes_.back());
    }
  }

 public:
  CheckHoldersResult CheckHolders(const NodeId& target, std::vector<NodeId>& old_close_nodes,
                                  std::vector<NodeId>& new_close_nodes) {
    std::sort(new_close_nodes.begin(), new_close_nodes.end(),
              [&](const NodeId& lhs, const NodeId& rhs) {
      return NodeId::CloserToTarget(lhs, rhs, target);
    });
    std::sort(old_close_nodes.begin(), old_close_nodes.end(),
              [&](const NodeId& lhs, const NodeId& rhs) {
      return NodeId::CloserToTarget(lhs, rhs, target);
    });
    unsigned int new_node_tail(Parameters::group_size), old_node_tail(Parameters::group_size);
    if (new_close_nodes.front() != target)
      --new_node_tail;
    if (old_close_nodes.front() != target)
      --old_node_tail;
    bool now_in_range(
        NodeId::CloserToTarget(this->kNodeId_, new_close_nodes[new_node_tail], target));
    bool was_in_range(
        NodeId::CloserToTarget(this->kNodeId_, old_close_nodes[old_node_tail], target));
    LOG(kVerbose) << "now_in_range : " << std::boolalpha << now_in_range
                  << " was_in_range : " << was_in_range;
    /*    // kNodeId_ is included in the new_close_nodes_
        if (this->kNodeId_ == new_close_nodes[new_node_tail])
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
    std::vector<NodeId> diff_new_holders;
    std::for_each(std::begin(new_close_nodes), new_close_nodes.begin() + new_node_tail + 1,
                  [&](const NodeId& new_holder) {
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
                  [this](const NodeId& lhs, const NodeId& rhs) {
          return NodeId::CloserToTarget(lhs, rhs, this->kNodeId_);
        });
        NodeId fcn_distance;
        if (new_close_nodes.size() >= Parameters::closest_nodes_size)
          fcn_distance = kNodeId_ ^ new_close_nodes[Parameters::closest_nodes_size - 1];
        else
          fcn_distance = kNodeId_ ^ NodeInNthBucket(kNodeId_, Parameters::closest_nodes_size);
        crypto::BigInt radius(
            crypto::BigInt((fcn_distance.ToStringEncoded(NodeId::EncodingType::kHex) + 'h').c_str())
       *
            Parameters::proximity_factor);

        // sort by target
        std::sort(old_close_nodes.begin(), old_close_nodes.end(),
                  [target](const NodeId& lhs,
                           const NodeId& rhs) { return NodeId::CloserToTarget(lhs, rhs, target); });

        std::sort(new_close_nodes.begin(), new_close_nodes.end(),
                  [target](const NodeId& lhs,
                           const NodeId& rhs) { return NodeId::CloserToTarget(lhs, rhs, target); });

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
    */
    /*
        if (GroupRangeStatus::kInRange != holders_result.proximity_status) {
          holders_result.new_holders.clear();
          holders_result.old_holders.clear();
        }
    */
  }

  void DoCheckHoldersTest(const CloseNodesChange& close_nodes_change) {
    NodeId target_id(RandomString(NodeId::kSize));
    auto result(close_nodes_change.CheckHolders(target_id));
    auto test_result(CheckHolders(target_id, old_close_nodes_, new_close_nodes_));
    if ((result.new_holder != test_result.new_holder) ||
        (result.proximity_status != test_result.proximity_status)) {
      std::stringstream stream;
      stream << "kNodeId_ is " << DebugId(kNodeId_) << " target is " << DebugId(target_id);
      stream << "\nnew_close_nodes :";
      std::for_each(std::begin(new_close_nodes_), std::end(new_close_nodes_),
                    [&](const NodeId& new_holder) { stream << "\t" << DebugId(new_holder); });
      stream << "\nold_close_nodes :";
      std::for_each(std::begin(old_close_nodes_), std::end(old_close_nodes_),
                    [&](const NodeId& old_holder) { stream << "\t" << DebugId(old_holder); });
      LOG(kInfo) << stream.str();
    }
    ASSERT_EQ(result.new_holder, test_result.new_holder);
    ASSERT_EQ(result.proximity_status, test_result.proximity_status);
    // shall check whether reported as leaving range
    //    ASSERT_EQ(result.old_holders, test_result.old_holders);
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
  {
    CloseNodesChange close_nodes_change(kNodeId_, old_close_nodes_, new_close_nodes_);
    for (auto i(0); i != 1000; ++i) {
      LOG(kVerbose) << "random target against same table, interation " << i;
      DoCheckHoldersTest(close_nodes_change);
    }
  }
  {
    for (auto i(0); i != 1000; ++i) {
      LOG(kVerbose) << "random target and node change, iteration " << i;
      NodeId target_node(RandomString(NodeId::kSize));
      if (i % 2 == 0)
        new_close_nodes_.push_back(target_node);
      else
        old_close_nodes_.push_back(target_node);
      CloseNodesChange close_nodes_change(kNodeId_, old_close_nodes_, new_close_nodes_);
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

TEST_F(CloseNodesChangeTest, BEH_SmallSizeRoutingTable) {
  NodeId node_id(RandomString(NodeId::kSize));
  RoutingTableChangeFunctor routing_table_change_functor(
      [&](const RoutingTableChange& routing_table_change) {
        EXPECT_TRUE(routing_table_change.close_nodes_change != nullptr);
        EXPECT_TRUE(routing_table_change.close_nodes_change->new_node().IsValid());
        EXPECT_FALSE(routing_table_change.close_nodes_change->lost_node().IsValid());
      });
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair());
  routing_table.InitialiseFunctors(routing_table_change_functor);
  while (routing_table.size() < Parameters::closest_nodes_size) {
    auto node(MakeNode());
    EXPECT_TRUE(routing_table.AddNode(node));
  }
}

TEST_F(CloseNodesChangeTest, BEH_FullSizeRoutingTable) {
  NodeId node_id(RandomString(NodeId::kSize));
  std::set<NodeId, std::function<bool(const NodeId& lhs, const NodeId& rhs)>> new_ids([node_id](
      const NodeId& lhs, const NodeId& rhs) { return NodeId::CloserToTarget(lhs, rhs, node_id); });
  NodeInfo new_node;
  NodeId removed;
  RoutingTable routing_table(false, node_id, asymm::GenerateKeyPair());
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
              std::none_of(std::begin(new_ids), std::end(new_ids), [&](const NodeId& new_node_id) {
                return NodeId::CloserToTarget(removed, new_node_id, node_id);
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
            if (NodeId::CloserToTarget(new_node.id, *iter, node_id)) {
              EXPECT_NE(routing_table_change.close_nodes_change, nullptr);
              EXPECT_TRUE(routing_table_change.close_nodes_change->new_node().IsValid());
              EXPECT_TRUE(routing_table_change.close_nodes_change->lost_node().IsValid());
              std::advance(iter, 1);
              EXPECT_EQ(routing_table_change.close_nodes_change->lost_node(), *iter);
            }
          } else {
            if (!NodeId::CloserToTarget(*iter, removed, node_id)) {
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

template <typename ConnectionType>
class ConnectionChangesTest : public testing::Test {
 public:
  ConnectionChangesTest() {
    std::vector<NodeId> old_ids, new_ids;

    for (auto index(Parameters::closest_nodes_size); index != 0; --index)
      old_ids.push_back(NodeId(RandomString(NodeId::kSize)));

    new_ids = old_ids;
    old_ids.erase(old_ids.begin());
    auto close_nodes_change(ConnectionType(NodeId(RandomString(NodeId::kSize)), old_ids, new_ids));
    auto copy(close_nodes_change);
    auto copy2(std::move(copy));
    EXPECT_EQ(close_nodes_change, copy2);
    EXPECT_NE(copy, copy2);
  }
};

TYPED_TEST_CASE_P(ConnectionChangesTest);

TYPED_TEST_P(ConnectionChangesTest, BEH_BEH_Constructors) {}

REGISTER_TYPED_TEST_CASE_P(ConnectionChangesTest, BEH_BEH_Constructors);
typedef testing::Types<ConnectionsChange, ClientNodesChange, CloseNodesChange> ConnectionTypes;
INSTANTIATE_TYPED_TEST_CASE_P(AccountTransfer, ConnectionChangesTest, ConnectionTypes);

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
