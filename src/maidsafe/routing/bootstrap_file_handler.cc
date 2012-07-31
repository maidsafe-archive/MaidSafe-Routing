/*******************************************************************************
 *  Copyright 2012 maidsafe.net limited                                        *
 *                                                                             *
 *  The following source code is property of maidsafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the licence   *
 *  file licence.txt found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of maidsafe.net. *
 ******************************************************************************/

#include "maidsafe/routing/bootstrap_file_handler.h"

#include <string>

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing_pb.h"


namespace fs = boost::filesystem;

namespace maidsafe {

namespace routing {

std::vector<boost::asio::ip::udp::endpoint> ReadBootstrapFile(const fs::path& path) {
  protobuf::Bootstrap protobuf_bootstrap;
  std::vector<boost::asio::ip::udp::endpoint> bootstrap_nodes;

  std::string serialised_endpoints;
  if (!ReadFile(path, &serialised_endpoints)) {
    LOG(kError) << "Could not read bootstrap file.";
    return bootstrap_nodes;
  }

  if (!protobuf_bootstrap.ParseFromString(serialised_endpoints)) {
    LOG(kError) << "Could not parse bootstrap file.";
    return bootstrap_nodes;
  }

  bootstrap_nodes.reserve(protobuf_bootstrap.bootstrap_contacts().size());
  for (int i = 0; i < protobuf_bootstrap.bootstrap_contacts().size(); ++i) {
    bootstrap_nodes.push_back(boost::asio::ip::udp::endpoint(
        boost::asio::ip::address::from_string(protobuf_bootstrap.bootstrap_contacts(i).ip()),
        static_cast<uint16_t>(protobuf_bootstrap.bootstrap_contacts(i).port())));
  }

  return bootstrap_nodes;
}

bool WriteBootstrapFile(const std::vector<boost::asio::ip::udp::endpoint> &endpoints,
                        const fs::path& path) {
  protobuf::Bootstrap protobuf_bootstrap;

  for (size_t i = 0; i < endpoints.size(); ++i) {
    protobuf::Endpoint* endpoint = protobuf_bootstrap.add_bootstrap_contacts();
    endpoint->set_ip(endpoints[i].address().to_string());
    endpoint->set_port(endpoints[i].port());
  }

  std::string serialised_bootstrap_nodes;
  if (!protobuf_bootstrap.SerializeToString(&serialised_bootstrap_nodes)) {
    LOG(kError) << "Could not serialise bootstrap contacts.";
    return false;
  }

  if (!WriteFile(path, serialised_bootstrap_nodes)) {
    LOG(kError) << "Could not write bootstrap file.";
    return false;
  }

  return true;
}

}  // namespace routing

}  // namespace maidsafe
