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

#ifndef MAIDSAFE_ROUTING_MESSAGES_FORWARD_CONNECT_H_
#define MAIDSAFE_ROUTING_MESSAGES_FORWARD_CONNECT_H_

#include "maidsafe/common/config.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/compile_time_mapper.h"
#include "maidsafe/rudp/contact.h"

#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/utils.h"
#include "maidsafe/routing/messages/connect.h"
#include "maidsafe/routing/messages/messages_fwd.h"

namespace maidsafe {

namespace routing {

struct ForwardConnect {
  ForwardConnect() = default;
  ~ForwardConnect() = default;

  template<typename T>
  ForwardConnect(T&& requester_in) : requester{std::forward<T>(requester_in)} {}

  ForwardConnect(ForwardConnect&& other) MAIDSAFE_NOEXCEPT
      : requester{std::move(other.requester)} {}

  ForwardConnect& operator=(ForwardConnect&& other) MAIDSAFE_NOEXCEPT {
    requester = std::move(other.requester);
    return *this;
  }

  ForwardConnect(const ForwardConnect&) = delete;
  ForwardConnect& operator=(const ForwardConnect&) = delete;

  void operator()() {

  }

  template<typename Archive>
  void serialize(Archive& archive) {
    archive(requester);
  }

  rudp::Contact requester;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_FORWARD_CONNECT_H_
