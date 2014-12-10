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

#include <map>

#include "maidsafe/common/test.h"
#include "maidsafe/passport/passport.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/node_info.h"

#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

const size_t kNetworkSize(100);
typedef std::shared_ptr<RoutingTable> RoutingTablePtr;

struct RoutingTableInfo {
 public:
  explicit RoutingTableInfo(const passport::Pmid& pmid_in) : routing_table(), pmid(pmid_in) {}
  RoutingTablePtr routing_table;
  passport::Pmid pmid;

  std::vector<NodeId> GetGroup(const NodeId& target);
  static size_t ready_nodes;
};

std::vector<NodeId> RoutingTableInfo::GetGroup(const NodeId& target) {
  std::vector<NodeId> group_ids;
  std::partial_sort(
      std::begin(routing_table->nodes_), std::begin(routing_table->nodes_) + Parameters::group_size,
      std::end(routing_table->nodes_), [target](const NodeInfo& lhs, const NodeInfo& rhs) {
        return NodeId::CloserToTarget(lhs.id, rhs.id, target);
      });
  for (auto iter(std::begin(routing_table->nodes_));
       iter != std::begin(routing_table->nodes_) + Parameters::group_size; ++iter)
    group_ids.push_back(iter->id);
  group_ids.push_back(routing_table->kNodeId());
  std::sort(std::begin(group_ids), std::end(group_ids),
            [target](const NodeId& lhs,
                     const NodeId& rhs) { return NodeId::CloserToTarget(lhs, rhs, target); });
  group_ids.resize(Parameters::group_size);
  return group_ids;
}

size_t RoutingTableInfo::ready_nodes = 0;

class RoutingTableNetwork : public testing::Test {
 public:
  explicit RoutingTableNetwork(const size_t number_of_closest_nodes = 16)
      : nodes_info_(),
        kNumberofClosestNode(number_of_closest_nodes),
        nodes_changed_(),
        mutex_(),
        network_map_(),
        node_ids_(),
        max_close_index_(0),
        total_close_index_(0),
        close_index_count_(0),
        route_history_size_(5) {
    Parameters::closest_nodes_size = static_cast<unsigned int>(kNumberofClosestNode);
    //    CreateKeys();
  }

 protected:
  void RoutingTablesInfo();
  void CreateNetworkWithNoDiscovery();
  void CreateNetworkWithDiscovery();
  void ValidateRoutingTable();
  void ValidateRoutingTable(std::shared_ptr<RoutingTableInfo> info);
  void ValidateGroup();
  void FindCloseNodesOnDemand();
  void GetCloseNodeIndexStats();
  void AddNewNode();
  void ValidateNewGroupMessaging();
  void ValidateNewGroupMessagingDetails(const NodeId& target, std::set<NodeId>& expected_group,
      std::shared_ptr<RoutingTableInfo> routing_table_info);
  std::vector<std::shared_ptr<RoutingTableInfo>> nodes_info_;
  size_t GetClosenessIndex(const NodeId& node_id, const NodeId& target_id);

 protected:
  void CreateKeys();
  RoutingTablePtr CreateRoutingTable(const passport::Pmid& pmid);
  void OnRoutingTableChange(const NodeId& node_id, const RoutingTableChange& routing_table_change);
  void PartialSortFromTarget(const NodeId& target, const size_t limit, const size_t search_limit);
  void AddNode(std::shared_ptr<RoutingTableInfo> lhs, std::shared_ptr<RoutingTableInfo> rhs);
  bool ConfirmHoldersKnowGroup(const NodeId &target, const std::vector<NodeId>& holders_id,
                               const std::vector<NodeId>& expected_group);
  size_t RoutingTableSize(RoutingTablePtr routing_table);
  size_t kNumberofClosestNode;
  std::set<NodeId> nodes_changed_;
  std::mutex mutex_;
  std::map<NodeId, std::shared_ptr<RoutingTableInfo>> network_map_;
  std::vector<NodeId> node_ids_;
  size_t max_close_index_;
  uint64_t total_close_index_;
  size_t close_index_count_;
  size_t route_history_size_;
};

void RoutingTableNetwork::CreateKeys() {
  size_t index(0);
  while (index++ < kNetworkSize) {
    nodes_info_.push_back(
        std::make_shared<RoutingTableInfo>(passport::CreatePmidAndSigner().first));
  }
}

void RoutingTableNetwork::PartialSortFromTarget(const NodeId& target, size_t sort_limit,
                                                const size_t search_limit) {
  if (sort_limit > search_limit)
    return;

  std::partial_sort(
      std::begin(nodes_info_), std::begin(nodes_info_) + sort_limit,
      std::begin(nodes_info_) + search_limit,
      [target](std::shared_ptr<RoutingTableInfo> lhs, std::shared_ptr<RoutingTableInfo> rhs) {
        return NodeId::CloserToTarget(lhs->routing_table->kNodeId(), rhs->routing_table->kNodeId(),
                                      target);
      });
}

void RoutingTableNetwork::AddNode(std::shared_ptr<RoutingTableInfo> lhs,
                                  std::shared_ptr<RoutingTableInfo> rhs) {
  if (lhs->routing_table->kNodeId() == rhs->routing_table->kNodeId())
    return;
  NodeInfo lhs_node_info, rhs_node_info;
  lhs_node_info.id = lhs->routing_table->kNodeId();
  rhs_node_info.id = rhs->routing_table->kNodeId();
  lhs_node_info.public_key = lhs->pmid.public_key();
  rhs_node_info.public_key = rhs->pmid.public_key();
  if (lhs->routing_table->CheckNode(rhs_node_info) &&
      rhs->routing_table->CheckNode(lhs_node_info)) {
    lhs->routing_table->AddNode(rhs_node_info);
    rhs->routing_table->AddNode(lhs_node_info);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      nodes_changed_.insert(rhs_node_info.id);
      nodes_changed_.insert(lhs_node_info.id);
    }
    FindCloseNodesOnDemand();
  }
}

void RoutingTableNetwork::FindCloseNodesOnDemand() {
  while (!nodes_changed_.empty()) {
    NodeId node_id;
    {
      if (RoutingTableInfo::ready_nodes <= kNumberofClosestNode) {
        nodes_changed_.clear();
        return;
      }
      auto iter(nodes_changed_.begin());
      node_id = *iter;
      nodes_changed_.erase(node_id);
    }
    LOG(kInfo) << "find node on close node change event " << node_id << "size "
               << nodes_changed_.size();
    std::partial_sort(std::begin(node_ids_), std::begin(node_ids_) + kNumberofClosestNode + 1,
                      std::end(node_ids_), [node_id](const NodeId& lhs, const NodeId& rhs) {
      return NodeId::CloserToTarget(lhs, rhs, node_id);
    });

    auto this_iter(std::find_if(std::begin(nodes_info_), std::end(nodes_info_),
                                [node_id](std::shared_ptr<RoutingTableInfo> info) {
      return info->routing_table->kNodeId() == node_id;
    }));
    if (this_iter == std::end(nodes_info_))
      return;
    size_t this_index(std::distance(std::begin(nodes_info_), this_iter));
    for (size_t index(0); index < kNumberofClosestNode; ++index) {
      LOG(kVerbose) << node_id << ", index " << index
                    << nodes_info_.at(index)->routing_table->kNodeId();
      AddNode(nodes_info_.at(this_index), network_map_[node_ids_.at(index)]);
    }
  }
}

RoutingTablePtr RoutingTableNetwork::CreateRoutingTable(const passport::Pmid& pmid) {
  NodeId node_id(pmid.name()->string());
  asymm::Keys keys;
  keys.private_key = pmid.private_key();
  keys.public_key = pmid.public_key();
  auto routing_table(std::make_shared<RoutingTable>(false, node_id, keys));
  routing_table->InitialiseFunctors([node_id, this](
      const RoutingTableChange& routing_table_change) {
    OnRoutingTableChange(node_id, routing_table_change);
  });
  return routing_table;
}

void RoutingTableNetwork::OnRoutingTableChange(const NodeId& node_id,
                                               const RoutingTableChange& routing_table_change) {
  if (routing_table_change.removed.node.id.IsValid()) {
    LOG(kVerbose) << node_id << " removes removed " << routing_table_change.removed.node.id;
    network_map_.at(routing_table_change.removed.node.id)->routing_table->DropNode(node_id, true);
    std::lock_guard<std::mutex> lock(mutex_);
    nodes_changed_.insert(node_id);
  }
  FindCloseNodesOnDemand();
}

void RoutingTableNetwork::AddNewNode() {
  nodes_info_.push_back(std::make_shared<RoutingTableInfo>(passport::CreatePmidAndSigner().first));
  PartialSortFromTarget(NodeId(nodes_info_.back()->pmid.name()->string()), kNumberofClosestNode,
                        RoutingTableInfo::ready_nodes);
  nodes_info_.back()->routing_table = CreateRoutingTable(nodes_info_.back()->pmid);
  network_map_.insert(
      std::make_pair(nodes_info_.back()->routing_table->kNodeId(), nodes_info_.back()));
  node_ids_.push_back(nodes_info_.back()->routing_table->kNodeId());
  //  for (size_t index(0); index < kNumberofClosestNode; ++index)
  //    LOG(kVerbose) << nodes_info_.at(index)->routing_table->kNodeId()
  //                  << " close to " << nodes_info_.back()->routing_table->kNodeId();
  size_t close_node_index(
      std::min(RoutingTableInfo::ready_nodes, static_cast<size_t>(kNumberofClosestNode)));
  for (size_t index(0); index < close_node_index; ++index) {
    AddNode(nodes_info_.back(), nodes_info_.at(index));
    auto distance(GetClosenessIndex(nodes_info_.at(index)->routing_table->kNodeId(),
                                    nodes_info_.back()->routing_table->kNodeId()));
    max_close_index_ = (distance > max_close_index_) ? distance : max_close_index_;
    total_close_index_ += distance;
    close_index_count_++;
    LOG(kVerbose) << "distances " << distance << ", " << index + 1 << " "
                  << nodes_info_.back()->routing_table->kNodeId() << " and "
                  << nodes_info_.at(index)->routing_table->kNodeId();
  }

  if (RoutingTableInfo::ready_nodes > size_t(0) &&
      RoutingTableInfo::ready_nodes <=
          static_cast<size_t>(Parameters::max_routing_table_size * 2)) {
    for (unsigned int index(0);
         index < std::min(RoutingTableInfo::ready_nodes,
                          static_cast<size_t>(Parameters::max_routing_table_size * 2));
         ++index) {
      AddNode(*nodes_info_.rbegin(), nodes_info_.at(index));
    }
  } else if (RoutingTableInfo::ready_nodes != 0) {
    while ((*nodes_info_.rbegin())->routing_table->nodes_.size() <
           static_cast<size_t>(Parameters::unidirectional_interest_range)) {
      AddNode(*nodes_info_.rbegin(),
              nodes_info_.at(RandomUint32() % RoutingTableInfo::ready_nodes));
    }
  }
  if (RoutingTableInfo::ready_nodes > 100) {
    size_t random_index(RandomUint32() % RoutingTableInfo::ready_nodes);
    for (size_t i(0); i < RoutingTableInfo::ready_nodes; ++i) {
      AddNode(*nodes_info_.rbegin(),
              nodes_info_.at((i + random_index) % RoutingTableInfo::ready_nodes));
      if ((*nodes_info_.rbegin())->routing_table->nodes_.size() > 60)
        break;
    }
  }

  RoutingTableInfo::ready_nodes++;
  LOG(kSuccess) << (*nodes_info_.rbegin())->routing_table->kNodeId() << " added successfully "
                << RoutingTableInfo::ready_nodes << " node_ids_ " << node_ids_.size();
}

size_t RoutingTableNetwork::GetClosenessIndex(const NodeId& node_id, const NodeId& target_id) {
  size_t sort_limit(std::min(RoutingTableInfo::ready_nodes, size_t(64))), distance(0);
  bool found(false);
  while (!found) {
    std::partial_sort(std::begin(node_ids_), std::begin(node_ids_) + sort_limit,
                      std::end(node_ids_), [node_id](const NodeId& lhs, const NodeId& rhs) {
      return NodeId::CloserToTarget(lhs, rhs, node_id);
    });
    auto target_iter(
        std::find(std::begin(node_ids_), std::begin(node_ids_) + sort_limit, target_id));
    distance = static_cast<size_t>(std::distance(std::begin(node_ids_), target_iter));
    if (distance >= sort_limit) {
      sort_limit = (sort_limit + 64 < node_ids_.size()) ? (sort_limit + 64) : node_ids_.size();
    } else {
      found = true;
    }
  }
  return distance;
}

void RoutingTableNetwork::ValidateRoutingTable() {
  std::map<NodeId, size_t> result;
  size_t max_distance(0), min_distance(kNumberofClosestNode), total_distance(0), distance_count(0);
  std::vector<NodeId> node_ids;
  for (auto iter(std::begin(nodes_info_)); iter != std::end(nodes_info_); ++iter)
    node_ids.push_back((*iter)->routing_table->kNodeId());
  for (auto iter(std::begin(nodes_info_)); iter != std::end(nodes_info_); ++iter) {
    std::partial_sort(std::begin(node_ids), std::begin(node_ids) + kNumberofClosestNode + 1,
                      std::end(node_ids), [iter](const NodeId& lhs, const NodeId& rhs) {
      return NodeId::CloserToTarget(lhs, rhs, (*iter)->routing_table->kNodeId());
    });
    auto last_close_iter(std::begin(node_ids));
    std::advance(last_close_iter, kNumberofClosestNode);
    for (auto node_ids_iter(std::begin(node_ids) + 1); node_ids_iter != last_close_iter;
         ++node_ids_iter) {
      if (std::none_of(std::begin((*iter)->routing_table->nodes_),
                       std::end((*iter)->routing_table->nodes_),
                       [node_ids_iter](const NodeInfo& node_info) {
            return node_info.id == *node_ids_iter;
          })) {
        LOG(kError) << *node_ids_iter << " is not in close nodes of "
                    << (*iter)->routing_table->kNodeId() << " distance "
                    << std::distance(std::begin(node_ids), node_ids_iter);
        max_distance =
            (static_cast<size_t>(std::distance(std::begin(node_ids), node_ids_iter)) > max_distance)
                ? std::distance(std::begin(node_ids), node_ids_iter)
                : max_distance;
        min_distance =
            (static_cast<size_t>(std::distance(std::begin(node_ids), node_ids_iter)) < min_distance)
                ? std::distance(std::begin(node_ids), node_ids_iter)
                : min_distance;
        total_distance += std::distance(std::begin(node_ids), node_ids_iter);
        distance_count++;
        if (result.find((*iter)->routing_table->kNodeId()) != std::end(result))
          result[(*iter)->routing_table->kNodeId()]++;
        else
          result.insert(std::make_pair((*iter)->routing_table->kNodeId(), size_t(1)));
      }
    }
  }
  auto network_size(nodes_info_.size());
  LOG(kSuccess) << "Number of nodes missing close nodes " << result.size() << " out of"
                << network_size << " nodes";
  size_t accumulate(0);
  for (const auto& errors : result)
    accumulate += errors.second;

  LOG(kSuccess) << "Total number of missings close nodes " << accumulate;
  LOG(kSuccess) << "Maximum distance " << max_distance << " network size " << network_size;
  LOG(kSuccess) << "Minimum distance " << min_distance << " network size " << network_size;
  LOG(kSuccess) << "Average distance "
                << total_distance / ((distance_count == 0) ? 1 : distance_count) << " network size "
                << network_size;

  LOG(kSuccess) << "Max close index " << max_close_index_ << " network size " << network_size;
  LOG(kSuccess) << "Average close index "
                << total_close_index_ / ((close_index_count_ == 0) ? 1 : close_index_count_)
                << " network size " << network_size;
  max_close_index_ = 0;
}

void RoutingTableNetwork::ValidateRoutingTable(std::shared_ptr<RoutingTableInfo> info) {
  PartialSortFromTarget(info->routing_table->kNodeId(), kNumberofClosestNode + 1,
                        RoutingTableInfo::ready_nodes);
  auto last_close(std::begin(nodes_info_));
  std::advance(last_close, kNumberofClosestNode + 1);
  for (auto iter(std::begin(nodes_info_) + 1); iter != last_close; ++iter) {
    EXPECT_FALSE(std::none_of(std::begin(info->routing_table->nodes_),
                              std::end(info->routing_table->nodes_),
                              [iter](const NodeInfo& node_info) {
      return node_info.id == (*iter)->routing_table->kNodeId();
    }))
        << info->routing_table->kNodeId() << " missing close " << (*iter)->routing_table->kNodeId();
  }
}

void RoutingTableNetwork::ValidateGroup() {
  std::vector<size_t> close_nodes_results(Parameters::group_size, 0);
  size_t number_of_tests(1000), kNumberofTests(1000);
  while (number_of_tests-- > 0) {
    NodeId random_node_id(RandomString(NodeId::kSize));
    PartialSortFromTarget(random_node_id, static_cast<size_t>(Parameters::group_size),
                          RoutingTableInfo::ready_nodes);
    std::vector<NodeId> group_ids;
    for (auto iter(std::begin(nodes_info_));
         iter != std::begin(nodes_info_) + Parameters::group_size; ++iter) {
      group_ids.push_back((*iter)->routing_table->kNodeId());
    }
    ASSERT_EQ(group_ids.size(), size_t(4));
    for (size_t index(0); index < Parameters::group_size; ++index) {
      auto routing_table_group(nodes_info_.at(index)->GetGroup(random_node_id));
      if (!std::equal(std::begin(routing_table_group), std::end(routing_table_group),
                      std::begin(group_ids))) {
        close_nodes_results[index]++;
      }
    }
  }
  for (size_t index(0); index < Parameters::group_size; ++index)
    LOG(kSuccess) << "Number of times " << index + 1 << "th group member missed the group id "
                  << close_nodes_results[index] << ", "
                  << close_nodes_results[index] * 100.00 / kNumberofTests << "% of "
                  << kNumberofTests << " attempts in " << RoutingTableInfo::ready_nodes << " nodes";
}

void RoutingTableNetwork::GetCloseNodeIndexStats() {
  size_t max_close_index(0), total_close_index(0), close_index_count(0);
  for (auto iter(std::begin(nodes_info_)); iter != std::end(nodes_info_); ++iter) {
    NodeId node_id((*iter)->routing_table->kNodeId());
    std::partial_sort(std::begin((*iter)->routing_table->nodes_),
                      std::begin((*iter)->routing_table->nodes_) + kNumberofClosestNode,
                      std::end((*iter)->routing_table->nodes_),
                      [node_id](const NodeInfo& lhs, const NodeInfo& rhs) {
      return NodeId::CloserToTarget(lhs.id, rhs.id, node_id);
    });
    for (size_t index(0); index < kNumberofClosestNode; ++index) {
      auto distance(GetClosenessIndex((*iter)->routing_table->nodes_.at(index).id, node_id));
      max_close_index = (distance > max_close_index) ? distance : max_close_index;
      total_close_index += distance;
      close_index_count++;
    }
  }
  LOG(kSuccess) << "Network max close index " << max_close_index << " size: " << nodes_info_.size();
  LOG(kSuccess) << "Network average close index " << total_close_index / close_index_count
                << " size: " << nodes_info_.size();
}

void RoutingTableNetwork::RoutingTablesInfo() {
  for (const auto& info : nodes_info_) {
    LOG(kVerbose) << info->routing_table->kNodeId() << " : " << info->routing_table->size();
  }
}

TEST_F(RoutingTableNetwork, FUNC_AnalyseNetwork) {
  size_t kMaxNetworkSize(100), kReportInterval(50);
  LOG(kVerbose) << "Add new nodes";
  for (size_t index(0); index < kMaxNetworkSize; ++index) {
    AddNewNode();
    LOG(kVerbose) << "back id: " << nodes_info_.back()->routing_table->kNodeId();
    if (index > this->kNumberofClosestNode) {
      ValidateRoutingTable(nodes_info_.back());
      if (index % kReportInterval == 0) {
        LOG(kSuccess) << "\n\n\nStats for a network of " << nodes_info_.size() << " nodes.";
        RoutingTablesInfo();
        ValidateRoutingTable();
        GetCloseNodeIndexStats();
        ValidateGroup();
      }
    }
  }
}

void RoutingTableNetwork::ValidateNewGroupMessaging() {
  NodeId target(RandomString(NodeId::kSize));
  for (size_t index(0); index < 10; ++index) {
    std::set<NodeId> expected_group;
    std::partial_sort(std::begin(node_ids_),
                      std::begin(node_ids_) + Parameters::group_size + 1,
                      std::end(node_ids_),
                      [&, this](const NodeId& lhs, const NodeId& rhs) {
                        return NodeId::CloserToTarget(lhs, rhs, target);
                      });
    for (unsigned int index(0); index < Parameters::group_size; ++index)
      expected_group.insert(node_ids_.at(index));
    auto random_node(nodes_info_.at(RandomUint32() % nodes_info_.size()));
    ValidateNewGroupMessagingDetails(target, expected_group, random_node);
  }
}

void RoutingTableNetwork::ValidateNewGroupMessagingDetails(
    const NodeId& target, std::set<NodeId>& expected_group,
    std::shared_ptr<RoutingTableInfo> routing_table_info) {

  std::set<NodeId> potential_members, found_group, tried;
  potential_members.insert(routing_table_info->routing_table->kNodeId());
  LOG(kVerbose) << "ValidateNewGroupMessagingDetails " << target;

  do {
    bool self_added(false);
    auto current(network_map_.at(*potential_members.begin()));
    LOG(kVerbose) << current->routing_table->kNodeId() << ", target: " << target;
    auto closests_to_self(
        current->routing_table->GetClosestNodes(
            current->routing_table->kNodeId(), static_cast<unsigned int>(kNumberofClosestNode),
            true));

    if (std::find(std::begin(expected_group), std::end(expected_group),
                  current->routing_table->kNodeId()) != std::end(expected_group)) {
      found_group.insert(current->routing_table->kNodeId());
      self_added = true;
    }

    if (NodeId::CloserToTarget(target, closests_to_self.at(kNumberofClosestNode - 1).id,
                               current->routing_table->kNodeId())) {
      auto closests_to_target(
          current->routing_table->GetClosestNodes(target, Parameters::group_size, true));

      for (const auto& close : closests_to_target)
        LOG(kVerbose) << close.id;

      auto limit(std::begin(closests_to_target));
      std::advance(limit, (Parameters::group_size - (self_added ? 1 : 0)));
      for (auto iter(std::begin(closests_to_target)); iter != limit; ++iter) {
        LOG(kVerbose) << "inside: " << iter->id;
        if (std::find(std::begin(tried), std::end(tried), iter->id) == std::end(tried)) {
          potential_members.insert(iter->id);
        }
      }
    } else {
      auto closests_to_target(
          current->routing_table->GetClosestNodes(target, 1, true));
        if (std::find(std::begin(tried), std::end(tried),
            closests_to_target.begin()->id) == std::end(tried)) {
          potential_members.insert(closests_to_target.at(0).id);
        }
    }

    for (auto const& member : potential_members)
      LOG(kVerbose) << member;
    potential_members.erase(current->routing_table->kNodeId());
    tried.insert(current->routing_table->kNodeId());
    for (auto const& member : potential_members)
      LOG(kVerbose) << member;
  } while (!potential_members.empty());

  EXPECT_EQ(found_group.size(), expected_group.size());
  for (const auto& expected_node :  expected_group)
    EXPECT_NE(found_group.find(expected_node), std::end(found_group));
}

TEST_F(RoutingTableNetwork, FUNC_GroupMessaging) {
  size_t kMaxNetworkSize(1000);
  LOG(kVerbose) << "Add new nodes";
  for (size_t index(0); index < 300; ++index)
    AddNewNode();

  for (size_t index(300); index < kMaxNetworkSize; ++index) {
    AddNewNode();
    ValidateNewGroupMessaging();
    LOG(kVerbose) << index << " current add index\n";
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
