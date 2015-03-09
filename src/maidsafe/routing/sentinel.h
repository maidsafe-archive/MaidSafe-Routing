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
  // TODO(mmoadeli): ResultType below may have extra information which could be removed later
  using ResultType = std::tuple<MessageHeader, MessageTypeTag, SerialisedMessage>;
  Sentinel(SendGetClientKey send_get_client_key, SendGetGroupKey send_get_group_key)
      : send_get_client_key_(send_get_client_key), send_get_group_key_(send_get_group_key) {}
  Sentinel(const Sentinel&) = delete;
  Sentinel(Sentinel&&) = delete;
  ~Sentinel() = default;
  Sentinel& operator=(const Sentinel&) = delete;
  Sentinel& operator=(Sentinel&&) = delete;
  // at some stage this will return a valid answer when all data is accumulated
  // and signatures checked
  boost::optional<ResultType> Add(MessageHeader, MessageTypeTag, SerialisedMessage);

 private:
  using NodeKeyType = std::pair<NodeAddress, routing::MessageId>;
  using GroupKeyType = std::pair<GroupAddress, routing::MessageId>;
  using NodeAccumulatorType = Accumulator<NodeKeyType, ResultType>;
  using GroupAccumulatorType = Accumulator<GroupKeyType, ResultType>;
  using KeyAccumulatorType = Accumulator<GroupAddress, ResultType>;

  template <typename AccumulatorType, typename AccumulatorKeyType>
  boost::optional<ResultType> Validate(const typename AccumulatorType::Map& messages,
                                       const typename AccumulatorKeyType::Map& /*keys*/) {
    return messages.begin()->second;
  }

  SendGetClientKey send_get_client_key_;
  SendGetGroupKey send_get_group_key_;
  NodeAccumulatorType node_accumulator_{std::chrono::minutes(20), 1U};
  GroupAccumulatorType group_accumulator_{std::chrono::minutes(20), QuorumSize};
  KeyAccumulatorType group_key_accumulator_{std::chrono::minutes(20), QuorumSize};
  KeyAccumulatorType node_key_accumulator_{std::chrono::minutes(20), 1U};
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_SENTINEL_H_
