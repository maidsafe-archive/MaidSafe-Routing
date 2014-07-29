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

#ifndef MAIDSAFE_ROUTING_BOOTSTRAP_FILE_OPERATIONS_H_
#define MAIDSAFE_ROUTING_BOOTSTRAP_FILE_OPERATIONS_H_

#include <string>
#include <vector>

#include "boost/asio/ip/udp.hpp"
#include "boost/filesystem/path.hpp"

namespace maidsafe {

namespace routing {

namespace detail {

boost::filesystem::path DoGetBootstrapFilePath(bool is_client,
                                               const boost::filesystem::path& file_name);

template <bool is_client>
boost::filesystem::path GetOverrideBootstrapFilePath() {
  static const boost::filesystem::path override_bootstrap_file_path
      = DoGetBootstrapFilePath(is_client, "bootstrap_override.dat");
  return override_bootstrap_file_path;
}

template <bool is_client>
boost::filesystem::path GetDefaultBootstrapFilePath() {
  static const boost::filesystem::path default_bootstrap_file_path
      = DoGetBootstrapFilePath(is_client, "bootstrap.dat");
  return default_bootstrap_file_path;
}

}  // namespace detail

// FIXME(Team) BEFORE_RELEASE add public key and timestamp to BootstrapContact
// Note : asymm::PublicKey should be used here (not PublicPmid)
typedef boost::asio::ip::udp::endpoint BootstrapContact;

typedef std::vector<BootstrapContact> BootstrapContacts;

void WriteBootstrapContacts(const BootstrapContacts& bootstrap_contacts,
                            const boost::filesystem::path& bootstrap_file_path);

BootstrapContacts ReadBootstrapContacts(const boost::filesystem::path& bootstrap_file_path);

void InsertOrUpdateBootstrapContact(const BootstrapContact& bootstrap_contact,
                                    const boost::filesystem::path& bootstrap_file_path);
}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_BOOTSTRAP_FILE_OPERATIONS_H_
