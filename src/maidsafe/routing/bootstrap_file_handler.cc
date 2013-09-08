/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.novinet.com/license

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#include "maidsafe/routing/bootstrap_file_handler.h"

#include <string>

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing.pb.h"


namespace fs = boost::filesystem;

namespace maidsafe {

namespace routing {

// namespace {
//
typedef boost::asio::ip::udp::endpoint Endpoint;

// fs::path BootstrapFilePath() {
//   static fs::path bootstrap_file_path;
//   if (bootstrap_file_path.empty()) {
//     boost::system::error_code exists_error_code, is_regular_file_error_code;
//     bootstrap_file_path = fs::current_path() / "bootstrap";
//
//     if (!fs::exists(bootstrap_file_path, exists_error_code) ||
//         !fs::is_regular_file(bootstrap_file_path, is_regular_file_error_code) ||
//         exists_error_code || is_regular_file_error_code) {
//       if (exists_error_code) {
//         LOG(kWarning) << "Failed to find bootstrap file at " << bootstrap_file_path << ".  "
//                       << exists_error_code.message();
//       }
//       if (is_regular_file_error_code) {
//         LOG(kWarning) << "bootstrap file is not a regular file " << bootstrap_file_path << ".  "
//                       << is_regular_file_error_code.message();
//       }
//       LOG(kInfo) << "No bootstrap file";
//       bootstrap_file_path.clear();
//     } else {
//       LOG(kVerbose) << "Found bootstrap file at " << bootstrap_file_path;
//     }
//   }
//   return bootstrap_file_path;
// }
//
// }  // unnamed namespace
//
// std::vector<boost::asio::ip::udp::endpoint> ReadBootstrapFile() {
//   fs::path bootstrap_file_path(BootstrapFilePath());
//   std::vector<boost::asio::ip::udp::endpoint> bootstrap_nodes;
//   std::string serialised_endpoints;
//   if (bootstrap_file_path.empty() || !ReadFile(bootstrap_file_path, &serialised_endpoints)) {
//     LOG(kError) << "Could not read bootstrap file : " << bootstrap_file_path.string();
//     return bootstrap_nodes;
//   }
//
//   protobuf::Bootstrap protobuf_bootstrap;
//   if (!protobuf_bootstrap.ParseFromString(serialised_endpoints)) {
//     LOG(kError) << "Could not parse bootstrap file.";
//     return bootstrap_nodes;
//   }
//
//   bootstrap_nodes.reserve(protobuf_bootstrap.bootstrap_contacts().size());
//   for (int i = 0; i < protobuf_bootstrap.bootstrap_contacts().size(); ++i) {
//     bootstrap_nodes.push_back(boost::asio::ip::udp::endpoint(
//         boost::asio::ip::address::from_string(protobuf_bootstrap.bootstrap_contacts(i).ip()),
//         static_cast<uint16_t>(protobuf_bootstrap.bootstrap_contacts(i).port())));
//   }
//
//   std::reverse(bootstrap_nodes.begin(), bootstrap_nodes.end());
//   return bootstrap_nodes;
// }
//
// bool WriteBootstrapFile(const std::vector<boost::asio::ip::udp::endpoint>& endpoints,
//                         const fs::path& bootstrap_file_path) {
//   protobuf::Bootstrap protobuf_bootstrap;
//
//   for (size_t i = 0; i < endpoints.size(); ++i) {
//     protobuf::Endpoint* endpoint = protobuf_bootstrap.add_bootstrap_contacts();
//     endpoint->set_ip(endpoints[i].address().to_string());
//     endpoint->set_port(endpoints[i].port());
//   }
//
//   std::string serialised_bootstrap_nodes;
//   if (!protobuf_bootstrap.SerializeToString(&serialised_bootstrap_nodes)) {
//     LOG(kError) << "Could not serialise bootstrap contacts.";
//     return false;
//   }
//
//   if (!WriteFile(bootstrap_file_path, serialised_bootstrap_nodes)) {
//     LOG(kError) << "Could not write bootstrap file.";
//     return false;
//   }
//
//   return true;
// }
//
// void UpdateBootstrapFile(const boost::asio::ip::udp::endpoint& endpoint, bool remove) {
//   fs::path bootstrap_file_path(BootstrapFilePath());
//   if (bootstrap_file_path.empty()) {
// //     LOG(kWarning) << "Empty bootstrap file path" << path;
//     return;
//   }
//
//   if (endpoint.address().is_unspecified()) {
//     LOG(kWarning) << "Invalid Endpoint" << endpoint;
//     return;
//   }
//
//   std::vector<boost::asio::ip::udp::endpoint> bootstrap_endpoints(ReadBootstrapFile());
//   auto itr(std::find(bootstrap_endpoints.begin(), bootstrap_endpoints.end(), endpoint));
//   if (remove) {
//     if (itr != bootstrap_endpoints.end()) {
//       bootstrap_endpoints.erase(itr);
//     } else {
//       LOG(kVerbose) << "Can't find endpoint to remove : " << endpoint;
//       return;
//     }
//   } else {
//     if (itr == bootstrap_endpoints.end()) {
//       bootstrap_endpoints.insert(bootstrap_endpoints.begin(), endpoint);
//     }  else {
//       LOG(kVerbose) << "Endpoint already in the list : " << endpoint;
//       return;
//     }
//   }
//
//   if (!WriteBootstrapFile(bootstrap_endpoints, bootstrap_file_path))
//     LOG(kError) << "Failed to write bootstrap file back to " << bootstrap_file_path;
//   else
//     LOG(kVerbose) << "Updated bootstrap file : " << bootstrap_file_path;
//   return;
// }

std::vector<boost::asio::ip::udp::endpoint> MaidSafeEndpoints() {
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

  std::vector<boost::asio::ip::udp::endpoint> maidsafe_endpoints;
  for (const auto& i : endpoint_string)
    maidsafe_endpoints.push_back(Endpoint(boost::asio::ip::address::from_string(i), 5483));
  return maidsafe_endpoints;
}

std::vector<boost::asio::ip::udp::endpoint> MaidSafeLocalEndpoints() {
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

  std::vector<boost::asio::ip::udp::endpoint> maidsafe_endpoints;
  for (const auto& i : endpoint_string)
    maidsafe_endpoints.push_back(Endpoint(boost::asio::ip::address::from_string(i), 5483));
  return maidsafe_endpoints;
}

}  // namespace routing

}  // namespace maidsafe
