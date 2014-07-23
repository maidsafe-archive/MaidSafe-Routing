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

#include "maidsafe/routing/tests/zero_state_helpers.h"

#include "boost/filesystem/operations.hpp"

#include "maidsafe/common/config.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/bootstrap_file_operations.h"

namespace fs = boost::filesystem;

namespace maidsafe {

namespace routing {

namespace test {

fs::path LocalNetworkBootstrapFile() {
  return fs::path(ThisExecutableDir() / "local_network_bootstrap.dat");
}

void WriteZeroStateBootstrapFile(const boost::asio::ip::udp::endpoint& contact0,
                                 const boost::asio::ip::udp::endpoint& contact1) {
  fs::path bootstrap_file{ GetOverrideBootstrapFilePath(false) };
  fs::remove(bootstrap_file);
  WriteBootstrapContacts(BootstrapContacts{ contact0, contact1 }, bootstrap_file);
}

void WriteLocalNetworkBootstrapFile() {
  fs::remove(LocalNetworkBootstrapFile());
  WriteBootstrapContacts(BootstrapContacts{ 1, BootstrapContact{ GetLocalIp(), kLivePort } },
                         LocalNetworkBootstrapFile());
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
