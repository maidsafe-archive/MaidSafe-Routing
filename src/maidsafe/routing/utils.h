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

#ifndef MAIDSAFE_ROUTING_UTILS_H_
#define MAIDSAFE_ROUTING_UTILS_H_

#include <vector>
#include <utility>

#include "cereal/types/array.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/types.h"
#include "maidsafe/rudp/contact.h"
#include "maidsafe/rudp/types.h"

#include "maidsafe/routing/types.h"
#include "maidsafe/routing/message_header.h"

namespace maidsafe {

namespace routing {}  // namespace routing

}  // namespace maidsafe

namespace cereal {

template <typename Archive>
void save(Archive& archive, const maidsafe::rudp::Endpoint& endpoint) {
  using address_v6 = asio::ip::address_v6;
  address_v6 ip_address;
  if (endpoint.address().is_v4()) {
    ip_address = address_v6::v4_compatible(endpoint.address().to_v4());
  } else {
    ip_address = endpoint.address().to_v6();
  }
  address_v6::bytes_type bytes = ip_address.to_bytes();
  archive(bytes, endpoint.port());
}

template <typename Archive>
void load(Archive& archive, maidsafe::rudp::Endpoint& endpoint) {
  using address_v6 = asio::ip::address_v6;
  using address = asio::ip::address;
  address_v6::bytes_type bytes;
  maidsafe::routing::Port port;
  archive(bytes, port);
  address_v6 ip_v6_address(bytes);
  address ip_address;
  if (ip_v6_address.is_v4_compatible())
    ip_address = ip_v6_address.to_v4();
  else
    ip_address = ip_v6_address;
  endpoint = maidsafe::rudp::Endpoint(ip_address, port);
}

template <typename Archive>
void serialize(Archive& archive, maidsafe::rudp::EndpointPair& endpoints) {
  archive(endpoints.local, endpoints.external);
}

template <typename Archive>
void serialize(Archive& archive, maidsafe::rudp::Contact& contact) {
  archive(contact.id, contact.endpoint_pair, contact.public_key);
}

}  // namespace cereal

#endif  // MAIDSAFE_ROUTING_UTILS_H_
