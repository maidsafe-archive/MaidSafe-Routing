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

#ifndef MAIDSAFE_ROUTING_SENTINEL_H_
#define MAIDSAFE_ROUTING_SENTINEL_H_

#include <vector>
#include <utility>
#include "boost/optional/optional.hpp"
#include "maidsafe/common/rsa.h"
#include "maidsafe/routing/accumulator.h"
#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/messages/messages_fwd.h"

namespace maidsafe {
namespace routing {

class Sentinel {
 public:
  using ResultType = std::tuple<SourceAddress, MessageTypeTag, SerialisedMessage>;
  explicit Sentinel(boost::asio::io_service& io_service) : io_service_(io_service) {}
  Sentinel(const Sentinel&) = delete;
  Sentinel(Sentinel&&) = delete;
  ~Sentinel() = default;
  Sentinel& operator=(const Sentinel&) = delete;
  Sentinel& operator=(Sentinel&& rhs) = delete;
  // at some stage this will return a valid answer when all data is accumulated
  // and signatures checked
  boost::optional<std::future<ResultType>> Add(MessageHeader, MessageTypeTag, SerialisedMessage);
  ResultType AccumulateDirectValue(NodeAddress);
  ResultType AccumulateDhtValue(GroupAddress);
  std::vector<std::pair<asymm::PublicKey, Address>> AccumulateKeys(GroupAddress);

 private:
  boost::asio::io_service& io_service_;
  Accumulator<NodeAddress, ResultType> node_accumulator_{std::chrono::minutes(20), 1U};
  Accumulator<GroupAddress, ResultType> group_accumulator_{std::chrono::minutes(20), QuorumSize};
  Accumulator<GroupAddress, ResultType> group_key_accumulator_{std::chrono::minutes(20),
                                                               QuorumSize};
  Accumulator<NodeAddress, ResultType> node_key_accumulator_{std::chrono::minutes(20), QuorumSize};
};

// TODO(dirvine) move to cc file  :19/01/2015
inline boost::optional<std::future<Sentinel::ResultType>> Sentinel::Add(MessageHeader header,
                                                                        MessageTypeTag tag,
                                                                        SerialisedMessage message) {
  if (tag == MessageTypeTag::GetKeyResponse) {
    if (!header.FromGroup())  // "keys should always come from a group");
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
    if (group_key_accumulator_.Add(*header.FromGroup(),
                                   std::make_tuple(header.GetSource(), tag, std::move(message)),
                                   header.FromNode())) {
      // get the other accumulator and go for it
    }
  } else {
    if (header.FromGroup() && !group_accumulator_.HaveKey(*header.FromGroup())) {  // we need
      // to send a findkey for this GroupAddress

    } else if (!node_accumulator_.HaveKey(header.FromNode())) {  // we need
      // to send a findkey for this SourceAddress
    }
  }

  if (node_key_accumulator_.HaveKey(header.FromNode())) {  // ok direct
  } else if (header.FromGroup() && group_key_accumulator_.HaveKey(*header.FromGroup())) {  // ok dht
  }
  return boost::none;
}



}  // namespace routing
}  // namespace maidsafe


#endif  // MAIDSAFE_ROUTING_SENTINEL_H_
