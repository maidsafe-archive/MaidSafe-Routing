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

Firewall::Firewall()
    : mutex_(), history_() {}

bool Firewall::Add(const NodeId& source_id, int32_t message_id) {
  if (source_id.IsZero())
    return false;
  std::unique_lock<std::mutex> lock(mutex_);
  if (std::any_of(std::begin(history_), std::end(history_),
                  [&](const ProcessedMessageInfo& processed_message)->bool {
                    return ((processed_message.source == source_id) &&
                            (processed_message.id == message_id));
                  }))
    return false;

  history_.push_back(ProcessedMessageInfo(source_id, message_id));
  if (history_.size() > Parameters::max_firewall_history_size)
    history_.pop_front();
  return true;
}

}  // namespace routing

}  // namespace maidsafe

