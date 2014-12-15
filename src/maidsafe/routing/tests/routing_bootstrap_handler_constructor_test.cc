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

#include "maidsafe/routing/bootstrap_handler.h"

#include <vector>
#include <tuple>
#include <chrono>
#include "boost/asio/ip/udp.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/filesystem/operations.hpp"

#include "maidsafe/routing/types.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/sqlite3_wrapper.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/test.h"


namespace maidsafe {

namespace routing {

namespace test {
namespace fs = boost::filesystem;
TEST(BootstrapHandlerUnitTest, BEH_CreateBoostrapDataBase) {
  maidsafe::test::TestPath test_path(maidsafe::test::CreateTestPath("MaidSafe_TestBootstrap"));
  fs::path bootstrap_file_path(*test_path / "bootstrap");
  EXPECT_NO_THROW(BootstrapHandler tmp(bootstrap_file_path));
  std::string file_content(RandomString(3000 + RandomUint32() % 1000));
  ASSERT_TRUE(fs::exists(bootstrap_file_path));
  EXPECT_TRUE(WriteFile(bootstrap_file_path, file_content));
  EXPECT_TRUE(fs::exists(bootstrap_file_path));
  EXPECT_THROW(BootstrapHandler tmp(bootstrap_file_path), std::exception);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
