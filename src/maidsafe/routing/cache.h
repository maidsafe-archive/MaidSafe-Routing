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

#ifndef MAIDSAFE_ROUTING_CACHE_H_
#define MAIDSAFE_ROUTING_CACHE_H_

#include "maidsafe/common/clock.h"
#include <chrono>
#include "maidsafe/routing/message_header.h"

namespace maidsafe {
namespace routing {

class Cache {
 public:
  static const std::chrono::minutes time_to_live(60);
  Cache() = default;
  Cache(Cache const&) = default;
  Cache(Cache&&) = delete;
  ~Cache() = default;
  Cache& operator=(Cache const&) = default;
  Cache& operator=(Cache&& rhs) = delete;
  void Block(MessageHeader header);
  bool Check(const MessageHeader&);

 private:
  void Purge();

  std::vector<std::pair<MessageHeader, std::chrono::time_point<std::chrono::system_clock>>>
      messages_;
  mutable std::mutex mutex_;
};


}  // namespace routing
}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_CACHE_H_
