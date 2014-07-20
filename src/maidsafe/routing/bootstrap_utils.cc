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

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"

namespace maidsafe {

namespace routing {

namespace {
  typedef boost::asio::ip::udp::endpoint Endpoint;
}

BootstrapContacts MaidSafeBootstrapContacts() {
  std::vector<std::string> endpoint_string;
  endpoint_string.reserve(6);
  endpoint_string.push_back("104.131.253.66");
  endpoint_string.push_back("95.85.32.100");
  endpoint_string.push_back("128.199.159.50");
  endpoint_string.push_back("178.79.156.73");
  endpoint_string.push_back("106.185.24.221");
  endpoint_string.push_back("23.239.27.245");

  BootstrapContacts maidsafe_endpoints;
  for (const auto& i : endpoint_string)
    maidsafe_endpoints.push_back(Endpoint(boost::asio::ip::address::from_string(i), 5483));
  return maidsafe_endpoints;
}

BootstrapContacts MaidSafeLocalBootstrapContacts() {
  std::vector<std::string> endpoint_string;

#if defined QA_BUILD
//  LOG(kVerbose) << "Appending 192.168.0.130:5483 to bootstrap endpoints";
//  endpoint_string.reserve(2);
//  endpoint_string.push_back("192.168.0.130");
#elif defined TESTING
  LOG(kVerbose) << "Appending maidsafe local endpoints to bootstrap endpoints";
  endpoint_string.reserve(14);
  endpoint_string.push_back("192.168.0.109");
  endpoint_string.push_back("192.168.0.4");
  endpoint_string.push_back("192.168.0.35");
  endpoint_string.push_back("192.168.0.16");
  endpoint_string.push_back("192.168.0.20");
  endpoint_string.push_back("192.168.0.9");
  endpoint_string.push_back("192.168.0.10");
  endpoint_string.push_back("192.168.0.19");
  endpoint_string.push_back("192.168.0.8");
  endpoint_string.push_back("192.168.0.11");
  endpoint_string.push_back("192.168.0.13");
  endpoint_string.push_back("192.168.0.86");
  endpoint_string.push_back("192.168.0.6");
  endpoint_string.push_back("192.168.0.55");
#endif
  assert(endpoint_string.size() &&
         "Either QA_BUILD or TESTING must be defined to use maidsafe local endpoint option");

  BootstrapContacts maidsafe_endpoints;
  for (const auto& i : endpoint_string)
    maidsafe_endpoints.push_back(Endpoint(boost::asio::ip::address::from_string(i), 5483));
  return maidsafe_endpoints;
}

}  // namespace routing

}  // namespace maidsafe
