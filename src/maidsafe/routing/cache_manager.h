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

#include <string>

#include "maidsafe/routing/api_config.h"

namespace maidsafe {

namespace routing {

namespace protobuf { class Message; }

class NetworkUtils;

class CacheManager {
 public:
  CacheManager(const NodeId& node_id, NetworkUtils &network);

  void InitialiseFunctors(MessageReceivedFunctor message_received_functor,
                          StoreCacheDataFunctor store_cache_data);
  void AddToCache(const protobuf::Message& message);
  void HandleGetFromCache(protobuf::Message& message);

 private:
  CacheManager(const CacheManager&);
  CacheManager(const CacheManager&&);
  CacheManager& operator=(const CacheManager&);

  const NodeId kNodeId_;
  NetworkUtils& network_;
  MessageReceivedFunctor message_received_functor_;
  StoreCacheDataFunctor store_cache_data_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_CACHE_MANAGER_H_
