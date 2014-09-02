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

TEST(BootstrapFileOperationsTest, FUNC_FileExists) {
  maidsafe::test::TestPath test_path(maidsafe::test::CreateTestPath("MaidSafe_TestUtils"));
  fs::path bootstrap_file_path(*test_path / "bootstrap");
  std::string file_content(RandomString(3000 + RandomUint32() % 1000));
  ASSERT_FALSE(fs::exists(bootstrap_file_path));
  EXPECT_TRUE(WriteFile(bootstrap_file_path, file_content));
  EXPECT_TRUE(fs::exists(bootstrap_file_path));
  EXPECT_THROW(ReadBootstrapContacts(bootstrap_file_path), std::exception) << "read from bad file";
  EXPECT_TRUE(fs::exists(bootstrap_file_path)) << bootstrap_file_path.string() << "should exist";
  BootstrapContacts bootstrap_contacts;
  for (int i(0); i < 100; ++i)
    bootstrap_contacts.push_back(
        BootstrapContact(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()));

  EXPECT_THROW(WriteBootstrapContacts(bootstrap_contacts, bootstrap_file_path), std::exception)
      << "file exists, should throw";
  EXPECT_THROW(ReadBootstrapContacts(bootstrap_file_path), std::exception) << "read bad file";
}

TEST(BootstrapFileOperationsTest, BEH_ReadWrite) {
  maidsafe::test::TestPath test_path(maidsafe::test::CreateTestPath("MaidSafe_TestUtils"));
  fs::path bootstrap_file_path(*test_path / "bootstrap");
  ASSERT_FALSE(fs::exists(bootstrap_file_path));
  EXPECT_THROW(ReadBootstrapContacts(bootstrap_file_path), std::exception);
  EXPECT_FALSE(fs::exists(bootstrap_file_path));
  BootstrapContacts bootstrap_contacts;
  for (int i(0); i < 1000; ++i)
    bootstrap_contacts.push_back(
        BootstrapContact(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()));

  EXPECT_NO_THROW(WriteBootstrapContacts(bootstrap_contacts, bootstrap_file_path));
  auto bootstrap_contacts_result = ReadBootstrapContacts(bootstrap_file_path);
  EXPECT_EQ(bootstrap_contacts_result, bootstrap_contacts) << bootstrap_contacts_result.size()
                                                           << " vs " << bootstrap_contacts.size();
}

TEST(BootstrapFileOperationsTest, FUNC_Parallel_Unique_Update) {
  maidsafe::test::TestPath test_path(maidsafe::test::CreateTestPath("MaidSafe_TestUtils"));
  fs::path bootstrap_file_path(*test_path / "bootstrap");
  ASSERT_FALSE(fs::exists(bootstrap_file_path));
  EXPECT_THROW(ReadBootstrapContacts(bootstrap_file_path), std::exception);
  EXPECT_FALSE(fs::exists(bootstrap_file_path));

  ::maidsafe::test::RunInParallel(20, [&] {
    BootstrapContacts bootstrap_contacts;
    for (int i(0); i < 20; ++i) {
      bootstrap_contacts.push_back(
          BootstrapContact(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()));

      EXPECT_NO_THROW(
          InsertOrUpdateBootstrapContact(bootstrap_contacts.back(), bootstrap_file_path));
    }
    auto bootstrap_contacts_result = ReadBootstrapContacts(bootstrap_file_path);
    for (const auto& i : bootstrap_contacts) {
      EXPECT_NE(bootstrap_contacts_result.end(),
                std::find(bootstrap_contacts_result.begin(), bootstrap_contacts_result.end(), i));
    }
  });
}

TEST(BootstrapFileOperationsTest, DISABLED_FUNC_Parallel_Duplicate_Update) {
  maidsafe::test::TestPath test_path(maidsafe::test::CreateTestPath("MaidSafe_TestUtils"));
  fs::path bootstrap_file_path(*test_path / "bootstrap");
  ASSERT_FALSE(fs::exists(bootstrap_file_path));
  EXPECT_THROW(ReadBootstrapContacts(bootstrap_file_path), std::exception);
  EXPECT_FALSE(fs::exists(bootstrap_file_path));
  // set up vector of all same contacts
  BootstrapContacts bootstrap_contacts;
  for (int i(0); i < 20; ++i) {
    bootstrap_contacts.push_back(
        BootstrapContact(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()));
  }

  ::maidsafe::test::RunInParallel(20, [&] {
    for (const auto& i : bootstrap_contacts) {
      EXPECT_NO_THROW(InsertOrUpdateBootstrapContact(i, bootstrap_file_path));
    }
    auto bootstrap_contacts_result = ReadBootstrapContacts(bootstrap_file_path);
    for (const auto& i : bootstrap_contacts) {
      EXPECT_NE(bootstrap_contacts_result.end(),
                std::find(bootstrap_contacts_result.begin(), bootstrap_contacts_result.end(), i));
    }
  });
}

}  // namespace test
}  // namespace routing
}  // namespace maidsafe
