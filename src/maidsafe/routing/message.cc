/* Copyright 2013 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

#include "maidsafe/routing/message.h"

namespace maidsafe {

namespace routing {

GroupSource::GroupSource() : group_id(), sender_id() {}

GroupSource::GroupSource(GroupId group_id_in, SingleId sender_id_in)
    : group_id(group_id_in),
      sender_id(sender_id_in) {}

GroupSource::GroupSource(const GroupSource& other)
    : group_id(other.group_id),
      sender_id(other.sender_id) {}

GroupSource::GroupSource(GroupSource&& other)
    : group_id(std::move(other.group_id)),
      sender_id(std::move(other.sender_id)) {}

GroupSource& GroupSource::operator=(GroupSource other) {
  swap(*this, other);
  return *this;
}

void swap(GroupSource& lhs, GroupSource& rhs) {
  using std::swap;
  swap(lhs.group_id, rhs.group_id);
  swap(lhs.sender_id, rhs.sender_id);
}

}  // namespace routing

}  // namespace maidsafe
