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

/*
Guarantees
__________

1:  Find any node by key.
2:  Find any value by key.
3:  Ensure messages are sent to all closest nodes in order (close to furthest).
4:  Provide NAT traversal techniques where necessary.
5:  Read and Write configuration file to allow bootstrap from known nodes.
6:  Read and Write configuration file preserving ID and private key.
7:  Allow retrieval of bootstrap nodes from known location.
8:  Remove bad nodes from all routing tables (ban from network).
9:  Inform of close node changes in routing table.
10: Respond to every send that requires it, either with timeout or reply

Client connects with a made up address (which must be unique (he can remember it))
but requres to sign with a valid PMID
Connects to closest nodes plus some others (no need to update much)
all returns will come through close nodes mostly unless we allow
by proxy which I think we may.
We can detect a client and wrap his message in one of ours
this can make sure they can only do certain things as well (no accounts/CIH etc.)
we can register client acceptable messages or vault accptable and allow
clients anything else ?
if a client or hacker tries to start as a vault ID it will not be unique
  we can use dans signal block to make sure clients only get signalled client stuff
  this will help is we release a public API and soembody tries to get smart
  (help not solve)
  The node passing the message back knows its a client he is talking to and if the message is a vault
  type he can drop it.
*/

#ifndef MAIDSAFE_ROUTING_ROUTING_API_H_
#define MAIDSAFE_ROUTING_ROUTING_API_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>  // for pair
#include <map>
#include <vector>
#include "boost/signals2/signal.hpp"
#include "boost/filesystem/path.hpp"
#include "maidsafe/common/rsa.h"
#include "maidsafe/transport/managed_connection.h"
#include "maidsafe/routing/version.h"

#if MAIDSAFE_ROUTING_VERSION != 100
#  error This API is not compatible with the installed library.\
  Please update the maidsafe_routing library.
#endif

namespace maidsafe {

namespace routing {

namespace protobuf { class Message; }

class RoutingTable;
class NodeId;
class NodeInfo;
class Service;
class Rpcs;
class Timer;

struct Message {
 public:
  Message();
  explicit Message(const protobuf::Message &protobuf_message);
  int32_t type;
  std::string source_id;
  std::string destination_id;
  std::string data;
  bool timeout;
  bool cacheable;
  bool direct;
  int32_t replication;
};

typedef std::function<void(int, std::string)> ResponseReceivedFunctor;

class Routing {
 public:
  enum NodeType { kVault, kClient };
  Routing(NodeType node_type,
          const asymm::PrivateKey &private_key,
          const std::string &node_id,
          bool encryption_required);  // full asymm encrypt of all messages
  ~Routing();
  void BootStrapFromThisEndpoint(const maidsafe::transport::Endpoint& endpoint);
  int Send(const Message &message,
            const ResponseReceivedFunctor &response_functor);
  void ValidateThisNode(bool valid,
                        std::string node_id,
                        asymm::PublicKey &public_key);  // upper layers must
                        //  do this in response to the ValidateNodeId signal
  boost::signals2::signal<void(int, std::string)> &RequestReceivedSignal();
  boost::signals2::signal<void(unsigned int)> &NetworkStatusSignal();
  boost::signals2::signal<void(std::string)> &ValidateNodeIdSignal();

 private:
  Routing(const Routing&);  // no copy
  Routing& operator=(const Routing&);  // no assign
  void Init();
  bool ReadBootstrapFile();
  bool WriteBootstrapFile() const;
  void Join();
  void SendOn(const protobuf::Message &message);
  void AckReceived(const transport::TransportCondition &, const std::string &);
  void ReceiveMessage(const std::string &message);
  void ProcessMessage(protobuf::Message &message);
  void ProcessPingResponse(protobuf::Message &message);
  void ProcessConnectResponse(protobuf::Message &message);
  void ProcessFindNodeResponse(protobuf::Message &message);
  void FindAndKillWaitingNodeValidation(NodeId node);
  void AddToCache(const protobuf::Message &message);
  bool GetFromCache(protobuf::Message &message);
  void TryAddNode(NodeId node);
  AsioService asio_service_;
  fs::path bootstrap_file_;
  std::vector<transport::Endpoint> bootstrap_nodes_;
  asymm::PrivateKey private_key_;
  transport::Endpoint node_local_endpoint_;
  transport::Endpoint node_external_endpoint_;
  std::shared_ptr<transport::ManagedConnection> transport_;
  std::shared_ptr<RoutingTable> routing_table_;
  std::shared_ptr<Rpcs> rpc_ptr_;
  std::shared_ptr<Service> service_;
  std::shared_ptr<Timer> timer_;
  boost::signals2::signal<void(int, std::string)> message_received_signal_;
  boost::signals2::signal<void(unsigned int)> network_status_signal_;
  boost::signals2::signal<void(std::string)> validate_node_signal_;
  unsigned int cache_size_hint_;
  std::vector<std::pair<std::string, std::string>> cache_chunks_;
  std::vector<NodeInfo> waiting_node_validation_;
  std::map<uint32_t, std::pair<std::shared_ptr<boost::asio::deadline_timer>,
                              ResponseReceivedFunctor> > waiting_for_response_;
  std::vector<NodeInfo> client_connections_;
  bool joined_;
  bool signatures_required_;
  bool encryption_required_;
  Routing::NodeType node_type_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_API_H_
