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

/*
The purpose of this simple object is to maintain a list of bootstrap nodes. These are nodes that are
accessible through their published endpoint (external). Rudp confirms these nodes and passes us the
NodeId:PublicKey:Endpoint. This is maintained as a sqlite3 db for the time being to  multi-
process access (particularly useful for vaults).

This object in itself will very possibly end up in rudp itself.
*/

#ifndef MAIDSAFE_ROUTING_BOOTSTRAP_HANDLER_H_
#define MAIDSAFE_ROUTING_BOOTSTRAP_HANDLER_H_

#include <chrono>
#include <string>
#include <tuple>
#include <vector>

#include "asio/ip/udp.hpp"
#include "boost/filesystem/path.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/sqlite3_wrapper.h"

#include "maidsafe/routing/types.h"

namespace maidsafe {

namespace routing {

class BootstrapHandler {
 public:
  using Endpoint = asio::ip::udp::endpoint;
  using BootstrapContact = std::tuple<NodeId, asymm::PublicKey, Endpoint>;
  using BootstrapContacts = std::vector<BootstrapContact>;

  static const int MaxListSize = 1500;
  explicit BootstrapHandler(boost::filesystem::path bootstrap_filename);
  BootstrapHandler(const BootstrapHandler&) = delete;
  BootstrapHandler(BootstrapHandler&&) = delete;
  ~BootstrapHandler() = default;
  BootstrapHandler& operator=(const BootstrapHandler&) = delete;
  BootstrapHandler& operator=(BootstrapHandler&&) = delete;

  void AddBootstrapContact(const BootstrapContact& bootstrap_contact);
  BootstrapContacts ReadBootstrapContacts() const;
  void ReplaceBootstrapContacts(const BootstrapContacts& bootstrap_contacts);

 private:
  // Insert many contacts at once
  void InsertBootstrapContacts(const BootstrapContacts& bootstrap_contacts);
  // if we are asked to remove a contact for some reason
  void RemoveBoostrapContacts();
  // this should be put on an active object
  // we get all contacts and ping them (rudp_.ping) and when we have
  // MaxListSize or exhaunsted the list we replace the current list with the
  void CheckBootstrapContacts();

  boost::filesystem::path bootstrap_filename_;
  sqlite::Database database_;
  BootstrapContacts bootstrap_contacts_;
  std::chrono::steady_clock::time_point last_updated_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_BOOTSTRAP_HANDLER_H_
