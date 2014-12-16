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

#include "maidsafe/routing/firewall.h"

#include "maidsafe/routing/parameters.h"

namespace maidsafe {

namespace routing {

bool operator < (const Firewall::ProcessedEntry& lhs, const Firewall::ProcessedEntry& rhs) {
  return (lhs.message_id == rhs.message_id) ? lhs.source < rhs.source
                                            : lhs.message_id < rhs.message_id;
}

Firewall::Firewall()
    : mutex_(), history_() {}

bool Firewall::Add(const NodeId& source_id, int32_t message_id) {
  if (!source_id.IsValid())
    return false;

  std::unique_lock<std::mutex> lock(mutex_);
  auto entry(ProcessedEntry(source_id, message_id));
  auto found(history_.find(entry));
  if (found != std::end(history_))
    return false;

  history_.insert(entry);
  if (history_.size() % Parameters::firewall_history_cleanup_factor == 0)
    Remove(lock);

  return true;
}

void Firewall::Remove(std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());
  static_cast<void>(lock);
  using  accounts_by_update_time = boost::multi_index::index<ProcessedEntrySet, BirthTimeTag>::type;
  accounts_by_update_time& birth_time_index =
      boost::multi_index::get<BirthTimeTag>(history_);
  ProcessedEntry dummy(NodeId(RandomString(NodeId::kSize)), RandomInt32());
  auto upper(std::upper_bound(
      std::begin(birth_time_index), std::end(birth_time_index), dummy,
      [this](const ProcessedEntry& lhs, const ProcessedEntry& rhs) {
        return (lhs.birth_time - rhs.birth_time < Parameters::firewall_message_life);
      }));
  if (upper != std::end(birth_time_index))
    birth_time_index.erase(std::begin(birth_time_index), upper);
}

}  // namespace routing

}  // namespace maidsafe
