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
#include <sstream>
#include <utility>

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/vector.hpp"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

CloseNodesChange::CloseNodesChange()
    : Address_(), old_close_nodes_(), new_close_nodes_(), lost_node_(), new_node_(), radius_() {}

CloseNodesChange::CloseNodesChange(const CloseNodesChange& other)
    : Address_(other.node_id_),
      old_close_nodes_(other.old_close_nodes_),
      new_close_nodes_(other.new_close_nodes_),
      lost_node_(other.lost_node_),
      new_node_(other.new_node_),
      radius_(other.radius_) {}

CloseNodesChange::CloseNodesChange(CloseNodesChange&& other)
    : Address_(std::move(other.node_id_)),
      old_close_nodes_(std::move(other.old_close_nodes_)),
      new_close_nodes_(std::move(other.new_close_nodes_)),
      lost_node_(std::move(other.lost_node_)),
      new_node_(std::move(other.new_node_)),
      radius_(std::move(other.radius_)) {}

CloseNodesChange& CloseNodesChange::operator=(CloseNodesChange other) {
  swap(*this, other);
  return *this;
}

CloseNodesChange::CloseNodesChange(Address this_Address, const std::vector<Address>& old_close_nodes,
                                   const std::vector<Address>& new_close_nodes)
    : Address_(std::move(this_node_id)),
      old_close_nodes_([this](std::vector<Address> old_close_nodes_in) -> std::vector<Address> {
        std::sort(std::begin(old_close_nodes_in), std::end(old_close_nodes_in),
                  [this](const Address& lhs, const Address& rhs) {
          return Address::CloserToTarget(lhs, rhs, Address_);
        });
        return old_close_nodes_in;
      }(old_close_nodes)),
      new_close_nodes_([this](std::vector<Address> new_close_nodes_in) -> std::vector<Address> {
        std::sort(std::begin(new_close_nodes_in), std::end(new_close_nodes_in),
                  [this](const Address& lhs, const Address& rhs) {
          return Address::CloserToTarget(lhs, rhs, Address_);
        });
        return new_close_nodes_in;
      }(new_close_nodes)),
      lost_node_([this]() -> Address {
        std::vector<Address> lost_nodes;
        std::set_difference(std::begin(old_close_nodes_), std::end(old_close_nodes_),
                            std::begin(new_close_nodes_), std::end(new_close_nodes_),
                            std::back_inserter(lost_nodes),
                            [this](const Address& lhs, const Address& rhs) {
          return Address::CloserToTarget(lhs, rhs, Address_);
        });
        assert(lost_nodes.size() <= 1);
        return (lost_nodes.empty()) ? Address() : lost_nodes.at(0);
      }()),
      new_node_([this]() -> Address {
        std::vector<Address> new_nodes;
        std::set_difference(std::begin(new_close_nodes_), std::end(new_close_nodes_),
                            std::begin(old_close_nodes_), std::end(old_close_nodes_),
                            std::back_inserter(new_nodes),
                            [this](const Address& lhs, const Address& rhs) {
          return Address::CloserToTarget(lhs, rhs, Address_);
        });
        assert(new_nodes.size() <= 1);
        return (new_nodes.empty()) ? Address() : new_nodes.at(0);
      }()),
      radius_([this]() -> crypto::BigInt {
        Address fcn_distance;
        if (new_close_nodes_.size() >= Parameters::closest_nodes_size)
          fcn_distance = Address_ ^ new_close_nodes_[Parameters::closest_nodes_size - 1];
        else
          fcn_distance = NodeInNthBucket(Address_, Parameters::closest_nodes_size);
        return (crypto::BigInt(
                    (fcn_distance.ToStringEncoded(Address::EncodingType::kHex) + 'h').c_str()) *
                Parameters::proximity_factor);
      }()) {
}

CheckHoldersResult CloseNodesChange::CheckHolders(const Address& target) const {
  // Handle cases of lower number of group close_nodes nodes
  size_t group_size_adjust(Parameters::kGroupSize + 1U);
  size_t old_holders_size = std::min(old_close_nodes_.size(), group_size_adjust);
  size_t new_holders_size = std::min(new_close_nodes_.size(), group_size_adjust);

  std::vector<Address> old_holders(old_holders_size), new_holders(new_holders_size);

  std::partial_sort_copy(std::begin(old_close_nodes_), std::end(old_close_nodes_),
                         std::begin(old_holders), std::end(old_holders),
                         [target](const Address& lhs, const Address& rhs) {
    return Address::CloserToTarget(lhs, rhs, target);
  });
  std::partial_sort_copy(std::begin(new_close_nodes_), std::end(new_close_nodes_),
                         std::begin(new_holders), std::end(new_holders),
                         [target](const Address& lhs, const Address& rhs) {
    return Address::CloserToTarget(lhs, rhs, target);
  });

  // Remove target == node ids and adjust holder size
  old_holders.erase(std::remove(std::begin(old_holders), std::end(old_holders), target),
                    std::end(old_holders));
  if (old_holders.size() > Parameters::kGroupSize) {
    old_holders.resize(Parameters::kGroupSize);
    assert(old_holders.size() == Parameters::kGroupSize);
  }

  new_holders.erase(std::remove(std::begin(new_holders), std::end(new_holders), target),
                    std::end(new_holders));
  if (new_holders.size() > Parameters::kGroupSize) {
    new_holders.resize(Parameters::kGroupSize);
    assert(new_holders.size() == Parameters::kGroupSize);
  }

  CheckHoldersResult holders_result;
  holders_result.proximity_status = GroupRangeStatus::kOutwithRange;
  if (!new_holders.empty() && ((new_holders.size() < Parameters::kGroupSize) ||
                               Address::CloserToTarget(Address_, new_holders.back(), target))) {
    holders_result.proximity_status = GroupRangeStatus::kInRange;
    if (new_holders.size() == Parameters::kGroupSize)
      new_holders.pop_back();
    new_holders.push_back(Address_);
  }

  if (!old_holders.empty() && Address::CloserToTarget(Address_, old_holders.back(), target)) {
    old_holders.pop_back();
    if (old_holders.size() == Parameters::kGroupSize)
      old_holders.pop_back();
    old_holders.push_back(Address_);
  }

  std::vector<Address> diff_new_holders;
  std::for_each(std::begin(new_holders), std::end(new_holders), [&](const Address& new_holder) {
    if (std::find(std::begin(old_holders), std::end(old_holders), new_holder) ==
        std::end(old_holders))
      diff_new_holders.push_back(new_holder);
  });
  //   holders_result.new_holders = new_holders;
  //   holders_result.old_holders = old_holders;
  if (diff_new_holders.size() > 0)
    holders_result.new_holder = diff_new_holders.front();
  // in case the new_holder is the node itself, it shall be ignored
  if (holders_result.new_holder == Address_)
    holders_result.new_holder = Address();
  return holders_result;
}

Address CloseNodesChange::ChoosePmidNode(const std::set<Address>& online_pmids,
                                        const Address& target) const {
  if (online_pmids.empty())
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::invalid_parameter));

 
  for (const auto& Address : new_close_nodes_)
   
 
  for (const auto& pmid : online_pmids)
   

  // In case storing to PublicPmid, the data shall not be stored on the Vault itself
  // However, the vault will appear in DM's routing table and affect result
  std::vector<Address> temp(Parameters::kGroupSize + 1);
  std::partial_sort_copy(std::begin(new_close_nodes_), std::end(new_close_nodes_), std::begin(temp),
                         std::end(temp), [&target](const Address& lhs, const Address& rhs) {
    return Address::CloserToTarget(lhs, rhs, target);
  });

  auto temp_itr(std::begin(temp));
  auto pmids_itr(std::begin(online_pmids));
  while (*temp_itr != Address_) {
    ++temp_itr;
    //     assert(temp_itr != std::end(temp));
    if (temp_itr == std::end(temp)) {
      LOG(kError) << "Address_ not listed in group range having " << temp.size() << " nodes";
      break;
    }
    if (++pmids_itr == std::end(online_pmids))
      pmids_itr = std::begin(online_pmids);
  }
  return *pmids_itr;
}

void swap(CloseNodesChange& lhs, CloseNodesChange& rhs) MAIDSAFE_NOEXCEPT {
  using std::swap;
  swap(lhs.Address_, rhs.node_id_);
  swap(lhs.old_close_nodes_, rhs.old_close_nodes_);
  swap(lhs.new_close_nodes_, rhs.new_close_nodes_);
  swap(lhs.lost_node_, rhs.lost_node_);
  swap(lhs.new_node_, rhs.new_node_);
  swap(lhs.radius_, rhs.radius_);
}

void CloseNodesChange::Print() const {
  std::stringstream stream;
  for (const auto& Address : old_close_nodes_)
    stream << "\n\t\tentry in old_close_nodes\t------\t" << Address;

  for (const auto& Address : new_close_nodes_)
    stream << "\n\t\tentry in new_close_nodes\t------\t" << Address;

  stream << "\n\t\tentry in lost_node\t------\t" << lost_node_;
  stream << "\n\t\tentry in new_node\t------\t" << new_node_;
 
}

std::string CloseNodesChange::ReportConnection() const {
  std::stringstream stringstream;
  {
    cereal::JSONOutputArchive archive{stringstream};
    if (!lost_node_.IsZero())
      archive(cereal::make_nvp("vaultRemoved",
                               lost_node_.ToStringEncoded(Address::EncodingType::kHex)));
    if (!new_node_.IsZero())
      archive(cereal::make_nvp("vaultAdded",
                               new_node_.ToStringEncoded(Address::EncodingType::kHex)));

    size_t closest_size_adjust(std::min(new_close_nodes_.size(),
                                        static_cast<size_t>(Parameters::kGroupSize + 1U)));

    if (closest_size_adjust != 0) {
      std::vector<Address> closest_nodes(closest_size_adjust);
      std::partial_sort_copy(std::begin(new_close_nodes_), std::end(new_close_nodes_),
                             std::begin(closest_nodes), std::end(closest_nodes),
                             [&](const Address& lhs, const Address& rhs) {
                               return Address::CloserToTarget(lhs, rhs, this->Address_);
                             });
      if (Address_ == closest_nodes.front())
        closest_nodes.erase(std::begin(closest_nodes));
      else if (closest_nodes.size() > Parameters::kGroupSize)
        closest_nodes.pop_back();

      if (closest_nodes.size() != 0) {
        std::vector<std::string> closest_ids;
        for (auto& close_node : closest_nodes)
          closest_ids.emplace_back(close_node.ToStringEncoded(Address::EncodingType::kHex));
        archive(cereal::make_nvp("closeGroupVaults", closest_ids));
      }
    }
  }
  return stringstream.str();
}

}  // namespace routing

}  // namespace maidsafe
