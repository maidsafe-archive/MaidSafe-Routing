/* Copyright 2012 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

#include "maidsafe/routing/timer.h"

#include <algorithm>

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/error.h"
#include "maidsafe/common/log.h"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing.pb.h"

namespace maidsafe {

namespace routing {

Timer::Task::Task(const TaskId& id_in,
                  boost::asio::io_service& io_service,
                  const boost::posix_time::time_duration& timeout,
                  TaskResponseFunctor functor_in,
                  uint16_t expected_response_count_in)
    : id(id_in),
      timer(io_service, timeout),
      functor(functor_in),
      expected_response_count(expected_response_count_in) {}

Timer::Timer(AsioService& asio_service)
    : asio_service_(asio_service),
      task_id_(RandomInt32()),
      mutex_(),
      cond_var_(),
      tasks_() {}

Timer::~Timer() {
  CancelAllTask();
}

void Timer::CancelAllTask() {
  std::unique_lock<std::mutex> lock(mutex_);
  for (TaskPtr& task : tasks_)
    task->timer.cancel();
  cond_var_.wait(lock, [&] { return tasks_.empty(); });  // NOLINT (Fraser)
}

TaskId Timer::AddTask(const boost::posix_time::time_duration& timeout,
                      const TaskResponseFunctor& response_functor,
                      uint16_t expected_response_count) {
  std::lock_guard<std::mutex> lock(mutex_);
  TaskId task_id = ++task_id_;
  tasks_.push_back(TaskPtr(new Task(task_id, asio_service_.service(), timeout, response_functor,
                                    expected_response_count)));
  tasks_.back()->timer.async_wait([this, task_id](const boost::system::error_code& error) {
    ExecuteTask(task_id, error);
  });
  LOG(kInfo) << "AddTask added a task, with id " << task_id;
  return task_id;
}

std::vector<Timer::TaskPtr>::iterator Timer::FindTask(const TaskId& task_id) {
  return std::find_if(tasks_.begin(),
                      tasks_.end(),
                      [task_id](const TaskPtr& task) { return task->id == task_id; });  // NOLINT (Fraser)
}

void Timer::ExecuteTask(TaskId task_id, const boost::system::error_code& error) {
  TaskPtr task;
  uint16_t timed_out_response_count(0);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto itr(FindTask(task_id));
    if (itr == tasks_.end()) {
      LOG(kWarning) << "Task " << task_id << " not held by Timer.";
      return;
    }
    timed_out_response_count = (*itr)->expected_response_count;
    task = *itr;
    (*itr)->expected_response_count = 0;
  }

//  int return_code(kSuccess);
  switch (error.value()) {
    case boost::system::errc::success:
      // Task's timer has expired
      LOG(kError) << "Timed out waiting for task " << task->id;
      break;
    case boost::asio::error::operation_aborted:
//      assert(task->responses.size() <= task->expected_response_count);
      if (0 == task->expected_response_count) {
        // Cancelled via AddResponse
      } else {
        // Cancelled via CancelTask
        LOG(kInfo) << "Cancelled task " << task->id;
      }
      break;
    default:
//      return_code = kGeneralError;
      LOG(kError) << "Error waiting for task " << task->id << " - " << error.message();
  }

  asio_service_.service().dispatch([=] {
    for (uint16_t i(0); i != timed_out_response_count; ++i) {
      if (task->functor)
        task->functor(std::string());
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto itr(FindTask(task_id));
    if (itr != tasks_.end()) {
      tasks_.erase(itr);
      cond_var_.notify_one();
    }
  });
}

void Timer::CancelTask(TaskId task_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto itr(FindTask(task_id));
  if (itr == tasks_.end()) {
    LOG(kWarning) << "Task " << task_id << " not held by Timer.";
    return;
  }
  (*itr)->timer.cancel();
}

bool Timer::AddResponse(const protobuf::Message& response) {
  if (!response.has_id()) {
    LOG(kError) << "Received response with no ID.  Abort message handling.";
    return false;
  }
  std::shared_ptr<std::string> response_out(std::make_shared<std::string>(response.data(0)));
  TaskPtr task;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto itr(FindTask(response.id()));
    if (itr == tasks_.end() || ((*itr)->expected_response_count == 0)) {
      LOG(kWarning) << "Attempted to AddResponse to expired or non-existent task " << response.id();
      return false;
    }
    --((*itr)->expected_response_count);
    if (0 == (*itr)->expected_response_count) {
      LOG(kVerbose) << "Received response(s) for task " << response.id();
      (*itr)->timer.cancel();
    } else {
      LOG(kInfo) << "Received a response. Waiting for "
                 << (*itr)->expected_response_count
                 << " responses for task " << response.id();
    }
    task = *itr;
  }

  asio_service_.service().dispatch([=] {
      if (task->functor)
        task->functor(std::move(*response_out));
  });
  return true;
}

}  // namespace maidsafe

}  // namespace routing
