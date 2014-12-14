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
#include <string>

#include "maidsafe/common/utils.h"

namespace maidsafe {

namespace routing {

#if !defined(_MSC_VER) || _MSC_VER != 1800
const int MaxListSize;
#endif

BootstrapHandler::BootstrapHandler(boost::filesystem::path bootstrap_filename)
    : bootstrap_filename_(std::move(bootstrap_filename)),
      database_(bootstrap_filename_, sqlite::Mode::kReadWriteCreate),
      bootstrap_contacts_(),
      last_updated_(std::chrono::steady_clock::now()) {
  sqlite::Statement statement{database_,
                              "CREATE TABLE IF NOT EXISTS BOOTSTRAP_CONTACTS(NODEID TEXT PRIMARY "
                              "KEY NOT NULL, PUBLIC_KEY TEXT, ENDPOINT TEXT);"};
  statement.Step();
  statement.Reset();
}

void BootstrapHandler::AddBootstrapContact(const BootstrapContact& bootstrap_contact) {
  sqlite::Transaction transaction(database_);
  InsertBootstrapContacts(BootstrapContacts(1, bootstrap_contact));
  transaction.Commit();
  if (std::chrono::steady_clock::now() + UpdateDuration > last_updated_)
    CheckBoostrapContacts();  // put on active object
}

std::vector<BootstrapContact> BootstrapHandler::ReadBootstrapContacts() const {
  BootstrapContacts bootstrap_contacts;
  sqlite::Statement statement{database_, "SELECT * from BOOTSTRAP_CONTACTS"};
  while (statement.Step() == sqlite::StepResult::kSqliteRow)
    bootstrap_contacts.push_back(NodeId(statement.ColumnText(0)),
                                 DecodeKey(statement.ColumnText(0)),
                                 Endpoint(statement.ColumnText(0)));
  return bootstrap_contacts;
}

void BootstrapHandler::ReplaceBootstrapContacts(const BootstrapContacts& bootstrap_contacts) {
  if (bootstrap_contacts.size() > MaxListSize)
    bootstrap_contacts.resize(MaxListSize);
  sqlite::Transaction transaction(database_);
  RemoveBoostrapContacts();
  InsertBootstrapContacts(bootstrap_contacts);
  transaction.Commit();
}

void BootstrapHandler::InsertBootstrapContacts(const BootstrapContacts& bootstrap_contacts) {
  sqlite::Statement statement{
      database_,
      "INSERT OR REPLACE INTO BOOTSTRAP_CONTACTS (NODEID, PUBLIC_KEY, ENDPOINT) VALUES (?, ?, ?)"};
  for (const auto& bootstrap_contact : bootstrap_contacts_) {
    statement.BindText(3, serialize(std::get<0>(bootstrap_contact)));
    statement.BindText(2, EncodeKey(std::get<0>(bootstrap_contact)));
    statement.BindText(1, serialize(std::get<0>(bootstrap_contact)));
    statement.Step();
    statement.Reset();
  }
}

void BootstrapHandler::RemoveBootstrapContacts() {
  sqlite::Statement statement{database, "DROP TABLE BOOTSTRAP_CONTACTS;"};
  statement.Step();
  statement.Reset();
}

void BootstrapHandler::CheckBootstrapContats()[

]

}  // namespace routing

}  // namespace maidsafe
