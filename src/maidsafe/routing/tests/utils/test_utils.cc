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

#include <string>
#include <vector>

#include "maidsafe/common/make_unique.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/passport/types.h"

#include "maidsafe/passport/types.h"
#include "maidsafe/passport/passport.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

namespace test {

BootstrapHandler::BootstrapContact CreateBootstrapContact(asymm::PublicKey public_key) {
  if (!asymm::ValidateKey(public_key)) {
    auto keys(asymm::GenerateKeyPair());
    public_key = keys.public_key;
  }
  return BootstrapHandler::BootstrapContact{NodeId(RandomString(NodeId::kSize)),
                                            GetRandomEndpoint(), public_key};
}

std::vector<BootstrapHandler::BootstrapContact> CreateBootstrapContacts(size_t number) {
  auto keys(asymm::GenerateKeyPair());
  std::vector<BootstrapHandler::BootstrapContact> contacts;
  for (size_t i = 0; i != number; ++i)
    contacts.push_back(CreateBootstrapContact(keys.public_key));
  return contacts;
}

std::vector<std::unique_ptr<RoutingTable>> RoutingTableNetwork(size_t size) {
  // passport::PublicPmid fob{passport::Pmid(passport::Anpmid())};
  std::vector<std::unique_ptr<RoutingTable>> routing_tables;
  routing_tables.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    routing_tables.emplace_back(
        maidsafe::make_unique<RoutingTable>(Address(RandomString(Address::kSize))));
  }
  return routing_tables;
}

address_v4 GetRandomIPv4Address() {
  auto address = std::to_string(RandomUint32() % 256);
  for (int i = 0; i != 3; ++i)
    address += '.' + std::to_string(RandomUint32() % 256);
  return asio::ip::make_address_v4(address.c_str());
}

address_v6 GetRandomIPv6Address() {
  std::stringstream address;
  address << std::hex << (RandomUint32() % 65536);
  for (int i = 0; i != 7; ++i)
    address << ':' << RandomUint32() % 65536;
  return asio::ip::make_address_v6(address.str().c_str());
}

rudp::Endpoint GetRandomEndpoint() {
  return rudp::Endpoint{GetRandomIPv4Address(), static_cast<Port>((RandomUint32() % 64512) + 1024)};
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
