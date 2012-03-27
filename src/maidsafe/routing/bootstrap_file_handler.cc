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

#include "boost/thread/shared_mutex.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/filesystem.hpp"
#include "boost/filesystem/fstream.hpp"
#include "maidsafe/common/utils.h"
#include "maidsafe/transport/managed_connections.h"
#include "maidsafe/routing/bootstrap_file_handler.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/log.h"

namespace maidsafe {

namespace routing {

std::vector<transport::Endpoint> BootStrapFile::ReadBootstrapFile() {
  protobuf::Bootstrap protobuf_bootstrap;
  std::vector<transport::Endpoint> bootstrap_nodes;
   if (!GetFilePath())
     return bootstrap_nodes;

  std::string serialised_endpoints;
  if (!ReadFile(file_path_, &serialised_endpoints)) {
     DLOG(ERROR) << "could not read bootstrap file";
    return bootstrap_nodes;
  }
  if (!protobuf_bootstrap.ParseFromString(serialised_endpoints)) {
    DLOG(ERROR) << "could not parse bootstrap file";
    return bootstrap_nodes;
  }
  bootstrap_nodes.resize(protobuf_bootstrap.bootstrap_contacts().size());
  transport::Endpoint endpoint;
  transport::IP ip;
  for (int i = 0; i < protobuf_bootstrap.bootstrap_contacts().size(); ++i) {
    endpoint.ip = ip.from_string(protobuf_bootstrap.bootstrap_contacts(i).ip());
    endpoint.port = protobuf_bootstrap.bootstrap_contacts(i).port();
    bootstrap_nodes[i] = endpoint;
  }

  return  bootstrap_nodes;
}

bool BootStrapFile::WriteBootstrapFile(const std::vector<transport::Endpoint>
                                                                  &endpoints) {
  protobuf::Bootstrap protobuf_bootstrap;
  if (!GetFilePath()) {
    DLOG(ERROR) << "could not write bootstrap file";
    return false;
  }

  for (size_t i = 0; i < endpoints.size(); ++i) {
    protobuf::Endpoint *endpoint = protobuf_bootstrap.add_bootstrap_contacts();
    endpoint->set_ip(endpoints[i].ip.to_string());
    endpoint->set_port(endpoints[i].port);
  }
  std::string serialised_bootstrap_nodes;
  protobuf_bootstrap.SerializeToString(&serialised_bootstrap_nodes);
  return WriteFile(file_path_, serialised_bootstrap_nodes);
}

bool BootStrapFile::GetFilePath() {
//TODO(dirvine) get operating system correct location
// for now just get the name of the local file
// FIXME we should iterate through the system paths
  file_path_ = Parameters::bootstrap_file_path;
  std::string dummy_content;
  // seems daft but we will iterate paths soon enough
  if ((fs::exists(file_path_) && fs::is_regular_file(file_path_)) ||
      (WriteFile(file_path_, dummy_content) && fs::remove(file_path_))) {
    file_path_set_ = true;
    return true;
  } else {
    DLOG(ERROR) << "Cannot read/write file stream " << file_path_.string();
    file_path_set_ = false;
    return false;
  }
}



}  // namespace routing

}  // namespace maidsafe
