/* Copyright (c) 2009 maidsafe.net limited
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    * Neither the name of the maidsafe.net limited nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef MAIDSAFE_ROUTING_MAIDSAFE_ROUTING_H_
#define MAIDSAFE_ROUTING_MAIDSAFE_ROUTING_H_

#include "maidsafe/routing/version.h"

#if MAIDSAFE_ROUTING_VERSION != 3107
# error This API is not compatible with the installed library.\
  Please update the maidsafe_routing library.
#endif

#include "boost/asio/io_service.hpp"

#include "maidsafe/transport/transport.h"
#include "maidsafe/transport/message_handler.h"
#include "maidsafe/transport/tcp_transport.h"
#include "maidsafe/transport/udp_transport.h"

namespace maidsafe {

namespace routing {
/// Forward Declerations

class RoutingImpl;
class NodeId;

typedef boost::asio::ip::address IP;
typedef int16_t Port;

/// The size of ROUTING keys and node IDs in bytes.
const uint16_t kKeySizeBytes(64);

/// Size of closest nodes group
const uint16_t kClosestNodes(8);

/// total nodes in routing table
const uint16_t kRoutingTableSize(64);
static_assert(kClosestNodes <= kRoutingTableSize,
              "Cannot set closest nodes larger than routing table");

/// Nodes hint per bucket (hint as buckets will fill more than
/// this when space permits)
const int16_t kBucketSize(1);

typedef std::function<void(std::string &message)> GetValueFunctor;
typedef std::function<void(std::string &messsage)> PassMessageUpFunctor;


class Routing {
 public:
  Routing();
  void Start(boost::asio::io_service &service);
  void Stop();
  bool Running();
  void FindNodes(NodeId &target_id, int16_t num_nodes, std::vector<NodeId> *nodes);
  /// must be set before node can start  This allows node to
  /// check locally for data that's marked cacheable.
  void SetGetDataFunctor(GetValueFunctor &get_local_value);
  /// to recieve messages
  bool RegisterMessageHandler(PassMessageUpFunctor &pass_message_up_handler);
 private:
  std::unique_ptr<RoutingImpl>  Impl;
};


/// to allow message parameter setting and sending
class Message {
 public:
  /// Setters
  explicit Message(const std::string &message);
  void setMessageDestination(const NodeId &id);
  void setResponse(bool reponse);
  void setMessageCacheable(bool cache);
  void setMessageLock(bool lock);
  void setMessageSignature(const std::string &signature);
  void setMessageSignatureId(const std::string &signature_id);
  void setMessageDirect(bool direct);
  /// Getters
  const NodeId MessageDestination();
  bool Responce();
  bool MessageCacheable();
  bool MessageLock();
  const std::string  MessageSignature();
  const NodeId MessageSignatureId();
  bool MessageDirect();
  bool SendMessage();
private:
  Message(const Message&);
  Message &operator=(const Message&);
  
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MAIDSAFE_ROUTING_H_
