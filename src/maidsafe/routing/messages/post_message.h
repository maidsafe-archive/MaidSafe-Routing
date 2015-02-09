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

#ifndef MAIDSAFE_ROUTING_MESSAGES_POST_MESSAGE_H_
#define MAIDSAFE_ROUTING_MESSAGES_POST_MESSAGE_H_

#include <cstdint>
#include <vector>

#include "maidsafe/common/config.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/types.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/messages/messages_fwd.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

class PostMessage {
 public:
  PostMessage() = default;
  ~PostMessage() = default;

  template <typename T, typename V, typename W>
  PostMessage(T&& key, V&& data, W&& tag)
      : key_(std::forward<T>(key)), data_(std::forward<V>(data)), tag_(std::forward<W>(tag)) {}

  PostMessage(PostMessage&& other) MAIDSAFE_NOEXCEPT : key_{std::move(other.key_)},
                                                       data_{std::move(other.data_)},
                                                       tag_{std::move(other.tag_)} {}

  PostMessage& operator=(PostMessage&& other) MAIDSAFE_NOEXCEPT {
    key_ = std::move(other.key_);
    data_ = std::move(other.data_);
    tag_ = std::move(other.tag_);
    return *this;
  }

  PostMessage(const PostMessage&) = default;
  PostMessage& operator=(const PostMessage&) = default;

  void operator()() {}

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(key_, data_, tag_);
  }

  Identity get_key() { return key_; }
  SerialisedData get_data() { return data_; }
  DataTagValue get_tag() { return tag_; }

 private:
  Identity key_;
  SerialisedData data_;
  DataTagValue tag_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_POST_MESSAGE_H_
