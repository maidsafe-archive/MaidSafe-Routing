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

#ifndef MAIDSAFE_ROUTING_ROUTING_IMPL_H_
#define MAIDSAFE_ROUTING_ROUTING_IMPL_H_


#include <map>
#include <string>
#include <vector>
#include <utility>  // for pair<>
#include "boost/filesystem/path.hpp"
#include "boost/signals2/signal.hpp"

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/transport/managed_connection.h"

#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/routing_table.h"

namespace fs = boost::filesystem;
namespace bs2 = boost::signals2;

namespace maidsafe {

namespace routing {

namespace protobuf {
class Contact;
class Message;
}  // namespace protobuf


struct Message;
class NodeId;

class RoutingImpl {
 public:
  RoutingImpl(Routing::NodeType node_type, const fs::path &config_file);
  RoutingImpl(Routing::NodeType node_type,
              const fs::path &config_file,
              const asymm::PrivateKey &private_key,
              const std::string &node_id);
  void AddBootStrapEndpoint(const transport::Endpoint &endpoint);
  void Send(const Message &message, ResponseReceivedFunctor response_functor);
  // These are public members as RoutingImpl is the d-pointer for Routing.
  bs2::signal<void(int, Message)> message_received_signal_;
  bs2::signal<void(int16_t)> network_status_signal_;

 private:
  void Init();
  bool ReadConfigFile();
  bool WriteConfigFile() const;
  void Join();
  void SendOn(const protobuf::Message &message, const NodeId &target_node);
  void ReceiveMessage(const std::string &message);
  void ProcessMessage(protobuf::Message &message);
  void AddToCache(const protobuf::Message &message);
  bool GetFromCache(protobuf::Message &message);
  void DoPingRequest(protobuf::Message &message);
  void DoPingResponse(const protobuf::Message &message);
  void DoConnectRequest(protobuf::Message &message);
  void DoConnectResponse(const protobuf::Message &message);
  void DoFindNodeRequest(protobuf::Message &message);
  void DoFindNodeResponse(const protobuf::Message &message);
  void DoValidateIdRequest(const protobuf::Message &message);
  void DoValidateIdResponse(const protobuf::Message &message);
  AsioService asio_service_;
  fs::path config_file_;
  std::vector<transport::Endpoint> bootstrap_nodes_;
  asymm::PrivateKey private_key_;
  NodeId node_id_;
  transport::Endpoint node_local_endpoint_;
  transport::Endpoint node_external_endpoint_;
  std::unique_ptr<transport::ManagedConnection> transport_;
  RoutingTable routing_table_;
  std::map<NodeId, asymm::PublicKey> public_keys_;
  unsigned int cache_size_hint_;
  std::vector<std::pair<std::string, std::string>> cache_chunks_;
  bool private_key_is_set_;
  bool node_is_set_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_IMPL_H_
