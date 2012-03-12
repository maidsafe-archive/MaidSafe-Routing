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

#ifndef MAIDSAFE_ROUTING_API_H_
#define MAIDSAFE_ROUTING_API_H_

#include "maidsafe/routing/version.h"
#if MAIDSAFE_ROUTING_VERSION != 3107
# error This API is not compatible with the installed library.\
  Please update the maidsafe_routing library.
#endif

#include <functional>
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
/// Typedefs
struct Message; // defined below
std::function<void(uint16_t, Message)> ResponseRecievedFunctor;
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
/// Message timeout 

struct Message {
public:
  std::string source_id;
  std::string destination_id;
  bool  cachable;
  std::string data;
  bool direct;
  bool response;
  int32_t type;
  bool failure;
};

class Routing {
 public:
  enum NodeType {kVault, kClient};
  Routing(NodeType nodetype, boost::filesystem3::path & config_file);
  Routing(NodeType nodetype,
          const boost::filesystem3::path & config_file,
          const asymm::PrivateKey &private_key,
          const std::string & node_id);
  ~Routing();
  void Send(const Message &message, ResponseRecievedFunctor response);
  bs2::signal<void(uint16_t, Message)> &RequestReceivedSignal();
  bs2::signal<void(int16_t)> &NetworkStatusSignal();
 private:
  Routing(const Routing&);
  Routing &operator=(const Routing&);
  std::unique_ptr<RoutingPrivate> pimpl_;
};

}  // namespace routing
}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_API_H_
/***********************************************************************
 *
 *    API Documentation
 *
 * *********************************************************************
Guarantees
__________

1: Find any node by key.
2: Find any value by key.
3: Ensure messages are sent to all closest nodes in order (close to furthest)
4: Provide NAT traversal techniques where necessary.
5: Read and Write configuration file to allow bootstrap from known nodes.
6: Read and Write configuration file preserving ID and private key.
7: Allow retrieval of bootsrap nodes from known location.
8: Remove bad nodes from all routing tables (ban from network).
9: Inform of close node changes in routing table.
10:  
 
*/
