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

#include "maidsafe/routing/random_node_helper.h"

#include "maidsafe/common/utils.h"


namespace maidsafe {

namespace routing {

NodeId RandomNodeHelper::Get() const {
  std::lock_guard<std::mutex> lock(mutex_);
  assert(node_ids_.size() <= kMaxSize_);
  if (node_ids_.empty())
    return NodeId();
  return node_ids_[(node_ids_.size() == kMaxSize_) ? 0 : (RandomUint32() % node_ids_.size())];
}

void RandomNodeHelper::Add(const NodeId& node_id) {
  assert(!node_id.IsZero());
  std::lock_guard<std::mutex> lock(mutex_);
  if (std::find(node_ids_.begin(), node_ids_.end(), node_id) != node_ids_.end())
    return;

  node_ids_.push_back(node_id);
  if (node_ids_.size() > kMaxSize_)
    node_ids_.erase(node_ids_.begin());
}

void RandomNodeHelper::Remove(const NodeId& node_id) {
  assert(!node_id.IsZero());
  std::lock_guard<std::mutex> lock(mutex_);
  auto itr(std::find(node_ids_.begin(), node_ids_.end(), node_id));
  if (itr != node_ids_.end())
    node_ids_.erase(itr);
}

}  // namespace routing

}  // namespace maidsafe
