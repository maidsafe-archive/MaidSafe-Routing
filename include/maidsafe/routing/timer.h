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
#include <string>

#include "boost/asio/steady_timer.hpp"
#include "boost/asio/error.hpp"

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/error.h"
#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"

namespace maidsafe {

namespace routing {

namespace test {
class TimerTest;
}

typedef int32_t TaskId;

template <typename Response>
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
  void AddTask(const std::chrono::steady_clock::duration& timeout,
                 const ResponseFunctor& response_functor, int expected_response_count,
                 TaskId task_id);
  // Removes the task and invokes its functor once per "missing" expected Response, with a
  // default-constructed Response each time.  Throws if the indicated task doesn't exist.
  void CancelTask(TaskId task_id);
  // Invokes the response functor for the indicated task.  Throws if the indicated task doesn't
  // exist.
  void AddResponse(TaskId task_id, const Response& response);
  void CancelAll();

  TaskId NewTaskId();

  friend class test::TimerTest;

  std::string PrintTaskIds() {
    std::lock_guard<std::mutex> lock(mutex_);
  std::stringstream stream;
    stream << "This timer containing following tasks : ";
    for (auto& task : tasks_) {
      stream << "  task id  ---   " << task.first;
    }
    return stream.str();
  }

 private:
  struct Task {
    Task(boost::asio::io_service& io_service, const std::chrono::steady_clock::duration& timeout,
         ResponseFunctor functor_in, int expected_response_count);
    Task(Task&& other);
    Task& operator=(Task&& other);

    std::unique_ptr<boost::asio::steady_timer> timer;
    ResponseFunctor functor;
    int outstanding_response_count;

   private:
    Task() = delete;
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
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
template <typename Response>
Timer<Response>::Task::Task(boost::asio::io_service& io_service,
                            const std::chrono::steady_clock::duration& timeout,
                            ResponseFunctor functor_in, int expected_response_count)
    : timer(new boost::asio::steady_timer(io_service, timeout)),
      functor(std::move(functor_in)),
      outstanding_response_count(expected_response_count) {}

template <typename Response>
Timer<Response>::Task::Task(Task&& other)
    : timer(std::move(other.timer)),
      functor(std::move(other.functor)),
      outstanding_response_count(std::move(other.outstanding_response_count)) {}

template <typename Response>
typename Timer<Response>::Task& Timer<Response>::Task::operator=(Task&& other) {
  timer = std::move(other.timer);
  functor = std::move(other.functor);
  outstanding_response_count = std::move(other.outstanding_response_count);
  return *this;
}

template <typename Response>
Timer<Response>::Timer(AsioService& asio_service)
    : asio_service_(asio_service), new_task_id_(RandomInt32()), mutex_(), cond_var_(), tasks_() {}

template <typename Response>
Timer<Response>::~Timer() {
  CancelAll();
}

template <typename Response>
void Timer<Response>::CancelAll() {
  std::unique_lock<std::mutex> lock(mutex_);
  for (const auto& task : tasks_)
    task.second.timer->cancel();
  cond_var_.wait(lock, [&] { return tasks_.empty(); });
}


template <typename Response>
void Timer<Response>::AddTask(const std::chrono::steady_clock::duration& timeout,
                              const ResponseFunctor& response_functor,
                              int expected_response_count, TaskId task_id) {
  if (!response_functor || expected_response_count < 1) {
    LOG(kError) << "Timer<Response>::AddTask response_functor not initialised or "
                << " incorrect expected_response_count";
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::invalid_parameter));
  }
  std::lock_guard<std::mutex> lock(mutex_);
  auto result(tasks_.insert(std::move(
      std::make_pair(task_id, std::move(Task(asio_service_.service(), timeout, response_functor,
                                             expected_response_count))))));
  assert(result.second);
  result.first->second.timer->async_wait([this, task_id](const boost::system::error_code & error) {
    this->FinishTask(task_id, error);
  });
}

template <typename Response>
void Timer<Response>::FinishTask(TaskId task_id, const boost::system::error_code& error) {
  int outstanding_response_count(0);
  ResponseFunctor functor;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto itr(tasks_.find(task_id));
    if (itr == std::end(tasks_)) {
      LOG(kError) << "Timer<Response>::FinishTask Task " << task_id << " not held by Timer.";
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::invalid_parameter));
    }
    assert(itr->second.outstanding_response_count >= 0);
    if (itr->second.outstanding_response_count != 0) {
      outstanding_response_count = itr->second.outstanding_response_count;
      functor = itr->second.functor;
    }

    tasks_.erase(itr);

    switch (error.value()) {
      case boost::system::errc::success:  // Task's timer has expired
        LOG(kWarning) << "Timed out waiting for task " << task_id;
        break;
      case boost::asio::error::operation_aborted:  // Cancelled via CancelTask
        break;
      default:
        LOG(kError) << "Error waiting for task " << task_id << " - " << error.message();
    }
  }
  for (int i(0); i != outstanding_response_count; ++i)
    asio_service_.service().dispatch([=] { functor(Response()); });
  cond_var_.notify_one();
}

template <typename Response>
void Timer<Response>::CancelTask(TaskId task_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto itr(tasks_.find(task_id));
  if (itr == std::end(tasks_)) {
    LOG(kError) << "Task " << task_id << " not held by Timer.";
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::invalid_parameter));
  }
  itr->second.timer->cancel();
}

template <typename Response>
void Timer<Response>::AddResponse(TaskId task_id, const Response& response) {
  ResponseFunctor functor;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto itr(tasks_.find(task_id));
    if (itr == std::end(tasks_)) {
      LOG(kError) << "Task " << task_id << " not held by Timer.";
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::no_such_element));
    }
    if (itr->second.outstanding_response_count == 0) {
      LOG(kError) << "outstanding_response_count already reached zero";
      return;
    }
    --(itr->second.outstanding_response_count);
    functor = itr->second.functor;
    if (itr->second.outstanding_response_count == 0)
      itr->second.timer->cancel();  // Invokes 'FinishTask'
  }
  asio_service_.service().dispatch([=] { functor(response); });
}

template <typename Response>
TaskId Timer<Response>::NewTaskId() {
  std::lock_guard<std::mutex> lock(mutex_);
  return new_task_id_++;
}


}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TIMER_H_
