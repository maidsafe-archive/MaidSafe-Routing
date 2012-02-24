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
#include "boost/filesystem.hpp"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/transport/rudp_transport.h"
#include <common/rsa.h>

namespace maidsafe {

namespace routing {

class NodeId;
class PrivateKey;
class RoutingPrivate;


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

// replication shoudl be lower than ClseNodes
const uint16_t kReplicationSize(4);
static_assert(kReplicationSize <= kClosestNodes,
              "Cannot set replication factor larger than closest nodes");

/// Nodes hint per bucket (hint as buckets will fill more than
/// this when space permits)
const int16_t kBucketSize(1);

/// how many chunks to cache (hint as it will be adjusted if mem low)
const uint16_t kNumChunksToCache(100);

typedef std::function<void(std::string &messsage)> PassMessageUpFunctor;

class Routing {
 class Contact;
 public:
  Routing();
  ~Routing();
  bool StartVault(boost::asio::io_service &service);
  bool StartClient(boost::asio::io_service &service);
  void Send(const protobuf::Message &message);
  // TODO FIXME this should be a signal !! 
  bool RegisterMessageHandler(PassMessageUpFunctor &pass_message_up_handler);
  void Stop();
  bool Running();
  // setters
  bool setConfigFilePath(boost::filesystem3::path &);
  bool setMyPrivateKey(asymm::PrivateKey &);
  bool setMyNodeId(NodeId &);
  bool setBootStrapNodes(std::vector<Contact> &);
  // getters
  boost::filesystem3::path ConfigFilePath();
  asymm::PrivateKey MyPrivateKey();
  NodeId MyNodeID();
  std::vector<Contact> BootStrapNodes();
 private:
  Routing(const Routing&);
  Routing &operator=(const Routing&);
  std::unique_ptr<RoutingPrivate> pimpl_;
};
// TODO FIXME - is it forced on us to just include the
// routing.bh.h file so we can prepare messages for sending properly !!
// I think it may be unless we want send to take string and that mease
// an extra serialise / parse stage - too slow !! 
/// to allow message parameter setting and sending
// class ThisMessage {
//  public:
//   /// Setters
//   explicit ThisMessage(const std::string &message);
//   void setMessageDestination(const NodeId &id);
//   void setResponse(bool reponse);
//   void setMessageCacheable(bool cache);
//   void setMessageLock(bool lock);
//   void setMessageSignature(const std::string &signature);
//   void setMessageSignatureId(const std::string &signature_id);
//   void setMessageDirect(bool direct);
//   /// Getters
//   const NodeId MessageDestination();
//   bool Response();
//   bool MessageCacheable();
//   bool MessageLock();
//   const std::string  MessageSignature();
//   const NodeId MessageSignatureId();
//   bool MessageDirect();
// private:
//   ThisMessage(const ThisMessage&);
//   ThisMessage &operator=(const ThisMessage&);
//   
// };

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MAIDSAFE_ROUTING_H_
