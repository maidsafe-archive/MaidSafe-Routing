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

#ifndef MAIDSAFE_ROUTING_MESSAGE_H_
#define MAIDSAFE_ROUTING_MESSAGE_H_

#include <string>
#include <utility>

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/tagged_value.h"

namespace maidsafe {

namespace routing {

typedef TaggedValue <NodeId, struct GroupTag> GroupId;
typedef TaggedValue <NodeId, struct SingleTag> SingleId;
typedef TaggedValue <NodeId, struct SingleSourceTag> SingleSource;

enum class Cacheable : int {
  kNone = 0,
  kGet = 1,
  kPut = 2
};

struct GroupSource {
  GroupSource();
  GroupSource(GroupId group_id_in, SingleId sender_id_in);
  GroupSource(const GroupSource& other);
  GroupSource(GroupSource&& other);
  GroupSource& operator=(GroupSource other);

  GroupId group_id;
  SingleId sender_id;
};

void swap(GroupSource& lhs, GroupSource& rhs);

template <typename Sender, typename Receiver>
struct Message {
  Message();
  Message(const std::string& contents_in,
          const Sender& sender_in,
          const Receiver& receiver_in,
          Cacheable cacheable_in);
  Message(const Message& other);
  Message(Message&& other);
  Message& operator=(Message other);

  std::string contents;
  Sender sender;
  Receiver receiver;
  Cacheable cacheable;
};

template <typename Sender, typename Receiver>
void swap(Message<Sender, Receiver>& lhs, Message<Sender, Receiver>& rhs);



// ==================== Implementation =============================================================
template <typename Sender, typename Receiver>
Message<Sender, Receiver>::Message()
    : contents(),
      sender(),
      receiver(),
      cacheable(Cacheable::kNone) {}

template <typename Sender, typename Receiver>
Message<Sender, Receiver>::Message(const std::string& contents_in,
                                   const Sender& sender_in,
                                   const Receiver& receiver_in,
                                   Cacheable cacheable_in)
    : contents(contents_in),
      sender(sender_in),
      receiver(receiver_in),
      cacheable(cacheable_in) {}

template <typename Sender, typename Receiver>
Message<Sender, Receiver>::Message(const Message& other)
    : contents(other.contents),
      sender(other.sender),
      receiver(other.receiver),
      cacheable(other.cacheable) {}

template <typename Sender, typename Receiver>
Message<Sender, Receiver>::Message(Message&& other)
    : contents(std::move(other.contents)),
      sender(std::move(other.sender)),
      receiver(std::move(other.receiver)),
      cacheable(std::move(other.cacheable)) {}

template <typename Sender, typename Receiver>
Message<Sender, Receiver>& Message<Sender, Receiver>::operator=(Message other) {
  swap(*this, other);
  return *this;
}

template <typename Sender, typename Receiver>
void swap(Message<Sender, Receiver>& lhs, Message<Sender, Receiver>& rhs) {
  using std::swap;
  swap(lhs.contents, rhs.contents);
  swap(lhs.sender, rhs.sender);
  swap(lhs.receiver, rhs.receiver);
  swap(lhs.cacheable, rhs.cacheable);
}

typedef Message<SingleSource, SingleId> SingleToSingleMessage;
typedef Message<SingleSource, GroupId> SingleToGroupMessage;
typedef Message<GroupSource, SingleId> GroupToSingleMessage;
typedef Message<GroupSource, GroupId> GroupToGroupMessage;

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGE_H_
