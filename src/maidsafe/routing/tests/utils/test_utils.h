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

#ifndef MAIDSAFE_ROUTING_TESTS_UTILS_TEST_UTILS_H_
#define MAIDSAFE_ROUTING_TESTS_UTILS_TEST_UTILS_H_

#include <cstdint>
#include <string>
#include <memory>
#include <vector>

#include "boost/asio/ip/address.hpp"
#include "boost/asio/ip/udp.hpp"

#include "maidsafe/common/rsa.h"
#include "maidsafe/passport/passport.h"
#include "maidsafe/rudp/types.h"
#include "maidsafe/rudp/contact.h"

#include "maidsafe/routing/bootstrap_handler.h"

namespace maidsafe {

namespace routing {

class RoutingTable;

namespace test {

using address_v6 = asio::ip::address_v6;
using address_v4 = asio::ip::address_v4;
using address = asio::ip::address;

inline passport::PublicPmid PublicFob() {
  return passport::PublicPmid{passport::Pmid(passport::Anpmid())};
}
BootstrapHandler::BootstrapContact CreateBootstrapContact(
    asymm::PublicKey public_key = asymm::PublicKey());

std::vector<BootstrapHandler::BootstrapContact> CreateBootstrapContacts(size_t number);

std::vector<std::unique_ptr<RoutingTable>> RoutingTableNetwork(size_t size);

address_v4 GetRandomIPv4Address();

address_v6 GetRandomIPv6Address();

rudp::Endpoint GetRandomEndpoint();

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TESTS_UTILS_TEST_UTILS_H_
