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

TEST(BootstrapHandlerUnitTest, FUNC_AddContacts) {
  ScopedBootstrapFile bootstrap_file(boost::filesystem::initial_path() / "bootstrap.cache");
  BootstrapHandler test_handler;
  for (int i = 0; i < 100; ++i) {
    EXPECT_NO_THROW(test_handler.AddBootstrapContacts(CreateBootstrapContacts(10)));
  }
  std::vector<BootstrapHandler::BootstrapContact> contacts(CreateBootstrapContacts(1000));
  test_handler.AddBootstrapContacts(contacts);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
