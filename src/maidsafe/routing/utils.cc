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

#include "maidsafe/transport/managed_connections.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/node_id.h"
#include "boost/system/system_error.hpp"
#include "boost/filesystem.hpp"
#include "boost/filesystem/fstream.hpp"


namespace maidsafe {

namespace routing {

void SendOn(protobuf::Message &message,
            std::shared_ptr<transport::ManagedConnections> transport,
            std::shared_ptr<RoutingTable> routing_table) {

  std::string signature;
  asymm::Sign(message.data(), routing_table->kKeys().private_key, &signature);
  message.set_signature(signature);
  NodeInfo next_node(routing_table->GetClosestNode(NodeId(message.destination_id()),
                                               0));
// FIXME SEND transport_->Send(next_node.endpoint, message.SerializeAsString());

}

std::vector<transport::Endpoint> ReadBootstrapFile() {
  protobuf::ConfigFile protobuf_config;
  protobuf::Bootstrap protobuf_bootstrap;
  std::vector<transport::Endpoint> bootstrap_nodes;
  fs::path config_file("routing_config_file");  //TODO(dirvine) get correct location

  if (!fs::exists(config_file) || (!fs::is_regular_file(config_file))) {
      DLOG(ERROR) << "Cannot read config file " /*<<
        ec.category().name()*/ << config_file;
   return bootstrap_nodes;
  }

  fs::ifstream config_file_stream;
  try {
    config_file_stream.open(config_file);
  } catch (const boost::filesystem::filesystem_error & ex) {
    DLOG(ERROR) << "Cannot read file stream " << config_file.string() ;
    return bootstrap_nodes;
  }

  if (!protobuf_config.ParseFromIstream(&config_file_stream)) {
    DLOG(ERROR) << "Cannot parse from file stream" ;
    return bootstrap_nodes;
  }

  transport::Endpoint endpoint;
  for (int i = 0; i != protobuf_bootstrap.bootstrap_contacts().size(); ++i) {
    endpoint.ip.from_string(protobuf_bootstrap.bootstrap_contacts(i).ip());
    endpoint.port= protobuf_bootstrap.bootstrap_contacts(i).port();
    bootstrap_nodes.push_back(endpoint);
  }
  return  bootstrap_nodes;
}

bool WriteBootstrapFile(const std::vector<transport::Endpoint> &endpoints) {
  protobuf::Bootstrap protobuf_bootstrap;
  fs::path config_file("routing_config_file");  //TODO(dirvine) get correct location

  if (!fs::exists(config_file) ||
      !fs::is_regular_file(config_file)) {
      DLOG(ERROR) << "Cannot read config file " /*<<
        error_code_.category().name()*/ << config_file;
   return false;
  }

  fs::ofstream config_file_stream;
  try {
    config_file_stream.open(config_file);
  } catch (const boost::filesystem::filesystem_error & ex) {
    DLOG(ERROR) << "Cannot read file stream " << config_file.string() ;
    return false;
  }

  for (size_t i = 0; i < endpoints.size(); ++i) {
    protobuf::Endpoint *endpoint = protobuf_bootstrap.add_bootstrap_contacts();
    endpoint->set_ip(endpoints[i].ip.to_string());
    endpoint->set_port(endpoints[i].port);
  }
  return protobuf_bootstrap.SerializeToOstream(&config_file_stream);
}


}  // namespace routing

}  // namespace maidsafe
