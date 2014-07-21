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

#ifndef MAIDSAFE_ROUTING_BOOTSTRAP_UTILS_H_
#define MAIDSAFE_ROUTING_BOOTSTRAP_UTILS_H_

#include <vector>

#include "boost/asio/ip/udp.hpp"
#include "boost/filesystem/path.hpp"

#include "maidsafe/common/log.h"
#include "maidsafe/routing/bootstrap_file_operations.h"

namespace maidsafe {

namespace routing {

enum class TargetNetwork {
  kSafe,
  kMachineLocalTestnet,
  kMaidSafeInternalTestnet,
  kTestnet01v006
};

template <TargetNetwork>
BootstrapContacts GetBootstrapContacts(bool is_client = true);

template <>
BootstrapContacts GetBootstrapContacts<TargetNetwork::kSafe>(bool is_client);

template <>
BootstrapContacts GetBootstrapContacts<TargetNetwork::kMachineLocalTestnet>(bool is_client);

template <>
BootstrapContacts GetBootstrapContacts<TargetNetwork::kMaidSafeInternalTestnet>(bool is_client);

template <>
BootstrapContacts GetBootstrapContacts<TargetNetwork::kTestnet01v006>(bool is_client);

BootstrapContacts GetZeroStateBootstrapContacts(boost::asio::ip::udp::endpoint local_endpoint);

namespace detail {

template <bool is_client>
BootstrapContacts GetFromLocalFile() {
  BootstrapContacts bootstrap_contacts;
  try {
    static const boost::filesystem::path kBootstrapFilePath{ GetBootstrapFilePath(is_client) };
    bootstrap_contacts = ReadBootstrapContacts(kBootstrapFilePath);
  }
  catch (const std::exception& error) {
    LOG(kWarning) << "Failed to read bootstrap contacts file: " << error.what();
  }
  return bootstrap_contacts;
}

}  // namespace detail

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_BOOTSTRAP_UTILS_H_
