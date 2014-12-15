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

#include "maidsafe/routing/tests/utils/test_utils.h"

#include <vector>

#include "maidsafe/common/utils.h"
#include "maidsafe/common/make_unique.h"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

namespace test {

BootstrapHandler::BootstrapContact CreateBootstrapContact() {
  auto keys(asymm::GenerateKeyPair());
  return std::make_tuple(
      NodeId(RandomString(NodeId::kSize)), keys.public_key,
      Endpoint(asio::ip::address::from_string("1.1.1.1"), (RandomUint32() + 1) % 65536));
}

std::vector<BootstrapHandler::BootstrapContact> CreateBootstrapContacts(size_t number) {
  return std::vector<BootstrapHandler::BootstrapContact>{number, CreateBootstrapContact()};
}

std::vector<std::unique_ptr<RoutingTable>> RoutingTableNetwork(size_t size) {
  asymm::Keys keys(asymm::GenerateKeyPair());
  std::vector<std::unique_ptr<RoutingTable>> routing_tables;
  routing_tables.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    routing_tables.emplace_back(
        maidsafe::make_unique<RoutingTable>(Address(RandomString(Address::kSize))));
  }
  return routing_tables;
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
