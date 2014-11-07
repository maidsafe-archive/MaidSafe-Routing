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

#ifndef MAIDSAFE_ROUTING_NODE_INFO_H_
#define MAIDSAFE_ROUTING_NODE_INFO_H_

#include <cstdint>
#include <vector>

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/type_check.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/tagged_value.h"

#include "maidsafe/rudp/nat_type.h"

namespace maidsafe {

namespace routing {

struct NodeInfo {
  NodeInfo() = default;
  NodeInfo(const NodeInfo& other) = default;
  NodeInfo(NodeInfo&& other) = default;
  NodeInfo& operator=(NodeInfo const&) = default;
  NodeInfo& operator=(NodeInfo&&) = default;

  //  for use with std::unique
  bool operator==(const NodeInfo& other) const {
    return id == other.id || asymm::MatchingKeys(public_key, other.public_key);
  }

  bool operator!=(const NodeInfo& other) const { return !operator==(other); }

  bool operator<(const NodeInfo& other) const { return id < other.id; }
  bool operator>(const NodeInfo& other) const { return id > other.id; }
  bool operator<=(const NodeInfo& other) const { return !operator>(other); }
  bool operator>=(const NodeInfo& other) const { return !operator<(other); }

  NonEmptyString Serialise() const;

  NodeId id;
  NodeId connection_id;  // Id of a node as far as rudp is concerned
  asymm::PublicKey public_key;
  int32_t rank;
  int32_t bucket;
  rudp::NatType nat_type;

  static const int32_t kInvalidBucket;
  // static_assert(is_regular<NodeInfo>::value, "Not a regular type");
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_NODE_INFO_H_
