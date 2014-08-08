/*  Copyright 2013 MaidSafe.net limited

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

#include "maidsafe/routing/public_key_holder.h"
#include "maidsafe/routing/parameters.h"

namespace maidsafe {

namespace routing {

PublicKeyHolder::PublicKeyHolder(AsioService& asio_service)
    : mutex_(), timer_(asio_service), public_keys_() {}

PublicKeyHolder::~PublicKeyHolder() {
 std::lock_guard<std::mutex> lock(mutex_);
  while (!public_keys_.empty())
    timer_.CancelTask(std::begin(public_keys_)->first);
}

void PublicKeyHolder::Add(const NodeId& node_id, const asymm::PublicKey& public_key) {
  TaskId task_id(timer_.NewTaskId());
  {
    std::lock_guard<std::mutex> lock(mutex_);
    public_keys_.insert(std::make_pair(task_id, std::make_pair(node_id, public_key)));
  }
  timer_.AddTask(Parameters::default_response_timeout,
                 [this, task_id](std::string /*public_key_holder*/) {
                   std::lock_guard<std::mutex> lock(this->mutex_);
                   this->public_keys_.erase(task_id);
                 }, 1, task_id);
}

boost::optional<asymm::PublicKey> PublicKeyHolder::Find(const NodeId& node_id) const {
  boost::optional<asymm::PublicKey> public_key;

  std::lock_guard<std::mutex> lock(mutex_);
  std::for_each(std::begin(public_keys_), std::end(public_keys_),
                [node_id, &public_key](
                    const std::pair<TaskId, std::pair<NodeId, asymm::PublicKey>>& pair) {
                  if (pair.second.first == node_id)
                    public_key = pair.second.second;
                });
  return public_key;
}

void PublicKeyHolder::Remove(const NodeId& node_id) {
  std::vector<TaskId> task_ids;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::for_each(std::begin(public_keys_), std::end(public_keys_),
                  [node_id, &task_ids](
                      const std::pair<TaskId, std::pair<NodeId, asymm::PublicKey>>& pair) {
                    if (pair.second.first == node_id)
                      task_ids.push_back(pair.first);
                  });
    for (const auto& task_id : task_ids)
      public_keys_.erase(task_id);
  }
  for (const auto& task_id : task_ids)
    timer_.CancelTask(task_id);
}

}  // namespace routing

}  // namespace maidsafe

