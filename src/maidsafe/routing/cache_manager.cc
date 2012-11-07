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

#include "maidsafe/routing/cache_manager.h"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/routing_pb.h"


namespace maidsafe {

namespace routing {

CacheManager::CacheManager() : have_cache_data_(), store_cache_data_() {}

void CacheManager::InitialiseFunctors(HaveCacheDataFunctor have_cache_data,
                                      StoreCacheDataFunctor store_cache_data) {
  assert(have_cache_data);
  assert(store_cache_data);
  have_cache_data_ = have_cache_data;
  store_cache_data_ = store_cache_data;
}

void CacheManager::AddToCache(const protobuf::Message& message) {
  assert(!message.request());
  if (store_cache_data_)
    store_cache_data_(message.data(0));
}

bool CacheManager::GetFromCache(protobuf::Message& message) const {
  assert(message.request());
  if (have_cache_data_) {
    std::string data(message.data(0));
    have_cache_data_(data);
    if (!data.empty()) {

      // TODO(Prakash) : Prepate response
      return true;
    }
  }
  return false;
}

}  // namespace routing

}  // namespace maidsafe
