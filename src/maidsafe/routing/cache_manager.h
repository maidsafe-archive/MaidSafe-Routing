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

#ifndef MAIDSAFE_ROUTING_CACHE_MANAGER_H_
#define MAIDSAFE_ROUTING_CACHE_MANAGER_H_

#include "boost/thread/shared_mutex.hpp"
#include "boost/thread/mutex.hpp"
#include "maidsafe/common/rsa.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/log.h"


namespace maidsafe {

namespace routing {

class CacheManager {
 public:
  CacheManager(RoutingTable &routing_table,
               rudp::ManagedConnections &rudp);
  void AddToCache(const protobuf::Message &message);
  bool GetFromCache(protobuf::Message &message);
 private:
  CacheManager(const CacheManager&);  // no copy
  CacheManager& operator=(const CacheManager&);  // no assign
  std::vector<std::pair<std::string, std::string> > cache_chunks_;
  rudp::ManagedConnections &rudp_;
  RoutingTable &routing_table_;
};

}  // namespace routing

}  // namespace maidsafe

#endif // MAIDSAFE_ROUTING_CACHE_MANAGER_H_
