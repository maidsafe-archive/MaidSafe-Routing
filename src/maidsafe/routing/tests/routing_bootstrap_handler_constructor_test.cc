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

namespace fs = boost::filesystem;

TEST(BootstrapHandlerUnitTest, BEH_CreateBoostrapDatabase) {
  ScopedBootstrapFile bootstrap_file(boost::filesystem::initial_path() / "bootstrap.cache");
  EXPECT_NO_THROW(BootstrapHandler tmp);
  SerialisedData file_content(RandomBytes(3000, 4000));
  auto bootstrap_file_path(GetBootstrapFilePath());
  ASSERT_TRUE(fs::exists(bootstrap_file_path));
  EXPECT_TRUE(WriteFile(bootstrap_file_path, file_content));
  EXPECT_TRUE(fs::exists(bootstrap_file_path));
  EXPECT_THROW(BootstrapHandler tmp, std::exception);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
