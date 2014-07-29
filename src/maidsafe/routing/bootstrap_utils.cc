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
  endpoint_string.reserve(15);
  endpoint_string.push_back("176.58.120.133");
  endpoint_string.push_back("178.79.163.139");
  endpoint_string.push_back("176.58.102.53");
  endpoint_string.push_back("176.58.113.214");
  endpoint_string.push_back("106.187.49.208");
  endpoint_string.push_back("198.74.60.81");
  endpoint_string.push_back("198.74.60.83");
  endpoint_string.push_back("198.74.60.84");
  endpoint_string.push_back("198.74.60.85");
  endpoint_string.push_back("198.74.60.86");
  endpoint_string.push_back("176.58.103.83");
  endpoint_string.push_back("106.187.102.233");
  endpoint_string.push_back("106.187.47.248");
  endpoint_string.push_back("106.187.93.100");
  endpoint_string.push_back("106.186.16.51");

  BootstrapContacts maidsafe_endpoints;
  for (const auto& i : endpoint_string)
    maidsafe_endpoints.push_back(Endpoint(boost::asio::ip::address::from_string(i), 5483));
  return maidsafe_endpoints;
}

BootstrapContacts MaidSafeLocalBootstrapContacts() {
  std::vector<std::string> endpoint_string;
  endpoint_string.reserve(2);
#if defined QA_BUILD
  LOG(kVerbose) << "Appending 192.168.0.130:5483 to bootstrap endpoints";
  endpoint_string.push_back("192.168.0.130");
#elif defined TESTING
  LOG(kVerbose) << "Appending 192.168.0.109:5483 to bootstrap endpoints";
  endpoint_string.push_back("192.168.0.109");
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
