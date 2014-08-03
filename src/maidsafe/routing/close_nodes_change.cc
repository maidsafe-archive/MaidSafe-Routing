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

#include "maidsafe/routing/close_nodes_change.h"

#include <limits>
#include <utility>

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

CloseNodesChange::CloseNodesChange()
    : node_id_(), old_close_nodes_(), new_close_nodes_(), lost_node_(), new_node_(), radius_() {}

CloseNodesChange::CloseNodesChange(const CloseNodesChange& other)
    : node_id_(other.node_id_),
      old_close_nodes_(other.old_close_nodes_),
      new_close_nodes_(other.new_close_nodes_),
      lost_node_(other.lost_node_),
      new_node_(other.new_node_),
      radius_(other.radius_) {}

CloseNodesChange::CloseNodesChange(CloseNodesChange&& other)
    : node_id_(std::move(other.node_id_)),
      old_close_nodes_(std::move(other.old_close_nodes_)),
      new_close_nodes_(std::move(other.new_close_nodes_)),
      lost_node_(std::move(other.lost_node_)),
      new_node_(std::move(other.new_node_)),
      radius_(std::move(other.radius_)) {}

CloseNodesChange& CloseNodesChange::operator=(CloseNodesChange other) {
  swap(*this, other);
  return *this;
}

CloseNodesChange::CloseNodesChange(NodeId this_node_id, const std::vector<NodeId>& old_close_nodes,
                                   const std::vector<NodeId>& new_close_nodes)
    : node_id_(std::move(this_node_id)),
      old_close_nodes_([this](std::vector<NodeId> old_close_nodes_in)->std::vector<NodeId> {
        std::sort(std::begin(old_close_nodes_in), std::end(old_close_nodes_in),
                  [this](const NodeId& lhs,
                         const NodeId& rhs) { return NodeId::CloserToTarget(lhs, rhs, node_id_); });
        return old_close_nodes_in;
      }(old_close_nodes)),
      new_close_nodes_([this](std::vector<NodeId> new_close_nodes_in)->std::vector<NodeId> {
        std::sort(std::begin(new_close_nodes_in), std::end(new_close_nodes_in),
                  [this](const NodeId& lhs,
                         const NodeId& rhs) { return NodeId::CloserToTarget(lhs, rhs, node_id_); });
        return new_close_nodes_in;
      }(new_close_nodes)),
      lost_node_([this]()->NodeId {
        std::vector<NodeId> lost_nodes;
        std::set_difference(std::begin(old_close_nodes_), std::end(old_close_nodes_),
                            std::begin(new_close_nodes_), std::end(new_close_nodes_),
                            std::back_inserter(lost_nodes),
                            [this](const NodeId& lhs, const NodeId& rhs) {
          return NodeId::CloserToTarget(lhs, rhs, node_id_);
        });
        assert(lost_nodes.size() <= 1);
        return (lost_nodes.empty()) ? NodeId() : lost_nodes.at(0);
      }()),
      new_node_([this]()->NodeId {
        std::vector<NodeId> new_nodes;
        std::set_difference(std::begin(new_close_nodes_), std::end(new_close_nodes_),
                            std::begin(old_close_nodes_), std::end(old_close_nodes_),
                            std::back_inserter(new_nodes),
                            [this](const NodeId& lhs, const NodeId& rhs) {
          return NodeId::CloserToTarget(lhs, rhs, node_id_);
        });
        assert(new_nodes.size() <= 1);
        return (new_nodes.empty()) ? NodeId() : new_nodes.at(0);
      }()),
      radius_([this]()->crypto::BigInt {
        NodeId fcn_distance;
        if (new_close_nodes_.size() >= Parameters::closest_nodes_size)
          fcn_distance = node_id_ ^ new_close_nodes_[Parameters::closest_nodes_size - 1];
        else
          fcn_distance = NodeInNthBucket(node_id_, Parameters::closest_nodes_size);
        return (crypto::BigInt(
                    (fcn_distance.ToStringEncoded(NodeId::EncodingType::kHex) + 'h').c_str()) *
                Parameters::proximity_factor);
      }()) {}

CheckHoldersResult CloseNodesChange::CheckHolders(const NodeId& target) const {
  // Handle cases of lower number of group close_nodes nodes
  size_t group_size_adjust(Parameters::group_size + 1U);
  size_t old_holders_size = std::min(old_close_nodes_.size(), group_size_adjust);
  size_t new_holders_size = std::min(new_close_nodes_.size(), group_size_adjust);

  std::vector<NodeId> old_holders(old_holders_size), new_holders(new_holders_size);

  std::partial_sort_copy(std::begin(old_close_nodes_), std::end(old_close_nodes_),
                         std::begin(old_holders), std::end(old_holders),
                         [target](const NodeId& lhs, const NodeId& rhs) {
    return NodeId::CloserToTarget(lhs, rhs, target);
  });
  std::partial_sort_copy(std::begin(new_close_nodes_), std::end(new_close_nodes_),
                         std::begin(new_holders), std::end(new_holders),
                         [target](const NodeId& lhs, const NodeId& rhs) {
    return NodeId::CloserToTarget(lhs, rhs, target);
  });

  // Remove target == node ids and adjust holder size
  old_holders.erase(std::remove(std::begin(old_holders), std::end(old_holders), target),
                    std::end(old_holders));
  if (old_holders.size() > Parameters::group_size) {
    old_holders.resize(Parameters::group_size);
    assert(old_holders.size() == Parameters::group_size);
  }

  new_holders.erase(std::remove(std::begin(new_holders), std::end(new_holders), target),
                    std::end(new_holders));
  if (new_holders.size() > Parameters::group_size) {
    new_holders.resize(Parameters::group_size);
    assert(new_holders.size() == Parameters::group_size);
  }

  CheckHoldersResult holders_result;
  holders_result.proximity_status = GroupRangeStatus::kOutwithRange;
  if (!new_holders.empty() && ((new_holders.size() < Parameters::group_size) ||
                               NodeId::CloserToTarget(node_id_, new_holders.back(), target))) {
    holders_result.proximity_status = GroupRangeStatus::kInRange;
    if (new_holders.size() == Parameters::group_size)
      new_holders.pop_back();
    new_holders.push_back(node_id_);
  }

  if (!old_holders.empty() && NodeId::CloserToTarget(node_id_, old_holders.back(), target)) {
    old_holders.pop_back();
    if (old_holders.size() == Parameters::group_size)
      old_holders.pop_back();
    old_holders.push_back(node_id_);
  }

  holders_result.new_holders = new_holders;
  holders_result.old_holders = old_holders;
  return holders_result;
}

NodeId CloseNodesChange::ChoosePmidNode(const std::set<NodeId>& online_pmids,
                                        const NodeId& target) const {
  if (online_pmids.empty())
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::invalid_parameter));

  LOG(kInfo) << "CloseNodesChange::ChoosePmidNode having following new_close_nodes_ : ";
  for (auto id : new_close_nodes_)
    LOG(kInfo) << "       new_close_nodes_ ids     ---  " << HexSubstr(id.string());
  LOG(kInfo) << "CloseNodesChange::ChoosePmidNode having target : " << HexSubstr(target.string())
             << " and following online_pmids : ";
  for (auto pmid : online_pmids)
    LOG(kInfo) << "       online_pmids        ---  " << HexSubstr(pmid.string());

  // In case storing to PublicPmid, the data shall not be stored on the Vault itself
  // However, the vault will appear in DM's routing table and affect result
  std::vector<NodeId> temp(Parameters::group_size + 1);
  std::partial_sort_copy(std::begin(new_close_nodes_), std::end(new_close_nodes_), std::begin(temp),
                         std::end(temp), [&target](const NodeId& lhs, const NodeId& rhs) {
    return NodeId::CloserToTarget(lhs, rhs, target);
  });

  LOG(kInfo) << "CloseNodesChange::ChoosePmidNode own id : " << HexSubstr(node_id_.string())
             << " and closest+1 to the target are : ";
  for (auto node : temp)
    LOG(kInfo) << "       sorted_neighbours   ---  " << HexSubstr(node.string());

  auto temp_itr(std::begin(temp));
  auto pmids_itr(std::begin(online_pmids));
  while (*temp_itr != node_id_) {
    ++temp_itr;
    //     assert(temp_itr != std::end(temp));
    if (temp_itr == std::end(temp)) {
      LOG(kError) << "node_id_ not listed in group range having " << temp.size() << " nodes";
      break;
    }
    if (++pmids_itr == std::end(online_pmids))
      pmids_itr = std::begin(online_pmids);
  }

  return *pmids_itr;
}

void swap(CloseNodesChange& lhs, CloseNodesChange& rhs) MAIDSAFE_NOEXCEPT {
  using std::swap;
  swap(lhs.node_id_, rhs.node_id_);
  swap(lhs.old_close_nodes_, rhs.old_close_nodes_);
  swap(lhs.new_close_nodes_, rhs.new_close_nodes_);
  swap(lhs.lost_node_, rhs.lost_node_);
  swap(lhs.new_node_, rhs.new_node_);
  swap(lhs.radius_, rhs.radius_);
}

void CloseNodesChange::Print() const {
  std::stringstream stream;
  for (auto entry : old_close_nodes_)
    stream << "\n\t\tentry in old_close_nodes"
           << "\t------\t" << entry;

  for (auto entry : new_close_nodes_)
    stream << "\n\t\tentry in new_close_nodes\t------\t" << entry;

  stream << "\n\t\tentry in lost_node\t------\t" << lost_node_;
  stream << "\n\t\tentry in new_node\t------\t" << new_node_;
  LOG(kInfo) << stream.str();
}

}  // namespace routing

}  // namespace maidsafe
