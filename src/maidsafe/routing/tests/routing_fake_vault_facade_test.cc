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
  vault::test::FakeVaultFacade vault1;
  Sleep(std::chrono::seconds(2));
  vault::test::FakeVaultFacade vault2;
  //ASSERT_NO_THROW(vault::test::FakeVaultFacade vault);
  Sleep(std::chrono::seconds(20));
}

//TEST(RoutingFakeVaultFacadeTest, FUNC_PutGet) {
//  using endpoint = asio::ip::udp::endpoint;
//  using address = asio::ip::address_v4;
//  vault::test::FakeVaultFacade facade1;
//  std::vector<std::pair<vault::test::FakeVaultFacade, unsigned short>> vaults(2);
//  unsigned short port(5483);
//  for (auto& vault : vaults)
//    vault.second = port++;
//  for (auto& vault : vaults)
//    ASSERT_NO_THROW(vault.first.StartAccepting(vault.second));

//  ASSERT_GE(vaults.size(), 2);

//  for (size_t i = 0; i != vaults.size() - 1; ++i)
//  for (size_t j = i + 1; j != vaults.size(); ++j)
//    ASSERT_NO_THROW(vaults[j].first.AddContact(endpoint(address::loopback(), vaults[i].second)));

//  auto vault_index(RandomUint32() % vaults.size());
//  ImmutableData data(NonEmptyString(RandomAlphaNumericString(RandomUint32() % 1000)));

//  vaults[vault_index].first.Put(NodeId(RandomString(NodeId::kSize)), data,
//      [](maidsafe_error error) {
//        ASSERT_EQ(error.code(), make_error_code(CommonErrors::success));
//      });

//  vaults[vault_index].first.Get<ImmutableData>(data.name(),
//      [](maidsafe_error error) {
//        ASSERT_EQ(error.code(), make_error_code(CommonErrors::success));
//      });
//}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
