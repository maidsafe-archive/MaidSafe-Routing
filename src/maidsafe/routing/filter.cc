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
    use of the MaidSafe Software.
 */

#include "maidsafe/routing/filter.h"

#include <vector>
#include <utility>
#include <chrono>
#include <algorithm>

#include "maidsafe/routing/message_header.h"


namespace maidsafe {
namespace routing {

void Filter::Block(MessageHeader header) {
  if (!check(header)) {
    std::lock_guard<std::mutex> lock(mutex_);
    messages_.push_back({header, std::chrono::system_clock::now()});
  }
  Purge();
}

bool Filter::check(const MessageHeader& header) {
  std::lock_guard<std::mutex> lock(mutex_);
  return std::find_any(
      std::begin(messages_), std::end(mesages_),
      [](const std::pair < header, std::chrono::time_point<std::chrono::system_clock> &
                                       item) { return header == item.first; });
}

void Filter::Purge() {
  messages_.erase(std::remove_if(std::begin(messages_), std::end(messages_),
                                 [this](const std::pair < header,
                                        std::chrono::time_point<std::chrono::system_clock> & item) {
                    return item.second + time_to_live > std::chrono::system_clock::now();
                  }),
                  std::end(messages_));
}



}  // namespace routing
}  // namespace maidsafe
