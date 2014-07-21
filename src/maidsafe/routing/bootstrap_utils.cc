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

using boost::asio::ip::udp;
using boost::asio::ip::address;

template <>
BootstrapContacts GetBootstrapContacts<TargetNetwork::kSafe>(bool is_client) {
  BootstrapContacts bootstrap_contacts{
      is_client ? detail::GetFromLocalFile<true>() : detail::GetFromLocalFile<false>() };
  // TODO(Fraser#5#): 2014-07-21 - BEFORE_RELEASE - Add hard-coded endpoints.
  return bootstrap_contacts;
}

template <>
BootstrapContacts GetBootstrapContacts<TargetNetwork::kMachineLocalTestnet>(bool is_client) {
  BootstrapContacts bootstrap_contacts{
      is_client ? detail::GetFromLocalFile<true>() : detail::GetFromLocalFile<false>() };
  if (bootstrap_contacts.empty())
    bootstrap_contacts = BootstrapContacts{ udp::endpoint{ GetLocalIp(), kLivePort } };
  return bootstrap_contacts;
}

template <>
BootstrapContacts GetBootstrapContacts<TargetNetwork::kMaidSafeInternalTestnet>(bool is_client) {
  BootstrapContacts bootstrap_contacts{
      is_client ? detail::GetFromLocalFile<true>() : detail::GetFromLocalFile<false>() };
  if (bootstrap_contacts.empty()) {
    bootstrap_contacts = BootstrapContacts{
        udp::endpoint{ address::from_string("192.168.0.109"), kLivePort },
        udp::endpoint{ address::from_string("192.168.0.4"), kLivePort },
        udp::endpoint{ address::from_string("192.168.0.35"), kLivePort },
        udp::endpoint{ address::from_string("192.168.0.16"), kLivePort },
        udp::endpoint{ address::from_string("192.168.0.20"), kLivePort },
        udp::endpoint{ address::from_string("192.168.0.9"), kLivePort },
        udp::endpoint{ address::from_string("192.168.0.10"), kLivePort },
        udp::endpoint{ address::from_string("192.168.0.19"), kLivePort },
        udp::endpoint{ address::from_string("192.168.0.8"), kLivePort },
        udp::endpoint{ address::from_string("192.168.0.11"), kLivePort },
        udp::endpoint{ address::from_string("192.168.0.13"), kLivePort },
        udp::endpoint{ address::from_string("192.168.0.86"), kLivePort },
        udp::endpoint{ address::from_string("192.168.0.6"), kLivePort },
        udp::endpoint{ address::from_string("192.168.0.55"), kLivePort }
    };
  }
  return bootstrap_contacts;
}

template <>
BootstrapContacts GetBootstrapContacts<TargetNetwork::kTestnet01v006>(bool is_client) {
  BootstrapContacts bootstrap_contacts{
      is_client ? detail::GetFromLocalFile<true>() : detail::GetFromLocalFile<false>() };
  if (bootstrap_contacts.empty()) {
    bootstrap_contacts = BootstrapContacts{
        udp::endpoint{ address::from_string("104.131.253.66"), kLivePort },
        udp::endpoint{ address::from_string("95.85.32.100"), kLivePort },
        udp::endpoint{ address::from_string("128.199.159.50"), kLivePort },
        udp::endpoint{ address::from_string("178.79.156.73"), kLivePort },
        udp::endpoint{ address::from_string("106.185.24.221"), kLivePort },
        udp::endpoint{ address::from_string("23.239.27.245"), kLivePort }
    };
  }
  return bootstrap_contacts;
}

BootstrapContacts GetZeroStateBootstrapContacts(udp::endpoint local_endpoint) {
  BootstrapContacts bootstrap_contacts{ detail::GetFromLocalFile<false>() };
  bootstrap_contacts.erase(std::remove(std::begin(bootstrap_contacts), std::end(bootstrap_contacts),
                                       local_endpoint),
                           std::end(bootstrap_contacts));
  return bootstrap_contacts;
}

}  // namespace routing

}  // namespace maidsafe
