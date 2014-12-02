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

#ifndef MAIDSAFE_ROUTING_FILTER_H_
#define MAIDSAFE_ROUTING_FILTER_H_

#include <chrono>
#include <mutex>
#include <utility>
#include <vector>

#include "maidsafe/routing/message_header.h"

namespace maidsafe {

namespace routing {

class Filter {
 public:
  static const std::chrono::seconds kTimeToLive;
  Filter() = default;
  Filter(Filter const&) = delete;
  Filter(Filter&&) = delete;
  ~Filter() = default;
  Filter& operator=(const Filter&) = delete;
  Filter& operator=(Filter&&) = delete;
  void Block(MessageHeader header);
  bool Check(const MessageHeader&) const;

 private:
  void Purge();

  std::vector<std::pair<MessageHeader, std::chrono::time_point<std::chrono::system_clock>>>
      messages_;
  mutable std::mutex mutex_;
};


}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_FILTER_H_
