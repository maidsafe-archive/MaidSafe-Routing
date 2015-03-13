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


#include <chrono>
#include <thread>
#include <tuple>
#include <vector>

#include "asio/ip/udp.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/path.hpp"

#include "maidsafe/common/identity.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/sqlite3_wrapper.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/bootstrap_handler.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/tests/utils/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST(BootstrapHandlerUnitTest, BEH_RefreshDatabase) {
  const size_t size(10);
  ScopedBootstrapFile bootstrap_file(boost::filesystem::initial_path() / "bootstrap.cache");
  maidsafe::test::TestPath test_path(maidsafe::test::CreateTestPath("MaidSafe_TestBootstrap"));
  BootstrapHandler test_handler;
  std::vector<BootstrapHandler::BootstrapContact> write_first(CreateBootstrapContacts(size));
  std::vector<BootstrapHandler::BootstrapContact> write_second(CreateBootstrapContacts(size));
  EXPECT_EQ(write_first.size(), size);
  EXPECT_EQ(write_second.size(), size);
  EXPECT_NO_THROW(test_handler.AddBootstrapContacts(write_first));

  auto read_from(test_handler.ReadBootstrapContacts());

  ASSERT_EQ(read_from.size(), write_first.size());
  for (size_t i(0); i < size; ++i) {
    EXPECT_EQ(read_from[i].id, write_first[i].id);
    EXPECT_EQ(read_from[i].endpoint_pair, write_first[i].endpoint_pair);
    EXPECT_TRUE(rsa::MatchingKeys(read_from[i].public_key, write_first[i].public_key));
  }
  test_handler.ReplaceBootstrapContacts(write_second);
  auto read_from_second(test_handler.ReadBootstrapContacts());
  EXPECT_EQ(read_from_second.size(), write_second.size());
  for (size_t i(0); i < size; ++i) {
    EXPECT_EQ(read_from_second[i].id, write_second[i].id);
    EXPECT_EQ(read_from_second[i].endpoint_pair, write_second[i].endpoint_pair);
    EXPECT_TRUE(rsa::MatchingKeys(read_from_second[i].public_key, write_second[i].public_key));
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
