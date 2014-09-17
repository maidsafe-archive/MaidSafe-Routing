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

#include "maidsafe/routing/bootstrap_file_operations.h"

#include <cstdint>
#include <string>

#include "maidsafe/common/log.h"
#include "maidsafe/common/sqlite3_wrapper.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/utils.h"

// NOTE: Temporarily defining COMPANY_NAME & APPLICATION_NAME if they are not defined.
// This is to allow setting up of BootstrapFilePath for routing only.

#ifndef COMPANY_NAME
# define COMPANY_NAME MaidSafe
# define COMPANY_NAME_DEFINED_TEMPORARILY
#endif
#ifndef APPLICATION_NAME
# define APPLICATION_NAME Routing
# define APPLICATION_NAME_DEFINED_TEMPORARILY
#endif

#include "maidsafe/common/application_support_directories.h"

#ifdef COMPANY_NAME_DEFINED_TEMPORARILY
# undef COMPANY_NAME
# undef COMPANY_NAME_DEFINED_TEMPORARILY
#endif
#ifdef APPLICATION_NAME_DEFINED_TEMPORARILY
# undef APPLICATION_NAME
# undef APPLICATION_NAME_DEFINED_TEMPORARILY
#endif


namespace fs = boost::filesystem;

namespace maidsafe {

namespace routing {

namespace {
typedef boost::asio::ip::udp::endpoint Endpoint;

boost::asio::ip::udp::endpoint GetEndpoint(const std::string& endpoint) {
  size_t delim = endpoint.rfind(':');
  boost::asio::ip::udp::endpoint ep;
  ep.port(boost::lexical_cast<uint16_t>(endpoint.substr(delim + 1)));
  ep.address(boost::asio::ip::address::from_string(endpoint.substr(0, delim)));
  return ep;
}

void InsertBootstrapContacts(sqlite::Database& database,
                             const BootstrapContacts& bootstrap_contacts) {
  std::string query("INSERT OR REPLACE INTO BOOTSTRAP_CONTACTS (ENDPOINT) VALUES (?)");
  sqlite::Statement statement{database, query};
  for (const auto& bootstrap_contact : bootstrap_contacts) {
    std::string endpoint_string = boost::lexical_cast<std::string>(bootstrap_contact);
    statement.BindText(1, endpoint_string);
    statement.Step();
    statement.Reset();
  }
}

void PrepareBootstrapTable(sqlite::Database& database) {
  std::string query(
      "CREATE TABLE IF NOT EXISTS BOOTSTRAP_CONTACTS("
      "ENDPOINT TEXT  PRIMARY KEY NOT NULL);");
  sqlite::Statement statement{database, query};
  statement.Step();
  statement.Reset();
}

}  // unnamed namespace

namespace detail {

boost::filesystem::path DoGetBootstrapFilePath(bool is_client,
                                               const boost::filesystem::path& file_name) {
  const boost::filesystem::path kLocalFilePath(ThisExecutableDir() / file_name);
  if (is_client && !boost::filesystem::exists(kLocalFilePath) &&
      (boost::filesystem::exists(GetUserAppDir() / file_name))) {  // Clients only
    return GetUserAppDir() / file_name;
  }
  return kLocalFilePath;
}

}  // namespace detail

void WriteBootstrapContacts(const BootstrapContacts& bootstrap_contacts,
                            const fs::path& bootstrap_file_path) {
  // carry out re-attempt for 10 times
  for (int i(0); i != 10; ++i) {
    try {
      sqlite::Database database(bootstrap_file_path, sqlite::Mode::kReadWriteCreate);
      sqlite::Tranasction transaction(database);
      PrepareBootstrapTable(database);
      InsertBootstrapContacts(database, bootstrap_contacts);
      transaction.Commit();
      return;
    } catch (const std::exception& e) {
      LOG(kError) << "error in attempt " << i << " write to " << bootstrap_file_path.string()
                  << " : " << boost::diagnostic_information(e);
      std::this_thread::sleep_for(std::chrono::milliseconds(RandomUint32() % 250 + 10));
    }
  }
  LOG(kError) << "Failed to write bootstrap contract after 10 attempts";
  BOOST_THROW_EXCEPTION(MakeError(CommonErrors::unable_to_handle_request));
}

// TODO(Team) : Consider timestamp in forming the list. If offline for more than a week, then
// list new nodes first
BootstrapContacts ReadBootstrapContacts(const fs::path& bootstrap_file_path) {
  sqlite::Database database(bootstrap_file_path, sqlite::Mode::kReadOnly);
  std::string query("SELECT * from BOOTSTRAP_CONTACTS");
  BootstrapContacts bootstrap_contacts;
  sqlite::Statement statement{database, query};
  for (;;) {
    if (statement.Step() == sqlite::StepResult::kSqliteRow) {
      std::string endpoint_string = statement.ColumnText(0);
      bootstrap_contacts.push_back(GetEndpoint(endpoint_string));
    } else {
      break;
    }
  }
  return bootstrap_contacts;
}

void InsertOrUpdateBootstrapContact(const BootstrapContact& bootstrap_contact,
                                    const boost::filesystem::path& bootstrap_file_path) {
  // carry out re-attempt for 10 times
  for (int i(0); i != 10; ++i) {
    try {
      sqlite::Database database(bootstrap_file_path, sqlite::Mode::kReadWriteCreate);
      sqlite::Tranasction transaction(database);
      PrepareBootstrapTable(database);
      InsertBootstrapContacts(database, BootstrapContacts(1, bootstrap_contact));
      transaction.Commit();
      return;
    } catch (const std::exception& e) {
      LOG(kError) << "error in attempt " << i << " insert into " << bootstrap_file_path.string()
                  << " : " << boost::diagnostic_information(e);
      std::this_thread::sleep_for(std::chrono::milliseconds(RandomUint32() % 250 + 10));
    }
  }
  LOG(kError) << "Failed to insert or update bootstrap contract after 10 attempts";
  BOOST_THROW_EXCEPTION(MakeError(CommonErrors::unable_to_handle_request));
}

}  // namespace routing

}  // namespace maidsafe
