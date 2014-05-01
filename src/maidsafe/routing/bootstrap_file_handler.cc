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

#include "maidsafe/routing/bootstrap_file_handler.h"

#include <string>

#include "boost/filesystem/path.hpp"

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing.pb.h"

namespace fs = boost::filesystem;

namespace maidsafe {

namespace routing {

namespace {
//
  typedef boost::asio::ip::udp::endpoint Endpoint;

// fs::path BootstrapFilePath() {
//   static fs::path bootstrap_file_path;
//   if (bootstrap_file_path.empty()) {
//     boost::system::error_code exists_error_code, is_regular_file_error_code;
//     bootstrap_file_path = fs::current_path() / "bootstrap";

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

}  // unnamed namespace


// TODO(Team) : Consider timestamp in forming the list. If offline for more than a week, then
// list new nodes first
BootstrapContacts ReadBootstrapFile(const fs::path& bootstrap_file_path) {
  try {
    auto serialised_bootstrap_contacts(ReadFile(bootstrap_file_path));
    protobuf::Bootstrap protobuf_bootstrap;
    if (!protobuf_bootstrap.ParseFromString(serialised_bootstrap_contacts.string())) {
      LOG(kError) << "Could not parse bootstrap file.";
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
    }
    BootstrapContacts bootstrap_contacts;
    bootstrap_contacts.reserve(protobuf_bootstrap.bootstrap_contacts().size());
    for (int i = 0; i < protobuf_bootstrap.bootstrap_contacts().size(); ++i) {
      boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address::from_string(
          protobuf_bootstrap.bootstrap_contacts(i).ip()),
          static_cast<uint16_t>(protobuf_bootstrap.bootstrap_contacts(i).port()));
      BootstrapContact bootstrap_contact = endpoint;
      bootstrap_contacts.push_back(bootstrap_contact);
    }
    std::reverse(std::begin(bootstrap_contacts), std::end(bootstrap_contacts));
    return bootstrap_contacts;
  } catch (const std::exception& e) {
    LOG(kError) << "Failed to read bootstrap file at : " << bootstrap_file_path.string()
                << ". Error : " << boost::diagnostic_information(e);
    throw;
  }
}

void WriteBootstrapFile(const BootstrapContacts& bootstrap_contacts,
                        const fs::path& bootstrap_file_path) {
  protobuf::Bootstrap protobuf_bootstrap;

  for (const auto& bootstrap_contact : bootstrap_contacts) {
    protobuf::Endpoint* endpoint = protobuf_bootstrap.add_bootstrap_contacts();
    endpoint->set_ip(bootstrap_contact.address().to_string());
    endpoint->set_port(bootstrap_contact.port());
  }

  std::string serialised_bootstrap_nodes;
  if (!protobuf_bootstrap.SerializeToString(&serialised_bootstrap_nodes)) {
    LOG(kError) << "Could not serialise bootstrap contacts.";
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::serialisation_error));
  }
  // TODO(Prakash) consider overloading WriteFile() to take NonEmptyString as parameter
  if (!WriteFile(bootstrap_file_path, serialised_bootstrap_nodes)) {
    LOG(kError) << "Could not write bootstrap file at : " << bootstrap_file_path;
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::filesystem_io_error));
  }
}

void UpdateBootstrapFile(const BootstrapContact& bootstrap_contact,
                         const boost::filesystem::path& bootstrap_file_path,
                         bool remove) {
  if (bootstrap_contact.address().is_unspecified()) {
    LOG(kWarning) << "Invalid Endpoint" << bootstrap_contact;
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::invalid_parameter));
  }
  BootstrapContacts bootstrap_contacts(ReadBootstrapFile(bootstrap_file_path));
  auto itr(std::find(std::begin(bootstrap_contacts), std::end(bootstrap_contacts),
                     bootstrap_contact));
  if (remove) {
    if (itr != std::end(bootstrap_contacts)) {
      bootstrap_contacts.erase(itr);
      WriteBootstrapFile(bootstrap_contacts, bootstrap_file_path);
    } else {
      LOG(kVerbose) << "Can't find endpoint to remove : " << bootstrap_contact;
    }
  } else {
    if (itr == std::end(bootstrap_contacts)) {
      bootstrap_contacts.push_back(bootstrap_contact);
      WriteBootstrapFile(bootstrap_contacts, bootstrap_file_path);
    }  else {
      LOG(kVerbose) << "Endpoint already in the list : " << bootstrap_contact;
    }
  }
}

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
