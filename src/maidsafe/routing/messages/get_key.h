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

#ifndef MAIDSAFE_ROUTING_MESSAGES_GET_KEY_H_
#define MAIDSAFE_ROUTING_MESSAGES_GET_KEY_H_

#include "boost/optional.hpp"

#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

struct GetKey {
  GetKey() = default;
  ~GetKey() = default;

  template <typename T, typename U>
  GetKey(T&& key, U&& relay_node)
      : key{std::forward<T>(key)}, relay_node{std::forward<T>(relay_node)} {}

  template <typename T>
  GetKey(T&& key)
      : key{std::forward<T>(key)} {}

  GetKey(GetKey&& other) MAIDSAFE_NOEXCEPT : key{std::move(other.key)},
                                             relay_node{std::move(other.relay_node)} {}

  GetKey& operator=(GetKey&& other) MAIDSAFE_NOEXCEPT {
    key = std::move(other.key);
    relay_node = std::move(other.relay_node);
    return *this;
  }

  GetKey(const GetKey&) = delete;
  GetKey& operator=(const GetKey&) = delete;

  void operator()() {}

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(key, relay_node);
  }

  KeyType key_requested;
  boost::optional<Address> relay_node;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_GET_KEY_H_
