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
#include <utility>
#include <vector>

#include "boost/thread/mutex.hpp"
#include "maidsafe/routing/message.h"
#include "maidsafe/routing/log.h"

namespace maidsafe {

namespace routing {

namespace protobuf { class Message;}  // namespace protobuf

class CacheManager {
 public:
  CacheManager();
  void AddToCache(Message &message, std::string name);
  bool GetFromCache(Message &message, std::string name);
 private:
  CacheManager(const CacheManager&);  // no copy
  CacheManager(const CacheManager&&);  // no move
  CacheManager& operator=(const CacheManager&);  // no assign
  std::vector<std::pair<std::string, std::string> > cache_chunks_;
  boost::mutex mutex_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_CACHE_MANAGER_H_
