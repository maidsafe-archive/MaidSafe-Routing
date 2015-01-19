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

#include <cstdint>

#include "maidsafe/common/utils.h"
#include "maidsafe/common/serialisation/serialisation.h"

#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

#if !defined(_MSC_VER) || _MSC_VER != 1800
const int BootstrapHandler::MaxListSize;
#endif
const std::chrono::steady_clock::duration BootstrapHandler::UpdateDuration = std::chrono::hours(4);

BootstrapHandler::BootstrapHandler(boost::filesystem::path bootstrap_filename)
    : bootstrap_filename_(std::move(bootstrap_filename)),
      database_(bootstrap_filename_, sqlite::Mode::kReadWriteCreate),
      bootstrap_contacts_(),
      last_updated_(std::chrono::steady_clock::now()) {
  sqlite::Statement statement{database_,
                              "CREATE TABLE IF NOT EXISTS BOOTSTRAP_CONTACTS(NODEID BLOB PRIMARY "
                              "KEY NOT NULL, PUBLIC_KEY BLOB, ENDPOINT BLOB)"};
  statement.Step();
}

void BootstrapHandler::AddBootstrapContacts(BootstrapContacts bootstrap_contacts) {
  sqlite::Transaction transaction(database_);
  InsertBootstrapContacts(bootstrap_contacts);
  transaction.Commit();
  if (std::chrono::steady_clock::now() + UpdateDuration > last_updated_)
    CheckBootstrapContacts();  // put on active object
}

std::vector<BootstrapHandler::BootstrapContact> BootstrapHandler::ReadBootstrapContacts() {
  BootstrapContacts bootstrap_contacts;
  sqlite::Statement statement{database_,
                              "SELECT NODEID, PUBLIC_KEY, ENDPOINT FROM BOOTSTRAP_CONTACTS"};
  while (statement.Step() == sqlite::StepResult::kSqliteRow) {
    bootstrap_contacts.push_back(BootstrapContact {
                                     Parse<NodeId>(statement.ColumnBlob(0)),
                                     Parse<Endpoint>(statement.ColumnBlob(2)),
                                     Parse<asymm::PublicKey>(statement.ColumnBlob(1))});
  }
  return bootstrap_contacts;
}

void BootstrapHandler::ReplaceBootstrapContacts(BootstrapContacts bootstrap_contacts) {
  if (bootstrap_contacts.size() > MaxListSize)
    bootstrap_contacts.resize(MaxListSize);
  sqlite::Transaction transaction(database_);
  RemoveBootstrapContacts();
  InsertBootstrapContacts(std::move(bootstrap_contacts));
  transaction.Commit();
}

void BootstrapHandler::InsertBootstrapContacts(BootstrapContacts bootstrap_contacts) {
  if (bootstrap_contacts.empty())
    return;
  std::string query = {
      "INSERT OR REPLACE INTO BOOTSTRAP_CONTACTS(NODEID, PUBLIC_KEY, ENDPOINT) VALUES(?, ?, ?)"};
  for (std::size_t i = 1; i != bootstrap_contacts.size(); ++i)
    query += ",(?, ?, ?)";
  sqlite::Statement statement{database_, query};

  int index = 1;
  for (const auto& bootstrap_contact : bootstrap_contacts) {
    statement.BindBlob(index++, Serialise(bootstrap_contact.id));
    statement.BindBlob(index++, Serialise(bootstrap_contact.public_key));
    statement.BindBlob(index++, Serialise(bootstrap_contact.endpoint_pair.external));
    assert(bootstrap_contact.endpoint_pair.external ==
           bootstrap_contact.endpoint_pair.local);
  }

  statement.Step();
  statement.Reset();
}

void BootstrapHandler::RemoveBootstrapContacts() {
  sqlite::Statement statement{database_, "DELETE FROM BOOTSTRAP_CONTACTS"};
  statement.Step();
  statement.Reset();
}

void BootstrapHandler::CheckBootstrapContacts() {}

}  // namespace routing

}  // namespace maidsafe
