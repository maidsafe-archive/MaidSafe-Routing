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

#ifndef MAIDSAFE_ROUTING_MESSAGES_FIND_GROUP_RESPONSE_H_
#define MAIDSAFE_ROUTING_MESSAGES_FIND_GROUP_RESPONSE_H_

#include "boost/optional.hpp"

#include "maidsafe/common/rsa.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

struct FindGroupResponse {
  FindGroupResponse() = default;
  ~FindGroupResponse() = default;


  template <typename T, typename U, typename V>
  FindGroupResponse(T&& requester_id, U&& close_node_id, V&& relay_node)
      : requester_id{std::forward<T>(requester_id)},
        close_node_id{std::forward<U>(close_node_id)},
        relay_node{std::forward<V>(relay_node)} {}

  template <typename T, typename U>
  FindGroupResponse(T&& requester_id, U&& close_node_id)
      : requester_id{std::forward<T>(requester_id)},
        close_node_id{std::forward<U>(close_node_id)} {}

  FindGroupResponse(FindGroupResponse&& other) MAIDSAFE_NOEXCEPT
      : requester_id{std::move(other.requester_id)},
        close_node_id{std::move(other.close_node_id)},
        relay_node{std::move(other.relay_node)} {}

  FindGroupResponse& operator=(FindGroupResponse&& other) MAIDSAFE_NOEXCEPT {
    requester_id = std::move(other.requester_id);
    close_node_id = std::move(other.close_node_id);
    relay_node = std::move(other.relay_node);
    return *this;
  }

  FindGroupResponse(const FindGroupResponse&) = delete;
  FindGroupResponse& operator=(const FindGroupResponse&) = delete;

  void operator()() {}

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(requester_id, close_node_id, relay_node);
  }

  Address requester_id;
  Address close_node_id;
  boost::optional<Address> relay_node;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_FIND_GROUP_RESPONSE_H_
