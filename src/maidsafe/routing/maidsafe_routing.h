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

#include "boost/signals2.hpp"
#include "boost/asio/io_service.hpp"
#include "boost/filesystem.hpp"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/transport/rudp_transport.h"
#include "common/rsa.h"

namespace maidsafe {

namespace routing {
/// forward declarations
class NodeId;
class PrivateKey;
class RoutingPrivate;
/// Aliases
namespace bs2 = boost::signals2;

/// The size of ROUTING keys and node IDs in bytes.
const uint16_t kKeySizeBytes(64);
/// Size of closest nodes group
const uint16_t kClosestNodes(8);
/// total nodes in routing table
const uint16_t kRoutingTableSize(64);
// replication should be lower than CloseNodes
const uint16_t kReplicationSize(4);
/// Nodes hint per bucket (hint as buckets will fill more than
/// this when space permits)
const int16_t kBucketSize(1);
/// how many chunks to cache (hint as it will be adjusted if mem low)
const uint16_t kNumChunksToCache(100);

class Routing {
 class Contact;
 public:
  Routing();
  ~Routing();
  bool StartVault(boost::asio::io_service &service);
  bool StartClient(boost::asio::io_service &service);
  void Send(const protobuf::Message &message);
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
  // references to Signal objects
  bs2::signal<void(uint16_t, std::string)> &MessageReceivedSignal();
  bs2::signal<void(int16_t)> &NetworkStatusSignal();
 private:
  Routing(const Routing&);
  Routing &operator=(const Routing&);
  std::unique_ptr<RoutingPrivate> pimpl_;
};

}  // namespace routing
}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MAIDSAFE_ROUTING_H_
