/*  Copyright 2015 MaidSafe.net limited

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

#include "boost/optional/optional.hpp"

#include "maidsafe/common/config.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/passport/types.h"

#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

struct NodeInfo {
  using PublicPmid = passport::PublicPmid;

  NodeInfo(Address id_in, PublicPmid dht_fob_in, bool connected_in)
      : id(id_in), dht_fob(std::move(dht_fob_in)), connected(connected_in) {}

  NodeInfo() : id(), dht_fob(), connected(false) {}

  NodeInfo(const NodeInfo&) = default;

  NodeInfo(NodeInfo&& other) MAIDSAFE_NOEXCEPT : id(std::move(other.id)),
                                                 dht_fob(std::move(other.dht_fob)),
                                                 connected(std::move(other.connected)) {}

  NodeInfo& operator=(const NodeInfo&) = default;

  NodeInfo& operator=(NodeInfo&& other) MAIDSAFE_NOEXCEPT {
    id = std::move(other.id);
    dht_fob = std::move(other.dht_fob);
    connected = std::move(other.connected);
    return *this;
  }

  bool operator==(const NodeInfo& other) const { return id == other.id; }
  bool operator!=(const NodeInfo& other) const { return id != other.id; }
  bool operator<(const NodeInfo& other) const { return id < other.id; }
  bool operator>(const NodeInfo& other) const { return id > other.id; }
  bool operator<=(const NodeInfo& other) const { return id <= other.id; }
  bool operator>=(const NodeInfo& other) const { return id >= other.id; }

  Address id;
  PublicPmid dht_fob;
  bool connected;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_NODE_INFO_H_
