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

class GetData {
 public:
  GetData() = default;
  ~GetData() = default;

  template <typename T, typename U, typename V>
  GetData(T&& key, U&& requester, V&& tag)
      : key_(std::forward<T>(key)),
        requester_(std::forward<U>(requester)),
        tag_(std::forward<V>(tag)) {}


  GetData(GetData&& other) MAIDSAFE_NOEXCEPT : key_{std::move(other.key_)},
                                               requester_{std::move(other.requester_)},
                                               tag_{std::move(other.tag_)} {}


  GetData& operator=(GetData&& other) MAIDSAFE_NOEXCEPT {
    key_ = std::move(other.key_);
    requester_ = std::move(other.requester_);
    tag_ = std::move(other.tag_);
    return *this;
  }

  GetData(const GetData&) = delete;
  GetData& operator=(const GetData&) = delete;

  void operator()() {}

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(key_, requester_, tag_);
  }
  Identity get_key() { return key_; }
  SourceAddress get_requester() { return requester_; }
  DataTagValue get_tag() { return tag_; }

 private:
  Identity key_;
  SourceAddress requester_;
  DataTagValue tag_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_GET_DATA_H_
