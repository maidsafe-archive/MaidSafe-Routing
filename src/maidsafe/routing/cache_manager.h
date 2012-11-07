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

class CacheManager {
 public:
  CacheManager();
  void InitialiseFunctors(HaveCacheDataFunctor have_cache_data,
                          StoreCacheDataFunctor store_cache_data);
  void AddToCache(const protobuf::Message& message);
  bool GetFromCache(protobuf::Message& message) const;

 private:
  CacheManager(const CacheManager&);
  CacheManager(const CacheManager&&);
  CacheManager& operator=(const CacheManager&);

  HaveCacheDataFunctor have_cache_data_;
  StoreCacheDataFunctor store_cache_data_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_CACHE_MANAGER_H_
