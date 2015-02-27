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

#include "maidsafe/common/test.h"
#include "maidsafe/routing/tests/utils/fake_vault_facade.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST(RoutingFakeVaultFacadeTest, FUNC_Constructor) {
  ASSERT_NO_THROW(vault::test::FakeVaultFacade vault);
}

TEST(RoutingFakeVaultFacadeTest, FUNC_Put) {
  std::vector<std::pair<vault::test::FakeVaultFacade, unsigned short>> vaults(3);
  unsigned short port(5483);
  for (auto& vault : vaults)
    vault.second = port++;
  for (auto& vault : vaults)
    ASSERT_NO_THROW(vault.first.StartAccepting(vault.second));

  for (size_t i = 0; i != vaults.size(); ++i) {
    for (size_t j = 0; j != vaults.size(); ++j) {
      if (j > i) {
        asio::ip::udp::endpoint endpoint(asio::ip::udp::v4(), vaults[j].second);
        vaults[i].first.AddContact(endpoint);
      }
    }
  }
  Sleep(std::chrono::seconds(10));
  std::cout << "test end" << std::endl;
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
