/*  Copyright 2012 MaidSafe.net limited

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

#ifndef MAIDSAFE_ROUTING_SOURCE_ADDRESS_H_
#define MAIDSAFE_ROUTING_SOURCE_ADDRESS_H_

#include <tuple>

#include "boost/optional/optional.hpp"

#include "maidsafe/common/config.h"

#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

struct SourceAddress {
  SourceAddress() : node_address(), group_address(), reply_to_address() {}
  SourceAddress(NodeAddress node_address_in, boost::optional<GroupAddress> group_address_in,
                boost::optional<ReplyToAddress> reply_to_address_in)
      : node_address(std::move(node_address_in)),
        group_address(std::move(group_address_in)),
        reply_to_address(std::move(reply_to_address_in)) {}
  SourceAddress(const SourceAddress&) = default;
  SourceAddress(SourceAddress&& other) MAIDSAFE_NOEXCEPT
      : node_address(std::move(other.node_address)),
        group_address(std::move(other.group_address)),
        reply_to_address(std::move(other.reply_to_address)) {}
  SourceAddress& operator=(const SourceAddress&) = default;
  SourceAddress& operator=(SourceAddress&& other) MAIDSAFE_NOEXCEPT {
    node_address = std::move(other.node_address);
    group_address = std::move(other.group_address);
    reply_to_address = std::move(other.reply_to_address);
    return *this;
  }
  template <class Archive>
  void serialize(Archive& archive) {
    archive(node_address, group_address, reply_to_address);
  }

  NodeAddress node_address;
  boost::optional<GroupAddress> group_address;
  boost::optional<ReplyToAddress> reply_to_address;
};

inline bool operator==(const SourceAddress& lhs, const SourceAddress& rhs) {
  return lhs.node_address == rhs.node_address && lhs.group_address == rhs.group_address &&
         lhs.reply_to_address == rhs.reply_to_address;
}

inline bool operator!=(const SourceAddress& lhs, const SourceAddress& rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator<(const SourceAddress& lhs, const SourceAddress& rhs) {
  return std::tie(lhs.node_address, lhs.group_address, lhs.reply_to_address) <
         std::tie(rhs.node_address, rhs.group_address, rhs.reply_to_address);
}

inline bool operator>(const SourceAddress& lhs, const SourceAddress& rhs) {
  return operator<(rhs, lhs);
}

inline bool operator<=(const SourceAddress& lhs, const SourceAddress& rhs) {
  return !operator>(lhs, rhs);
}

inline bool operator>=(const SourceAddress& lhs, const SourceAddress& rhs) {
  return !operator<(lhs, rhs);
}

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_SOURCE_ADDRESS_H_
