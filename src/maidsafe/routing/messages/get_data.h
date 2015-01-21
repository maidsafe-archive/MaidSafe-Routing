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

#ifndef MAIDSAFE_ROUTING_MESSAGES_GET_DATA_H_
#define MAIDSAFE_ROUTING_MESSAGES_GET_DATA_H_

#include "boost/optional.hpp"

#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

struct GetData {
  GetData() = default;
  ~GetData() = default;

  template <typename T, typename U>
  GetData(T&& key, U&& relay_node)
      : key(std::forward<T>(key)), relay_node(std::forward<T>(relay_node)) {}

  // 2015-01-21 ned:
  // VS2013 has an overload resolution bug where any T&& overload will be chosen
  // preferentially to a direct match i.e. this disables the move constructor below without
  // the enable_if.
  template <typename T, typename U =
    typename std::enable_if<!std::is_base_of<GetData,
      typename std::decay<T>::type>::value, T>::type>
  GetData(T&& key)
      : key(std::forward<U>(key)) {}

  GetData(GetData&& other) MAIDSAFE_NOEXCEPT : key{std::move(other.key)},
                                               relay_node{std::move(other.relay_node)} {}

  GetData& operator=(GetData&& other) MAIDSAFE_NOEXCEPT {
    key = std::move(other.key);
    relay_node = std::move(other.relay_node);
    return *this;
  }

  GetData(const GetData&) = delete;
  GetData& operator=(const GetData&) = delete;

  void operator()() {}

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(key, relay_node);
  }

  Address key;
  boost::optional<Address> relay_node;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_GET_DATA_H_
