/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

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
  assert(node_id.IsValid());
  std::lock_guard<std::mutex> lock(mutex_);
  if (std::find(node_ids_.begin(), node_ids_.end(), node_id) != node_ids_.end())
    return;

  node_ids_.push_back(node_id);
  if (node_ids_.size() > kMaxSize_)
    node_ids_.erase(node_ids_.begin());
}

void RandomNodeHelper::Remove(const NodeId& node_id) {
  assert(node_id.IsValid());
  std::lock_guard<std::mutex> lock(mutex_);
  auto itr(std::find(node_ids_.begin(), node_ids_.end(), node_id));
  if (itr != node_ids_.end())
    node_ids_.erase(itr);
}

}  // namespace routing

}  // namespace maidsafe
