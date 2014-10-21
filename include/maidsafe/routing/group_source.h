/*  Copyright 2013 MaidSafe.net limited

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

#ifndef MAIDSAFE_ROUTING_GROUP_SOURCE_H_
#define MAIDSAFE_ROUTING_GROUP_SOURCE_H_

#include <algorithm>
#include <string>
#include <utility>

#include "maidsafe/common/config.h"
#include "maidsafe/common/type_check.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/tagged_value.h"

namespace maidsafe {

namespace routing {

using GroupId = TaggedValue<NodeId, struct GroupTag>;
using SingleId = TaggedValue<NodeId, struct SingleTag>;
using SingleSource = TaggedValue<NodeId, struct SingleSourceTag>;

enum class Cacheable : int { kNone, kGet, kPut };

struct GroupSource {
  GroupSource() = default;
  GroupSource(GroupId group_id_in, SingleId sender_id_in)
      : group_id(std::move(group_id_in)), sender_id(std::move(sender_id_in)) {}
  GroupSource(const GroupSource& other) = default;
  GroupSource(GroupSource&& other) MAIDSAFE_NOEXCEPT : group_id(std::move(other.group_id)),
                                                       sender_id(std::move(other.sender_id)) {}
  GroupSource& operator=(const GroupSource& other) = default;

  bool operator==(const GroupSource& other) const MAIDSAFE_NOEXCEPT {
    return std::tie(group_id, sender_id) == std::tie(other.group_id, other.sender_id);
  }
  bool operator!=(const GroupSource& other) const MAIDSAFE_NOEXCEPT { return !operator==(other); }

  bool operator<(const GroupSource& other) const MAIDSAFE_NOEXCEPT {
    return std::tie(group_id, sender_id) < std::tie(other.group_id, other.sender_id);
  }

  GroupId group_id;
  SingleId sender_id;
};

static_assert(is_regular<GroupSource>::value, "Not a regular type");
}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGE_H_
