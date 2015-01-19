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
#include "maidsafe/common/node_id.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

// Accumulate data parts with time_to_live LRU-replacement cache
// requires sender id to ensure parts are delivered from different senders
template <typename KeyType, typename ValueType>
class Accumulator {
 public:
  explicit Accumulator(std::chrono::steady_clock::duration time_to_live, uint32_t quorum)
      : time_to_live_(time_to_live), quorum_(quorum) {}

  ~Accumulator() = default;
  Accumulator(const Accumulator&) = delete;
  Accumulator(Accumulator&&) = delete;
  Accumulator& operator=(const Accumulator&) = delete;
  Accumulator& operator=(Accumulator&&) = delete;
  using Map = std::map<NodeId, ValueType>;

  bool HaveKey(KeyType key) { return (storage_.find(key) != std::end(storage_)); }

  bool CheckQuorumReached(KeyType key) {
    auto it = storage_.find(key);
    if (it == std::end(storage_))
      return false;
    return (std::get<0>(it->second).size() >= quorum_);
  }
  // returns true when the quorum has been reached. This will return Quorum times
  // a tuple of of valuetype which should be Source Address signature tag tpye and value
  boost::optional<std::pair<KeyType, Map>> Add(KeyType key, ValueType value, NodeId sender) {
    auto it = storage_.find(key);
    if (it == std::end(storage_)) {
      AddNew(key, value, sender);
      it = storage_.find(key);
    }

    std::get<0>(it->second).insert(std::make_pair(sender, value));
    ReOrder(key);
    if (std::get<0>(it->second).size() >= quorum_) {
      return std::make_pair(it->first, std::get<0>(it->second));
    }
    return {boost::none};
  }

  // this is called when the return from Add returns a type that is incorrect
  // this means a node sent bad data, this method allows all parts to be collected
  // and we can attempt to identify the bad node.
  boost::optional<std::pair<KeyType, Map>> GetAll(const KeyType& key) const {
    auto it = storage_.find(key);
    if (it == std::end(storage_)) {
      return {boost::none};
    }
    return std::make_pair(it->first, std::get<0>(it->second));
  }

  size_t size() const { return storage_.size(); }

 private:
  void AddNew(KeyType key, ValueType value, NodeId sender) {
    // check if we have entries with time expired
    while (CheckTimeExpired())  // any old entries at beginning of the list
      RemoveOldestElement();

    // Record key as most-recently-used key
    auto it = key_order_.insert(std::end(key_order_), key);

    // Create the key-value entry,
    // linked to the usage record.
    Map map;
    map.insert(std::make_pair(sender, value));
    storage_.insert(
        std::make_pair(key, std::make_tuple(map, it, std::chrono::steady_clock::now())));
  }

  void RemoveOldestElement() {
    assert(!key_order_.empty());
    // Identify least recently used key
    const auto it = storage_.find(key_order_.front());
    assert(it != storage_.end());
    // Erase both elements in both containers
    storage_.erase(it);
    key_order_.pop_front();
  }

  bool CheckTimeExpired() const {
    if (time_to_live_ == std::chrono::steady_clock::duration::zero() || storage_.empty())
      return false;
    auto key = storage_.find(key_order_.front());
    assert(key != std::end(storage_) && "cannot find element - should not happen");
    return ((std::get<2>(key->second) + time_to_live_) < (std::chrono::steady_clock::now()));
  }

  void ReOrder(const KeyType& key) {
    const auto it = storage_.find(key);
    assert(it != storage_.end());
    key_order_.splice(key_order_.end(), key_order_, std::get<1>(it->second));
  }

  std::chrono::steady_clock::duration time_to_live_;
  uint32_t quorum_;
  std::list<KeyType> key_order_;
  std::map<KeyType, std::tuple<Map, typename std::list<KeyType>::iterator,
                               std::chrono::steady_clock::time_point>> storage_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ACCUMULATOR_H_
