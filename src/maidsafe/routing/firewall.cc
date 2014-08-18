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

const int Firewall::kQueueSize_ = 1000;

Firewall::Firewall()
    : mutex_(), history_() {}

bool Firewall::Add(const NodeId& source_id, const uint32_t& message_id) {
  if (source_id.IsZero())
    return false;
  std::unique_lock<std::mutex> lock(mutex_);
  auto found(std::find_if(history_.begin(),
                          history_.end(),
                          [&](const ProcessedMessage& processed_message)->bool {
                            return ((std::get<0>(processed_message) == source_id) &&
                                    (std::get<1>(processed_message) == message_id));
                          }));

  if (found != std::end(history_))
    return false;

  history_.push_back(std::make_tuple(source_id, message_id, std::time(NULL)));
  if (history_.size() > kQueueSize_)
    history_.pop_front();
  return true;
}

void Firewall::Remove(std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());
//  static_cast<void>(lock);
//  std::time_t now(std::time(NULL));

//  auto old_entry(std::find_if(history_.begin(), history_.end(),
//                              [&](const ProcessedMessage& processed_message) {
//                                return ((now - std::get<2>(processed_message)) <
//                                        Parameters::message_age_to_drop);
//                              }));

//  if (old_entry != history_.begin())
//    history_.erase(history_.begin(), old_entry);
}

}  // namespace routing

}  // namespace maidsafe

