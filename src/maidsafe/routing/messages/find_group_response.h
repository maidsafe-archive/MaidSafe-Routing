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

#include <vector>

#include "maidsafe/common/rsa.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/messages/find_group.h"
#include "maidsafe/passport/types.h"
#include "maidsafe/passport/passport.h"

namespace maidsafe {

namespace routing {

class FindGroupResponse {
 public:
  FindGroupResponse() = default;
  ~FindGroupResponse() = default;

  FindGroupResponse(Address target_id, std::vector<NodeInfo> node_info)
      : target_id_(std::move(target_id)), node_infos_(std::move(node_info)) {}

  FindGroupResponse(FindGroupResponse&& other) MAIDSAFE_NOEXCEPT
      : target_id_(std::move(other.target_id_)),
        node_infos_(std::move(other.node_infos_)) {}

  FindGroupResponse& operator=(FindGroupResponse&& other) MAIDSAFE_NOEXCEPT {
    target_id_ = std::move(other.target_id_);
    node_infos_ = std::move(other.node_infos_);
    return *this;
  }

  FindGroupResponse(const FindGroupResponse&) = delete;
  FindGroupResponse& operator=(const FindGroupResponse&) = delete;

  template <typename Archive>
  void serialize(Archive& archive) {
    archive(target_id_, node_infos_);
  }

  Address target_id() const { return target_id_; }
  std::vector<NodeInfo> node_infos() const { return node_infos_; }

 private:
  Address target_id_;
  std::vector<NodeInfo> node_infos_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_FIND_GROUP_RESPONSE_H_
