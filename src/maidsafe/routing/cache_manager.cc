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
#include "maidsafe/routing/utils.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"

namespace maidsafe {

namespace routing {

CacheManager::CacheManager(uint16_t cache_size_hint,
                    std::shared_ptr<RoutingTable> routing_table,
                    std::shared_ptr<transport::ManagedConnections> transport)
                    : cache_size_hint_(cache_size_hint),
                    cache_chunks_(),
                    transport_(transport),
                    routing_table_(routing_table)
                    {}


void CacheManager::AddToCache(const protobuf::Message& message) {
    std::pair<std::string, std::string> data;
  try {
    // check data is valid TODO FIXME - ask CAA
    if (crypto::Hash<crypto::SHA512>(message.data()) != message.source_id())
      return;
    data = std::make_pair(message.source_id(), message.data());
    cache_chunks_.push_back(data);
    while (cache_chunks_.size() > cache_size_hint_)
      cache_chunks_.erase(cache_chunks_.begin());
  }
  catch(const std::exception &/*e*/) {
    // oohps reduce cache size quickly
    cache_size_hint_ = cache_size_hint_ / 2;
    while (cache_chunks_.size() > cache_size_hint_)
      cache_chunks_.erase(cache_chunks_.begin()+1);
  }
}

bool CacheManager::GetFromCache(protobuf::Message &message) {
    for (auto it = cache_chunks_.begin(); it != cache_chunks_.end(); ++it) {
      if ((*it).first == message.source_id()) {
        message.set_destination_id(message.source_id());
        message.set_cacheable(true);
        message.set_data((*it).second);
        message.set_source_id(routing_table_->kKeys().identity);
        message.set_direct(true);
        message.set_response(false);
        SendOn(message, transport_, routing_table_);
        return true;
      }
  }
  return false;
}

}  // namespace routing
}  // namespace maidsafe
