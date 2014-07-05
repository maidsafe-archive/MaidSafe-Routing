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

void call_sqlite3_exec(sqlite3 *database, std::string& query, sqlite3_callback callback) {
  char *error_message = 0;
  if (sqlite3_exec(database, query.c_str(), callback, 0, &error_message) != SQLITE_OK) {
    LOG(kError) << "SQL error : " << error_message;
    sqlite3_free(error_message);
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::filesystem_io_error));  //FIXME Change to db error
  }
}

sqlite3 * call_sqlite3_open_v2(const boost::filesystem::path& filename, int flags) {
  sqlite3 *database;
  if (sqlite3_open_v2(filename.string().c_str(), &database, flags, NULL) != SQLITE_OK) {
    LOG(kError) << "Could not open db at : " << filename
                << ". Error : " << sqlite3_errmsg(database);
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::filesystem_io_error));  //FIXME Change to db error
  }
  return database;
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

// Throw if file exists
void WriteBootstrapContacts(const BootstrapContacts& bootstrap_contacts,
                            const fs::path& bootstrap_file_path) {
  sqlite3 *database = call_sqlite3_open_v2(bootstrap_file_path,
                                           SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
  std::string query = "CREATE TABLE BOOTSTRAP_CONTACTS("  \
                    "TIME_STAMP INT PRIMARY KEY     NOT NULL,"
                    "BOOTSTRAP_CONTACT     TEXT     NOT NULL);";
  LOG(kInfo) << "query : " << query;
  call_sqlite3_exec(database, query, NULL);
  int timestamp(0);
  query.clear();
  for (const auto& bootstrap_contact : bootstrap_contacts) {
  query += "INSERT INTO BOOTSTRAP_CONTACTS (TIME_STAMP,BOOTSTRAP_CONTACT) "  \
           "VALUES (" + std::to_string(timestamp) + ", '"
                      + SerialiseBootstrapContact(bootstrap_contact) +"');";
    ++timestamp;
  }
  call_sqlite3_exec(database, query, NULL);
}

// Throw if file doesn't exist
BootstrapContacts ReadBootstrapContacts(const fs::path& bootstrap_file_path) {
  sqlite3 *database = call_sqlite3_open_v2(bootstrap_file_path, SQLITE_OPEN_READONLY);
  std::string query = "SELECT * from BOOTSTRAP_CONTACTS";
  BootstrapContacts bootstrap_contacts;
  sqlite3_stmt *statement;
  if(sqlite3_prepare_v2(database, query.c_str(), -1, &statement, 0) != SQLITE_OK) {
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::filesystem_io_error));  // FIXME
  }
  int cols = sqlite3_column_count(statement);
  std::cout << "cols" << cols << std::endl;
  int result = 0;
  while(true) {
    result = sqlite3_step(statement);
    if(result == SQLITE_ROW) {
      std::string serialised_bootstrap_contact((char*)sqlite3_column_text(statement, 1));
      bootstrap_contacts.push_back(ParseBootstrapContact(serialised_bootstrap_contact));
    } else {
      break;
    }
  }
  return bootstrap_contacts;
}

// Creates if file doesn't exist
//void UpdateBootstrapFile(const BootstrapContact& /*bootstrap_contact*/,
//                         const boost::filesystem::path& /*bootstrap_file_path*/,
//                         bool /*remove*/) {

//}


}  // namespace routing

}  // namespace maidsafe
