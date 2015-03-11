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

#ifndef MAIDSAFE_ROUTING_MESSAGES_GET_CLIENT_KEY_RESPONSE_H_
#define MAIDSAFE_ROUTING_MESSAGES_GET_CLIENT_KEY_RESPONSE_H_

#include "maidsafe/common/config.h"

#include "maidsafe/routing/types.h"
#include "maidsafe/routing/source_address.h"

namespace maidsafe {

namespace routing {

class GetClientKeyResponse {
 public:
  GetClientKeyResponse() = default;
  ~GetClientKeyResponse() = default;

  GetClientKeyResponse(SerialisedMessage serialied_public_key)
      : serialied_public_key_(std::move(serialied_public_key)) {}

  GetClientKeyResponse(GetClientKeyResponse&& other) MAIDSAFE_NOEXCEPT
      : serialied_public_key_(std::move(other.serialied_public_key_)) {}

  GetClientKeyResponse& operator=(GetClientKeyResponse&& other) MAIDSAFE_NOEXCEPT {
    serialied_public_key_ = std::move(other.serialied_public_key_);
    return *this;
  }

  GetClientKeyResponse(const GetClientKeyResponse&) = delete;
  GetClientKeyResponse& operator=(const GetClientKeyResponse&) = delete;

  template <typename Archive>
  void serialize(Archive& archive) {
      archive(serialied_public_key_);
  }

  SerialisedMessage serialied_public_key() const { return serialied_public_key_; }

 private:
  SerialisedMessage serialied_public_key_;
};

}  // namespace routing

}  // namespace maidsafe


#endif // MAIDSAFE_ROUTING_MESSAGES_GET_CLIENT_KEY_RESPONSE_H_
