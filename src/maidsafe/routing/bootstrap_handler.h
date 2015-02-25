/*  Copyright 2015 MaidSafe.net limited

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
NodeId:PublicKey:Endpoint. This is maintained as a sqlite3 db for the time being to multi-
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
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/preprocessor/stringize.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/sqlite3_wrapper.h"

#include "maidsafe/routing/types.h"
#include "maidsafe/routing/contact.h"

namespace maidsafe {

namespace routing {

inline boost::filesystem::path GetBootstrapFilePath() {
  static const std::string file_name("bootstrap.cache");
  boost::filesystem::path file_path(boost::filesystem::initial_path() / file_name);
  if (boost::filesystem::exists(file_path))
    return file_path;
#if defined(BOOTSTRAP_FILE_PATH)
  file_path = BOOST_PP_STRINGIZE(BOOTSTRAP_FILE_PATH);
#elif defined(MAIDSAFE_WIN32)
  file_path = boost::filesystem::path(std::getenv("ALLUSERSPROFILE")) / "MaidSafe" / file_name;
#elif defined(MAIDSAFE_APPLE)
  file_path = boost::filesystem::path("/Library/Application Support/") / "MaidSafe" / file_name;
#elif defined(MAIDSAFE_LINUX)
  file_path = boost::filesystem::path("/opt/") / "maidsafe" / "sbin" / file_name;
#else
  LOG(kError) << "Cannot deduce system wide application directory path";
  BOOST_THROW_EXCEPTION(MakeError(CommonErrors::invalid_parameter));
#endif
  return file_path;
}

class BootstrapHandler {
 public:
  using BootstrapContact = Contact;
  using BootstrapContacts = std::vector<BootstrapContact>;

  static const int MaxListSize = 1500;
  static const std::chrono::steady_clock::duration UpdateDuration;

  BootstrapHandler();
  BootstrapHandler(const BootstrapHandler&) = delete;
  BootstrapHandler(BootstrapHandler&&) = delete;
  ~BootstrapHandler() = default;
  BootstrapHandler& operator=(const BootstrapHandler&) = delete;
  BootstrapHandler& operator=(BootstrapHandler&&) = delete;

  void AddBootstrapContacts(BootstrapContacts bootstrap_contacts);
  BootstrapContacts ReadBootstrapContacts();
  void ReplaceBootstrapContacts(BootstrapContacts bootstrap_contacts);
  bool OutOfDate() const {
    return (std::chrono::steady_clock::now() + UpdateDuration > last_updated_);
  }
  void ResetTimer() { last_updated_ = std::chrono::steady_clock::now(); }

 private:
  // Insert many contacts at once
  void InsertBootstrapContacts(BootstrapContacts bootstrap_contacts);
  // if we are asked to remove a contact for some reason
  void RemoveBootstrapContacts();
  // this should be put on an active object
  // we get all contacts and ping them (rudp_.ping) and when we have
  // MaxListSize or exhausted the list we replace the current list with the
  void CheckBootstrapContacts();

  sqlite::Database database_;
  std::chrono::steady_clock::time_point last_updated_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_BOOTSTRAP_HANDLER_H_
