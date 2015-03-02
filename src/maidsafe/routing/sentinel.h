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

#include <chrono>
#include <future>
#include <vector>
#include <utility>

#include "asio/io_service.hpp"
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
  explicit Sentinel(asio::io_service& io_service) : io_service_(io_service) {}
  Sentinel(const Sentinel&) = delete;
  Sentinel(Sentinel&&) = delete;
  ~Sentinel() = default;
  Sentinel& operator=(const Sentinel&) = delete;
  Sentinel& operator=(Sentinel&&) = delete;
  // at some stage this will return a valid answer when all data is accumulated
  // and signatures checked
  boost::optional<std::future<ResultType>> Add(MessageHeader, MessageTypeTag, SerialisedMessage);
  ResultType AccumulateDirectValue(NodeAddress);
  ResultType AccumulateDhtValue(GroupAddress);
  std::vector<std::pair<asymm::PublicKey, Address>> AccumulateKeys(GroupAddress);

 private:
  asio::io_service& io_service_;
  Accumulator<std::pair<NodeAddress, MessageId>, ResultType> node_accumulator_{std::chrono::minutes(20), 1U};
  // Accumulator<NodeAddress, ResultType> node_key_accumulator_{std::chrono::minutes(20), QuorumSize};
  Accumulator<std::pair<GroupAddress, MessageId>, ResultType> group_accumulator_{std::chrono::minutes(20), QuorumSize};
  Accumulator<std::pair<GroupAddress, MessageId>, ResultType> group_key_accumulator_{std::chrono::minutes(20),
                                                               QuorumSize};
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_SENTINEL_H_
