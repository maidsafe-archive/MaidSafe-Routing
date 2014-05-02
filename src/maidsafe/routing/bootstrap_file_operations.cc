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

#include "maidsafe/routing/bootstrap_file_operations.h"

#include <string>

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing.pb.h"

namespace fs = boost::filesystem;

namespace maidsafe {

namespace routing {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

BootstrapContacts ReadBootstrapFileInternal(const fs::path& bootstrap_file_path) {
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
    return bootstrap_contacts;
  } catch (const std::exception& e) {
    LOG(kError) << "Failed to read bootstrap file at : " << bootstrap_file_path.string()
                << ". Error : " << boost::diagnostic_information(e);
    throw;
  }
}

}  // unnamed namespace


// TODO(Team) : Consider timestamp in forming the list. If offline for more than a week, then
// list new nodes first
BootstrapContacts ReadBootstrapFile(const fs::path& bootstrap_file_path) {
  auto bootstrap_contacts(ReadBootstrapFileInternal(bootstrap_file_path));
  std::reverse(std::begin(bootstrap_contacts), std::end(bootstrap_contacts));
  return bootstrap_contacts;
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
  BootstrapContacts bootstrap_contacts(ReadBootstrapFileInternal(bootstrap_file_path));
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

}  // namespace routing

}  // namespace maidsafe
