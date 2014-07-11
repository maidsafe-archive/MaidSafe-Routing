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

extern "C" {
#include <sqlite3.h>
}

#include <string>

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/utils.h"

namespace fs = boost::filesystem;

namespace maidsafe {

namespace routing {

namespace {
  typedef boost::asio::ip::udp::endpoint Endpoint;

// Copied from Drive launcher
// TODO Move to utils of routing
boost::asio::ip::udp::endpoint GetEndpoint(const std::string& endpoint) {
  size_t delim = endpoint.rfind(':');
  boost::asio::ip::udp::endpoint ep;
  ep.port(boost::lexical_cast<uint16_t>(endpoint.substr(delim + 1)));
  ep.address(boost::asio::ip::address::from_string(endpoint.substr(0, delim)));
  return ep;
}

// DB
struct Sqlite3Statement;

struct Sqlite3DB {
  Sqlite3DB(const boost::filesystem::path& filename, int flags);
  ~Sqlite3DB();

  void Execute(const std::string& query);
 friend class Sqlite3Statement;

 private:
  sqlite3 *database;
};

Sqlite3DB::Sqlite3DB(const boost::filesystem::path& filename, int flags)
    : database(nullptr) {
  if (sqlite3_open_v2(filename.string().c_str(), &database, flags, NULL) != SQLITE_OK) {
    LOG(kError) << "Could not open db at : " << filename
                << ". Error : " << sqlite3_errmsg(database);
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::db_error));
  }
  sqlite3_busy_timeout(database, 250);
}

Sqlite3DB::~Sqlite3DB() {
  int result = sqlite3_close(database);
  if (result != SQLITE_OK)
    LOG(kError) << "Failed to close DB. Error : " << result;
}


void Sqlite3DB::Execute(const std::string& query) {
  char *error_message = 0;
  int result = sqlite3_exec(database, query.c_str(), NULL, 0, &error_message);
  assert(result != SQLITE_ROW);

  if (result != SQLITE_OK) {
    if (result == SQLITE_BUSY) {
      LOG(kWarning) << "SQL busy : " << error_message << " . Query :" << query;
      sqlite3_free(error_message);
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::db_busy));
    } else {
      LOG(kError) << "SQL error : " << error_message  << ". return value : " << result
                  << " . Query :" << query;
      sqlite3_free(error_message);
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::db_error));
    }
  }
}

// Tranasction
struct Sqlite3Tranasction {
  Sqlite3Tranasction(Sqlite3DB& database_in);
  ~Sqlite3Tranasction();
  void Commit();

 private:
  Sqlite3Tranasction(const Sqlite3Tranasction&);
  Sqlite3Tranasction(Sqlite3Tranasction&&);
  Sqlite3Tranasction& operator=(Sqlite3Tranasction);

  const int kAttempts;
  bool committed;
  Sqlite3DB& database;
};


Sqlite3Tranasction::Sqlite3Tranasction(Sqlite3DB& database_in)
    : kAttempts(100),
      database(database_in) {
  std::string query("BEGIN EXCLUSIVE TRANSACTION");  // FIXME consider immediate transaction
  for (int i(0); i != kAttempts; ++i) {
    try {
      database.Execute(query);
      return;
    } catch (const maidsafe_error& error) {
      if (error.code() == make_error_code(CommonErrors::db_busy)) {
        LOG(kWarning) << "SQLITE_BUSY. Attempts : " << i;
        std::this_thread::sleep_for(std::chrono::milliseconds(((RandomUint32() % 250) + 10) * i));
      } else {
        LOG(kError) << "SQL error. Attempts " << i;
        throw;
      }
    }
  }
  LOG(kError) << "Failed to aquire db lock in " << kAttempts << " attempts";
  BOOST_THROW_EXCEPTION(MakeError(CommonErrors::unable_to_handle_request));
}

Sqlite3Tranasction::~Sqlite3Tranasction() {
  if (committed)
    return;
  try {
    database.Execute("ROLLBACK TRANSACTION");
  } catch (const std::exception& error) {
    LOG(kError) << "Error on ROLLBACK TRANSACTION" << error.what();
  }
}

void Sqlite3Tranasction::Commit() {
  database.Execute("COMMIT TRANSACTION");
  committed = true;
}


enum class StepResult {
  kSqliteRow = SQLITE_ROW,
  kSqliteDone = SQLITE_DONE
};

struct Sqlite3Statement {

  Sqlite3Statement(Sqlite3DB& database_in, const std::string& query);
  ~Sqlite3Statement();
  void BindText(int index, const std::string& text);
  StepResult Step();
  void Reset();

  std::string ColumnText(int col_index);

 private :
  Sqlite3DB& database;
  sqlite3_stmt* statement;
};

Sqlite3Statement::Sqlite3Statement(Sqlite3DB& database_in, const std::string& query)
    : database(database_in),
      statement() {
  auto return_value = sqlite3_prepare_v2(database.database, query.c_str(), query.size(),
                                         &statement, 0);
  if(return_value != SQLITE_OK) {
    LOG(kError) << " sqlite3_prepare_v2 returned : " << return_value;
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::db_error));
  }
}

Sqlite3Statement::~Sqlite3Statement() {
  auto return_value = sqlite3_finalize(statement);
  if (return_value != SQLITE_OK)
    LOG(kError) << " sqlite3_finalize returned : " << return_value;
}

void Sqlite3Statement::BindText(int row_index, const std::string& text) {
  auto return_value = sqlite3_bind_text(statement, row_index, text.c_str(),
                                        static_cast<int>(text.size()), 0);
  if(return_value != SQLITE_OK) {
    LOG(kError) << " sqlite3_bind_text returned : " << return_value;
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::db_error));
  }
}

StepResult Sqlite3Statement::Step() {
  auto return_value = sqlite3_step(statement);
  if ((return_value == SQLITE_DONE) || (return_value == SQLITE_ROW)) {
    return StepResult(return_value);
  } else {
    LOG(kError) << "SQL error with sqlite3_step : " << return_value;
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::db_error));
  }
}

std::string Sqlite3Statement::ColumnText(int col_index) {
  std::string column_text((char*)sqlite3_column_text(statement, col_index));
  return column_text;
}

void Sqlite3Statement::Reset() {
  auto return_value = sqlite3_reset(statement);
  if (return_value != SQLITE_OK) {
    LOG(kError) << "SQL error with sqlite3_reset : " << return_value;
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::db_error));
  }
}

void InsertBootstrapContacts(Sqlite3DB& database, const BootstrapContacts& bootstrap_contacts) {
  std::string query ("INSERT OR REPLACE INTO BOOTSTRAP_CONTACTS (ENDPOINT) VALUES (?)");
  Sqlite3Statement statement{ database, query };
  for (const auto& bootstrap_contact : bootstrap_contacts) {
    std::string endpoint_string = boost::lexical_cast<std::string>(bootstrap_contact);
    statement.BindText(1, endpoint_string);
    statement.Step();
    statement.Reset();
  }
}

}  // unnamed namespace

std::string SerialiseBootstrapContact(const BootstrapContact& bootstrap_contact) {
  protobuf::BootstrapContact protobuf_bootstrap_contact;
  SetProtobufEndpoint(bootstrap_contact, protobuf_bootstrap_contact.mutable_endpoint());
  std::string serialised_bootstrap_contact;
  if (!protobuf_bootstrap_contact.SerializeToString(&serialised_bootstrap_contact)) {
    LOG(kError) << "Failed to serialise bootstrap contact.";
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::serialisation_error));
  }
  return serialised_bootstrap_contact;
}

std::string SerialiseBootstrapContacts(const BootstrapContacts& bootstrap_contacts) {
  protobuf::BootstrapContacts protobuf_bootstrap_contacts;
  for (const auto& bootstrap_contact : bootstrap_contacts) {
    protobuf_bootstrap_contacts.add_serialised_bootstrap_contacts(
        SerialiseBootstrapContact(bootstrap_contact));
  }
  std::string serialised_bootstrap_contacts;
  if (!protobuf_bootstrap_contacts.SerializeToString(&serialised_bootstrap_contacts)) {
    LOG(kError) << "Failed to serialise bootstrap contacts.";
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::serialisation_error));
  }
  return serialised_bootstrap_contacts;
}

BootstrapContact ParseBootstrapContact(const std::string& serialised_bootstrap_contact) {
  protobuf::BootstrapContact protobuf_bootstrap_contact;
  if (!protobuf_bootstrap_contact.ParseFromString(serialised_bootstrap_contact)) {
    LOG(kError) << "Could not parse bootstrap contact.";
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
  }
  return GetEndpointFromProtobuf(protobuf_bootstrap_contact.endpoint());
}

BootstrapContacts ParseBootstrapContacts(const std::string& serialised_bootstrap_contacts) {
  protobuf::BootstrapContacts protobuf_bootstrap_contacts;
  if (!protobuf_bootstrap_contacts.ParseFromString(serialised_bootstrap_contacts)) {
    LOG(kError) << "Could not parse bootstrap file.";
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
  }
  BootstrapContacts bootstrap_contacts;
  bootstrap_contacts.reserve(protobuf_bootstrap_contacts.serialised_bootstrap_contacts().size());
  for (const auto& serialised_bootstrap_contact :
           protobuf_bootstrap_contacts.serialised_bootstrap_contacts()) {
    bootstrap_contacts.push_back(ParseBootstrapContact(serialised_bootstrap_contact));
  }
  return bootstrap_contacts;
}


// TODO(Team) : Consider timestamp in forming the list. If offline for more than a week, then
// list new nodes first
BootstrapContacts ReadBootstrapFile(const fs::path& bootstrap_file_path) {
  auto bootstrap_contacts(ParseBootstrapContacts(ReadFile(bootstrap_file_path).string()));

  std::reverse(std::begin(bootstrap_contacts), std::end(bootstrap_contacts));
  return bootstrap_contacts;
}

void WriteBootstrapFile(const BootstrapContacts& bootstrap_contacts,
                        const fs::path& bootstrap_file_path) {
  // TODO(Prakash) consider overloading WriteFile() to take NonEmptyString as parameter
  if (!WriteFile(bootstrap_file_path, SerialiseBootstrapContacts(bootstrap_contacts))) {
    LOG(kError) << "Could not write bootstrap file at : " << bootstrap_file_path;
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::filesystem_io_error));
  }
}

void UpdateBootstrapFile(const BootstrapContact& bootstrap_contact,
                         const boost::filesystem::path& bootstrap_file_path,
                         bool remove) {
  if (bootstrap_contact.address().is_unspecified()) {
    LOG(kWarning) << "Invalid Endpoint" << bootstrap_contact;
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::invalid_parameter));
  }
  auto bootstrap_contacts(ParseBootstrapContacts(ReadFile(bootstrap_file_path).string()));
  auto itr(std::find(std::begin(bootstrap_contacts), std::end(bootstrap_contacts),
                     bootstrap_contact));
  if (remove) {
    if (itr != std::end(bootstrap_contacts)) {
      bootstrap_contacts.erase(itr);
      WriteBootstrapFile(bootstrap_contacts, bootstrap_file_path);
    } else {
      LOG(kVerbose) << "Can't find endpoint to remove : " << bootstrap_contact;
    }
  } else {
    if (itr == std::end(bootstrap_contacts)) {
      bootstrap_contacts.push_back(bootstrap_contact);
      WriteBootstrapFile(bootstrap_contacts, bootstrap_file_path);
    }  else {
      LOG(kVerbose) << "Endpoint already in the list : " << bootstrap_contact;
    }
  }
}

// Throw if file exists ?
// FIXME over write table if it exists ?
void WriteBootstrapContacts(const BootstrapContacts& bootstrap_contacts,
                            const fs::path& bootstrap_file_path) {
  Sqlite3DB database(bootstrap_file_path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
  Sqlite3Tranasction transaction(database);
  std::string query("CREATE TABLE BOOTSTRAP_CONTACTS(""ENDPOINT TEXT  PRIMARY KEY  NOT NULL);");
  database.Execute(query);
  InsertBootstrapContacts(database, bootstrap_contacts);
  transaction.Commit();
}

// Throw if file doesn't exist
BootstrapContacts ReadBootstrapContacts(const fs::path& bootstrap_file_path) {
  Sqlite3DB database(bootstrap_file_path, SQLITE_OPEN_READONLY);
  std::string query("SELECT * from BOOTSTRAP_CONTACTS");
  BootstrapContacts bootstrap_contacts;
  Sqlite3Statement statement{ database, query };
  for (;;) {
    if(statement.Step() == StepResult::kSqliteRow) {
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
  Sqlite3DB database(bootstrap_file_path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
  Sqlite3Tranasction transaction(database);
  std::string query("CREATE TABLE IF NOT EXISTS BOOTSTRAP_CONTACTS(""ENDPOINT TEXT  PRIMARY KEY NOT NULL);");
  database.Execute(query);
  InsertBootstrapContacts(database, BootstrapContacts(1, bootstrap_contact));
  transaction.Commit();
}

void RemoveBootstrapContact(const Endpoint& /*endpoint*/,
                            const boost::filesystem::path& /*bootstrap_file_path*/) {}


}  // namespace routing

}  // namespace maidsafe
