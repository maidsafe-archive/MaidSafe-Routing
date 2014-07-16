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

#include <string>

#include "maidsafe/common/log.h"
#include "maidsafe/common/sqlite3_wrapper.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/utils.h"

namespace fs = boost::filesystem;

namespace maidsafe {

namespace routing {

namespace {
typedef boost::asio::ip::udp::endpoint Endpoint;

// Copied from Drive launcher
// TODO(Prakash) Move to utils of routing
boost::asio::ip::udp::endpoint GetEndpoint(const std::string& endpoint) {
  size_t delim = endpoint.rfind(':');
  boost::asio::ip::udp::endpoint ep;
  ep.port(boost::lexical_cast<unsigned int>(endpoint.substr(delim + 1)));
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
                         const boost::filesystem::path& bootstrap_file_path, bool remove) {
  if (bootstrap_contact.address().is_unspecified()) {
    LOG(kWarning) << "Invalid Endpoint" << bootstrap_contact;
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::invalid_parameter));
  }
  auto bootstrap_contacts(ParseBootstrapContacts(ReadFile(bootstrap_file_path).string()));
  auto itr(
      std::find(std::begin(bootstrap_contacts), std::end(bootstrap_contacts), bootstrap_contact));
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
    } else {
      LOG(kVerbose) << "Endpoint already in the list : " << bootstrap_contact;
    }
  }
}

// Throw if file exists ?
// FIXME over write table if it exists ?
void WriteBootstrapContacts(const BootstrapContacts& bootstrap_contacts,
                            const fs::path& bootstrap_file_path) {
  sqlite::Database database(bootstrap_file_path, sqlite::Mode::kReadWriteCreate);
  sqlite::Tranasction transaction(database);
  std::string query(
      "CREATE TABLE BOOTSTRAP_CONTACTS("
      "ENDPOINT TEXT  PRIMARY KEY  NOT NULL);");
  database.Execute(query);
  InsertBootstrapContacts(database, bootstrap_contacts);
  transaction.Commit();
}

// Throw if file doesn't exist
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
  sqlite::Database database(bootstrap_file_path, sqlite::Mode::kReadWriteCreate);
  sqlite::Tranasction transaction(database);
  std::string query(
      "CREATE TABLE IF NOT EXISTS BOOTSTRAP_CONTACTS("
      "ENDPOINT TEXT  PRIMARY KEY NOT NULL);");
  database.Execute(query);
  InsertBootstrapContacts(database, BootstrapContacts(1, bootstrap_contact));
  transaction.Commit();
}

void RemoveBootstrapContact(const Endpoint& /*endpoint*/,
                            const boost::filesystem::path& /*bootstrap_file_path*/) {}


}  // namespace routing

}  // namespace maidsafe
