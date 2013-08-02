/* Copyright 2012 MaidSafe.net limited

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

#ifndef MAIDSAFE_ROUTING_MESSAGE_H_
#define MAIDSAFE_ROUTING_MESSAGE_H_

#include <string>

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/tagged_value.h"

namespace maidsafe {

namespace routing {

typedef TaggedValue <NodeId, struct GroupTag> GroupId;
typedef TaggedValue <NodeId, struct SingleTag> SingleId;

enum class Cacheable : int {
  kNone = 0,
  kGet = 1,
  kPut = 2
};

struct GroupSource {
  GroupId group_id;
  SingleId sender_id;
};

template <typename Sender, typename Receiver>
struct Message {
  std::string contents;
  Sender sender;
  Receiver receiver;
  Cacheable cacheable;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGE_H_
