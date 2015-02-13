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

#ifndef MAIDSAFE_ROUTING_MESSAGES_FIND_GROUP_H_
#define MAIDSAFE_ROUTING_MESSAGES_FIND_GROUP_H_

#include "maidsafe/common/config.h"

#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

class FindGroup {
 public:
  FindGroup() = default;
  ~FindGroup() = default;

  FindGroup(NodeAddress requester_id, Address target_id)
      : requester_id_(std::move(requester_id)), target_id_(std::move(target_id)) {}

  FindGroup(FindGroup&& other) MAIDSAFE_NOEXCEPT : requester_id_(std::move(other.requester_id_)),
                                                   target_id_(std::move(other.target_id_)) {}

  FindGroup& operator=(FindGroup&& other) MAIDSAFE_NOEXCEPT {
    requester_id_ = std::move(other.requester_id_);
    target_id_ = std::move(other.target_id_);
    return *this;
  }

  FindGroup(const FindGroup&) = delete;
  FindGroup& operator=(const FindGroup&) = delete;

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(requester_id_, target_id_);
  }

  NodeAddress requester_id() const { return requester_id_; }
  Address target_id() const { return target_id_; }

 private:
  NodeAddress requester_id_;
  Address target_id_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_FIND_GROUP_H_
