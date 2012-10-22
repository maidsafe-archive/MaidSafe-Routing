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

#ifndef MAIDSAFE_ROUTING_RANDOM_NODE_HELPER_H_
#define MAIDSAFE_ROUTING_RANDOM_NODE_HELPER_H_

#include <mutex>
#include <vector>

#include "maidsafe/common/node_id.h"


namespace maidsafe {

namespace routing {

class RandomNodeHelper {
 public:
  RandomNodeHelper() : node_ids_(), mutex_(), kMaxSize_(100) {}
  NodeId Get() const;
  void Add(const NodeId& node_id);
  void Remove(const NodeId& node_id);

 private:
  RandomNodeHelper(const RandomNodeHelper&);
  RandomNodeHelper(const RandomNodeHelper&&);
  RandomNodeHelper& operator=(const RandomNodeHelper&);

  std::vector<NodeId> node_ids_;
  mutable std::mutex mutex_;
  const size_t kMaxSize_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_RANDOM_NODE_HELPER_H_
