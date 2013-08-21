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

#ifndef MAIDSAFE_ROUTING_TIMER_H_
#define MAIDSAFE_ROUTING_TIMER_H_

#include <condition_variable>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>

#include "boost/asio/steady_timer.hpp"
#include "boost/asio/error.hpp"

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/error.h"
#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"


namespace maidsafe {

namespace routing {

namespace test { class TimerTest; }

typedef int32_t TaskId;

template<typename Response>
class Timer {
 public:
  typedef std::function<void(Response)> ResponseFunctor;
  explicit Timer(AsioService& asio_service);
  // Cancels all tasks and blocks until all functors have been executed and all tasks removed.
  ~Timer();
  // Adds a task with a deadline, and returns a unique ID for the task.  'response_functor' will be
  // invoked every time 'AddResponse' is called for that task, up to 'expected_response_count'
  // times.  At the point of timeout, any shortfall in response count will cause 'response_functor'
  // to be invoked the appropriate number of times with a default-constructed Response.  Throws if
  // 'response_functor' is null or if 'expected_response_count' < 1.
  TaskId AddTask(const std::chrono::steady_clock::duration& timeout,
                 const ResponseFunctor& response_functor,
                 int expected_response_count);
  // Removes the task and invokes its functor once per "missing" expected Response, with a
  // default-constructed Response each time.  Throws if the indicated task doesn't exist.
  void CancelTask(TaskId task_id);
  // Invokes the response functor for the indicated task.  Throws if the indicated task doesn't
  // exist.
  void AddResponse(TaskId task_id, const Response& response);

  friend class test::TimerTest;

 private:
  struct Task {
    Task(boost::asio::io_service& io_service,
         const std::chrono::steady_clock::duration& timeout,
         ResponseFunctor functor_in,
         int expected_response_count);
    Task(Task&& other);
    Task& operator=(Task&& other);

    std::unique_ptr<boost::asio::steady_timer> timer;
    ResponseFunctor functor;
    int outstanding_response_count;

   private:
    Task() MAIDSAFE_DELETE;
    Task(const Task&) MAIDSAFE_DELETE;
    Task& operator=(const Task&) MAIDSAFE_DELETE;
  };

  Timer(const Timer&);
  Timer(const Timer&&);
  Timer& operator=(Timer);

  void FinishTask(TaskId task_id, const boost::system::error_code& error);

  AsioService& asio_service_;
  TaskId new_task_id_;
  std::mutex mutex_;
  std::condition_variable cond_var_;
  std::map<TaskId, Task> tasks_;
};



// ==================== Implementation =============================================================
template<typename Response>
Timer<Response>::Task::Task(boost::asio::io_service& io_service,
                            const std::chrono::steady_clock::duration& timeout,
                            ResponseFunctor functor_in,
                            int expected_response_count)
    : timer(new boost::asio::steady_timer(io_service, timeout)),
      functor(functor_in),
      outstanding_response_count(expected_response_count) {}

template<typename Response>
Timer<Response>::Task::Task(Task&& other)
    : timer(std::move(other.timer)),
      functor(std::move(other.functor)),
      outstanding_response_count(std::move(other.outstanding_response_count)) {}

template<typename Response>
typename Timer<Response>::Task& Timer<Response>::Task::operator=(Task&& other) {
  timer = std::move(other.timer);
  functor = std::move(other.functor);
  outstanding_response_count = std::move(other.outstanding_response_count);
  return *this;
}

template<typename Response>
Timer<Response>::Timer(AsioService& asio_service)
    : asio_service_(asio_service),
      new_task_id_(RandomInt32()),
      mutex_(),
      cond_var_(),
      tasks_() {}

template<typename Response>
Timer<Response>::~Timer() {
  std::unique_lock<std::mutex> lock(mutex_);
  for (const auto& task : tasks_)
    task.second.timer->cancel();
  cond_var_.wait(lock, [&] { return tasks_.empty(); });
}

template<typename Response>
TaskId Timer<Response>::AddTask(const std::chrono::steady_clock::duration& timeout,
                                const ResponseFunctor& response_functor,
                                int expected_response_count) {
  if (!response_functor || expected_response_count < 1)
    ThrowError(CommonErrors::invalid_parameter);
  std::lock_guard<std::mutex> lock(mutex_);
  TaskId task_id(++new_task_id_);
  auto result(tasks_.insert(std::move(std::make_pair(task_id,
      std::move(Task(asio_service_.service(),
                     timeout,
                     response_functor,
                     expected_response_count))))));
  assert(result.second);
  result.first->second.timer->async_wait([this, task_id](const boost::system::error_code& error) {
    this->FinishTask(task_id, error);
  });
  return task_id;
}

template<typename Response>
void Timer<Response>::FinishTask(TaskId task_id, const boost::system::error_code& error) {
  int outstanding_response_count(0);
  ResponseFunctor functor;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto itr(tasks_.find(task_id));
    if (itr == std::end(tasks_)) {
      LOG(kError) << "Task " << task_id << " not held by Timer.";
      ThrowError(CommonErrors::invalid_parameter);
    }
    assert(itr->second.outstanding_response_count >= 0);

    if (itr->second.outstanding_response_count != 0) {
      outstanding_response_count = itr->second.outstanding_response_count;
      functor = itr->second.functor;
    }

    tasks_.erase(itr);
  }

  cond_var_.notify_one();

  switch (error.value()) {
    case boost::system::errc::success:  // Task's timer has expired
      LOG(kWarning) << "Timed out waiting for task " << task_id;
      break;
    case boost::asio::error::operation_aborted:  // Cancelled via CancelTask
      LOG(kInfo) << "Cancelled task " << task_id;
      break;
    default:
      LOG(kError) << "Error waiting for task " << task_id << " - " << error.message();
  }

  for (int i(0); i != outstanding_response_count; ++i)
    asio_service_.service().dispatch([=] { functor(Response()); });
}

template<typename Response>
void Timer<Response>::CancelTask(TaskId task_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto itr(tasks_.find(task_id));
  if (itr == std::end(tasks_)) {
    LOG(kError) << "Task " << task_id << " not held by Timer.";
    ThrowError(CommonErrors::invalid_parameter);
  }
  itr->second.timer->cancel();
}

template<typename Response>
void Timer<Response>::AddResponse(TaskId task_id, const Response& response) {
  ResponseFunctor functor;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto itr(tasks_.find(task_id));
    if (itr == std::end(tasks_)) {
      LOG(kError) << "Task " << task_id << " not held by Timer.";
      ThrowError(CommonErrors::invalid_parameter);
    }
    assert(itr->second.outstanding_response_count > 0);
    --(itr->second.outstanding_response_count);
    if (itr->second.outstanding_response_count == 0)
      itr->second.timer->cancel();  // Invokes 'FinishTask'
    functor = itr->second.functor;
  }
  asio_service_.service().dispatch([=] { functor(response); });
}

}  // namespace routing

}  // namespace maidsafe


#endif  // MAIDSAFE_ROUTING_TIMER_H_
