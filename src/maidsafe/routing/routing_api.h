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

#ifndef MAIDSAFE_ROUTING_API_H_
#define MAIDSAFE_ROUTING_API_H_

#include <functional>
#include "boost/signals2.hpp"
#include "boost/asio/io_service.hpp"
#include "boost/filesystem.hpp"
#include "maidsafe/transport/rudp_transport.h"
#include "common/rsa.h"
#include "maidsafe/routing/version.h"

#if MAIDSAFE_ROUTING_VERSION != 100
#  error This API is not compatible with the installed library.\
  Please update the maidsafe_routing library.
#endif


namespace maidsafe {

namespace routing {

class NodeId;
class PrivateKey;
class RoutingPrivate;
struct Message;

namespace bs2 = boost::signals2;

typedef std::function<void(uint16_t, Message)> ResponseRecievedFunctor;

struct Parameters {
  // The size of a group of closest nodes.
  static const unsigned int kClosestNodesSize;
  static const unsigned int kMaxRoutingTableSize;
  // Target number of nodes per bucket (buckets will fill more than this when
  // space permits).
  static const unsigned int kBucketTargetSize;
  static const unsigned int kNumChunksToCache;
};

struct Message {
public:
  Message();
  std::string source_id;
  std::string destination_id;
  bool  cachable;
  std::string data;
  bool direct;
  bool response;
  int32_t replication;
  int32_t type;
  bool routing_failure;
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
  bs2::signal<void(uint16_t, std::string)> &RequestReceivedSignal();
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
