/*  Copyright 2015 MaidSafe.net limited

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

#ifndef MAIDSAFE_ROUTING_MESSAGES_GET_CLIENT_KEY_H_
#define MAIDSAFE_ROUTING_MESSAGES_GET_CLIENT_KEY_H_

#include "maidsafe/common/config.h"

#include "maidsafe/routing/types.h"
#include "maidsafe/routing/source_address.h"


namespace maidsafe {

namespace routing {

class GetClientKey {
 public:
  GetClientKey() = default;
  ~GetClientKey() = default;

  GetClientKey(Identity requester, Identity target_id)
      : requester_(std::move(requester)),
        target_id_(std::move(target_id)) {}

  GetClientKey(GetClientKey&& other) MAIDSAFE_NOEXCEPT
      : requester_(std::move(other.requester_)),
        target_id_(std::move(other.target_id_)) {}

  GetClientKey& operator=(GetClientKey&& other) MAIDSAFE_NOEXCEPT {
    requester_ = std::move(other.requester_);
    target_id_ = std::move(other.target_id_);
    return *this;
  }

  GetClientKey(const GetClientKey&) = delete;
  GetClientKey& operator=(const GetClientKey&) = delete;

  template <typename Archive>
  void serialize(Archive& archive) {
      archive(requester_, target_id_);
  }

  Identity requester() const { return requester_; }
  Identity target_id() const { return target_id_; }

 private:
  Identity requester_;
  Identity target_id_;
};

}  // namespace routing

}  // namespace maidsafe

#endif // MAIDSAFE_ROUTING_MESSAGES_GET_CLIENT_KEY_H_
