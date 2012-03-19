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

Client connects with a made up address (which must be unique (he can remember it))
but requres to sign with a valid PMID
Connects to closest nodes plus some others (no need to update much)
Send all requests via closest node to destination_id
all returns will come through close nodes mostly unless we allow
by proxy which I think we should.
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
struct Message;
class RoutingImpl;

typedef std::function<void(int,
                           maidsafe::routing::Message)> ResponseReceivedFunctor;

struct Message {
 public:
  Message();
  explicit Message(const protobuf::Message &protobuf_message);
  std::string source_id;
  std::string destination_id;
  std::string target_name;
  bool cacheable;
  std::string data;
  bool direct;
  bool response;
  int32_t replication;
  int32_t type;
  bool routing_failure;
};

class Routing {
 public:
  enum NodeType { kVault, kClient };
  Routing(NodeType node_type, const boost::filesystem::path &config_dir);
  Routing(NodeType node_type,
          const boost::filesystem::path &config_dir,
          const asymm::PrivateKey &private_key,
          const std::string &node_id);
  ~Routing();
  void BootStrapFromThisEndpoint(const maidsafe::transport::Endpoint& endpoint);
  void Send(const Message &message,
            const ResponseReceivedFunctor &response_functor);
  boost::signals2::signal<void(int, Message)> &RequestReceivedSignal();
  boost::signals2::signal<void(int16_t)> &NetworkStatusSignal();

 private:
  Routing(const Routing&);
  Routing& operator=(const Routing&);
  std::unique_ptr<RoutingImpl> pimpl_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_API_H_
