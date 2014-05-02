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

#include <vector>

#include "boost/filesystem/operations.hpp"

#include "maidsafe/common/log.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/bootstrap_file_operations.h"

namespace fs = boost::filesystem;

namespace maidsafe {
namespace routing {
namespace test {

TEST(BootstrapFileOperationsTest, BEH_ReadWriteUpdate) {
  maidsafe::test::TestPath test_path(maidsafe::test::CreateTestPath("MaidSafe_TestUtils"));
  fs::path bootstrap_file_path(*test_path / "bootstrap");
  ASSERT_FALSE(fs::exists(bootstrap_file_path));
  EXPECT_THROW(ReadBootstrapFile(bootstrap_file_path), std::exception);
  EXPECT_FALSE(fs::exists(bootstrap_file_path));
  BootstrapContacts bootstrap_contacts;
  EXPECT_NO_THROW(WriteBootstrapFile(bootstrap_contacts, bootstrap_file_path));
  // Write
  BootstrapContacts expected_bootstrap_contacts;
  for (int i(0); i < 100; ++i) {
    BootstrapContact bootstrap_contact(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
    bootstrap_contacts.push_back(bootstrap_contact);
    expected_bootstrap_contacts.insert(std::begin(expected_bootstrap_contacts), bootstrap_contact);
    EXPECT_NO_THROW(WriteBootstrapFile(bootstrap_contacts, bootstrap_file_path));
    auto actual_bootstrap_contacts = ReadBootstrapFile(bootstrap_file_path);
    EXPECT_TRUE(std::equal(actual_bootstrap_contacts.begin(),
                           actual_bootstrap_contacts.end(),
                           expected_bootstrap_contacts.begin()));
  }

  // Update add
  for (int i(0); i < 100; ++i) {
    BootstrapContact bootstrap_contact(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
    bootstrap_contacts.push_back(bootstrap_contact);
    expected_bootstrap_contacts.insert(std::begin(expected_bootstrap_contacts), bootstrap_contact);
    EXPECT_NO_THROW(UpdateBootstrapFile(bootstrap_contact, bootstrap_file_path, false));
    auto actual_bootstrap_contacts = ReadBootstrapFile(bootstrap_file_path);
    EXPECT_TRUE(std::equal(actual_bootstrap_contacts.begin(),
                           actual_bootstrap_contacts.end(),
                           expected_bootstrap_contacts.begin()));
  }

  // Update remove
  // TODO(Prakash)
}

}  // namespace test
}  // namespace routing
}  // namespace maidsafe
