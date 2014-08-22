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

#ifndef MAIDSAFE_ROUTING_TIMED_CONTAINER_H_
#define MAIDSAFE_ROUTING_TIMED_CONTAINER_H_

#include <utility>
#include <map>
#include <string>
#include <vector>

#include "maidsafe/routing/timer.h"

namespace maidsafe {

namespace routing {

template <typename T, unsigned int DefaultTimeout = 10>
class TimedContainer {
 public:
  explicit TimedContainer(AsioService& asio_service);
  TimedContainer(const TimedContainer&) = delete;
  TimedContainer& operator=(const TimedContainer&) = delete;
  ~TimedContainer();
  bool Add(const T& value,
           const std::chrono::steady_clock::duration& timeout =
               std::chrono::seconds(DefaultTimeout));
  boost::optional<T> Find(const typename T::Key& key) const;
  void Remove(const typename T::Key& key);

 private:
  mutable std::mutex mutex_;
  Timer<std::string> timer_;
  std::map<TaskId, T> elements_;
};

template <typename T, unsigned int DefaultTimeout>
TimedContainer<T, DefaultTimeout>::TimedContainer(AsioService& asio_service)
    : mutex_(), timer_(asio_service), elements_() {}

template <typename T, unsigned int DefaultTimeout>
TimedContainer<T, DefaultTimeout>::~TimedContainer() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    elements_.clear();
  }
  timer_.CancelAll();
}

template <typename T, unsigned int DefaultTimeout>
bool TimedContainer<T, DefaultTimeout>::Add(
    const T& value, const std::chrono::steady_clock::duration& timeout) {
  TaskId task_id(timer_.NewTaskId());
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto result(elements_.insert(std::make_pair(task_id, value)));
    if (!result.second)
      return false;
  }
  timer_.AddTask(timeout,
                 [this, task_id](std::string /*dummy_string*/) {
                   std::lock_guard<std::mutex> lock(this->mutex_);
                   this->elements_.erase(task_id);
                 }, 1, task_id);
  return true;
}

template <typename T, unsigned int DefaultTimeout>
boost::optional<T> TimedContainer<T, DefaultTimeout>::Find(
    const typename T::Key& key) const {
  boost::optional<T> element;

  std::lock_guard<std::mutex> lock(mutex_);
  std::for_each(std::begin(elements_), std::end(elements_),
                [key, &element](const std::pair<TaskId, T>& pair) {
                  if (pair.second.GetKey() == key)
                    element.reset(pair.second);
                });
  return element;
}

template <typename T, unsigned int DefaultTimeout>
void TimedContainer<T, DefaultTimeout>::Remove(const typename T::Key& key) {
  std::vector<TaskId> task_ids;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::for_each(std::begin(elements_), std::end(elements_),
                  [key, &task_ids](const std::pair<TaskId, T>& pair) {
                    if (pair.second.GetKey() == key)
                      task_ids.push_back(pair.first);
                  });
    for (const auto& task_id : task_ids)
      elements_.erase(task_id);
  }
  for (const auto& task_id : task_ids)
    timer_.CancelTask(task_id);
}

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TIMED_CONTAINER_H_

