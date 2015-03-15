/*  Copyright 2014 MaidSafe.net limited

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

#ifndef MAIDSAFE_ROUTING_ACCUMULATOR_H_
#define MAIDSAFE_ROUTING_ACCUMULATOR_H_

#include <cassert>
#include <chrono>
#include <limits>
#include <list>
#include <map>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "boost/optional/optional.hpp"

#include "maidsafe/common/identity.h"

#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

// Accumulate data parts with time_to_live LRU-replacement cache
// requires sender id to ensure parts are delivered from different senders
template <typename NameType, typename ValueType>
class Accumulator {
 public:
  Accumulator(std::chrono::steady_clock::duration time_to_live, uint32_t quorum)
      : time_to_live_(time_to_live), quorum_(quorum) {}

  ~Accumulator() = default;
  Accumulator(const Accumulator&) = delete;
  Accumulator(Accumulator&&) = delete;
  Accumulator& operator=(const Accumulator&) = delete;
  Accumulator& operator=(Accumulator&&) = delete;
  using Map = std::map<Address, ValueType>;

  bool HaveName(NameType name) const { return (storage_.find(name) != std::end(storage_)); }

  bool CheckQuorumReached(NameType name) const {
    auto it = storage_.find(name);
    if (it == std::end(storage_))
      return false;
    return (std::get<0>(it->second).size() >= quorum_);
  }

  // returns true when the quorum has been reached. This will return Quorum times
  // a tuple of valuetype which should be Source Address signature tag type and value
  boost::optional<std::pair<NameType, Map>> Add(const NameType& name, ValueType value,
                                                Address sender) {
    auto it = storage_.find(name);
    if (it == std::end(storage_)) {
      AddNew(name, value, sender);
      it = storage_.find(name);
    }

    std::get<0>(it->second).insert(std::make_pair(std::move(sender), std::move(value)));
    ReOrder(name);
    if (std::get<0>(it->second).size() >= quorum_)
      return std::make_pair(it->first, std::get<0>(it->second));
    return boost::none;
  }

  // this is called when the return from Add returns a type that is incorrect
  // this means a node sent bad data, this method allows all parts to be collected
  // and we can attempt to identify the bad node.
  boost::optional<std::pair<NameType, Map>> GetAll(const NameType& name) const {
    auto it = storage_.find(name);
    if (it == std::end(storage_))
      return boost::none;
    return std::make_pair(it->first, std::get<0>(it->second));
  }

  size_t size() const { return storage_.size(); }

 private:
  void AddNew(NameType name, ValueType value, Address sender) {
    // check if we have entries with time expired
    while (CheckTimeExpired())  // any old entries at beginning of the list
      RemoveOldestElement();

    // Record name as most-recently-used name
    auto it = name_order_.insert(std::end(name_order_), name);

    // Create the name-value entry,
    // linked to the usage record.
    Map map;
    map.insert(std::make_pair(sender, value));
    storage_.insert(
        std::make_pair(name, std::make_tuple(map, it, std::chrono::steady_clock::now())));
  }

  void RemoveOldestElement() {
    assert(!name_order_.empty());
    // Identify least recently used name
    const auto it = storage_.find(name_order_.front());
    assert(it != storage_.end());
    // Erase both elements in both containers
    storage_.erase(it);
    name_order_.pop_front();
  }

  bool CheckTimeExpired() const {
    if (time_to_live_ == std::chrono::steady_clock::duration::zero() || storage_.empty())
      return false;
    auto name = storage_.find(name_order_.front());
    assert(name != std::end(storage_) && "cannot find element - should not happen");
    return ((std::get<2>(name->second) + time_to_live_) < (std::chrono::steady_clock::now()));
  }

  void ReOrder(const NameType& name) {
    const auto it = storage_.find(name);
    assert(it != storage_.end());
    name_order_.splice(name_order_.end(), name_order_, std::get<1>(it->second));
  }

  std::chrono::steady_clock::duration time_to_live_;
  uint32_t quorum_;
  std::list<NameType> name_order_;
  std::map<NameType, std::tuple<Map, typename std::list<NameType>::iterator,
                                std::chrono::steady_clock::time_point>> storage_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ACCUMULATOR_H_
