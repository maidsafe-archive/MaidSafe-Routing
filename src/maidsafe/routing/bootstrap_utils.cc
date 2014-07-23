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

#include "maidsafe/routing/bootstrap_utils.h"

#include <string>

#include "maidsafe/common/utils.h"

namespace maidsafe {

namespace routing {

namespace fs = boost::filesystem;

namespace {

using boost::asio::ip::udp;
using boost::asio::ip::address;

BootstrapContacts GetHardCodedBootstrapContacts() {
  return BootstrapContacts{
      udp::endpoint{ address::from_string("104.131.253.66"), kLivePort },
      udp::endpoint{ address::from_string("95.85.32.100"), kLivePort },
      udp::endpoint{ address::from_string("128.199.159.50"), kLivePort },
      udp::endpoint{ address::from_string("178.79.156.73"), kLivePort },
      udp::endpoint{ address::from_string("106.185.24.221"), kLivePort },
      udp::endpoint{ address::from_string("23.239.27.245"), kLivePort }
  };
}

}  // unnamed namespace

BootstrapContacts GetBootstrapContacts(bool is_client) {
  const fs::path kCurrentBootstrapFilePath{
    is_client ? detail::GetCurrentBootstrapFilePath<true>()
              : detail::GetCurrentBootstrapFilePath<false>() };
  auto bootstrap_contacts(ReadBootstrapContacts(kCurrentBootstrapFilePath));

  if (kCurrentBootstrapFilePath == (is_client ? detail::GetDefaultBootstrapFilePath<true>() :
                                                detail::GetDefaultBootstrapFilePath<false>())) {
    auto hard_coded_bootstrap_contacts = GetHardCodedBootstrapContacts();
    bootstrap_contacts.insert(bootstrap_contacts.end(), hard_coded_bootstrap_contacts.begin(),
                              hard_coded_bootstrap_contacts.end());
  }
  return bootstrap_contacts;
}

void InsertOrUpdateBootstrapContact(const BootstrapContact& bootstrap_contact, bool is_client) {
  InsertOrUpdateBootstrapContact(bootstrap_contact,
                                 is_client ? detail::GetCurrentBootstrapFilePath<true>()
                                           : detail::GetCurrentBootstrapFilePath<false>());
}

BootstrapContacts GetZeroStateBootstrapContacts(udp::endpoint local_endpoint) {
  BootstrapContacts bootstrap_contacts { GetBootstrapContacts(false) };
  bootstrap_contacts.erase(std::remove(std::begin(bootstrap_contacts), std::end(bootstrap_contacts),
                                       local_endpoint),
                           std::end(bootstrap_contacts));
  return bootstrap_contacts;
}

}  // namespace routing

}  // namespace maidsafe
