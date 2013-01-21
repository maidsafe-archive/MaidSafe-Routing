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

#include "maidsafe/routing/timer.h"

#include <algorithm>

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/log.h"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_pb.h"

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
      responses(),
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
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto itr(FindTask(task_id));
    if (itr == tasks_.end()) {
      LOG(kWarning) << "Task " << task_id << " not held by Timer.";
      return;
    }
    task = *itr;
  }

//  int return_code(kSuccess);
  switch (error.value()) {
    case boost::system::errc::success:
      // Task's timer has expired
//      return_code = kResponseTimeout;
      LOG(kError) << "Timed out waiting for task " << task->id;
      break;
    case boost::asio::error::operation_aborted:
      assert(task->responses.size() <= task->expected_response_count);
      if (task->responses.size() == task->expected_response_count) {
        // Cancelled via AddResponse
//        return_code = kSuccess;
      } else {
        // Cancelled via CancelTask
//        return_code = kResponseCancelled;
        LOG(kInfo) << "Cancelled task " << task->id;
      }
      break;
    default:
//      return_code = kGeneralError;
      LOG(kError) << "Error waiting for task " << task->id << " - " << error.message();
  }

  asio_service_.service().dispatch([=] {
    if (task->functor)
      task->functor(std::move(task->responses));
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

  std::lock_guard<std::mutex> lock(mutex_);
  auto itr(FindTask(response.id()));
  if (itr == tasks_.end()) {
    LOG(kWarning) << "Attempted to AddResponse to expired or non-existent task " << response.id();
    return false;
  }

  (*itr)->responses.emplace_back(response.data(0));

  if ((*itr)->responses.size() == (*itr)->expected_response_count) {
    LOG(kVerbose) << "Executing task " << response.id();
    (*itr)->timer.cancel();
  } else {
    LOG(kInfo) << "Received " << (*itr)->responses.size() << " response(s). Waiting for "
               << ((*itr)->expected_response_count - (*itr)->responses.size())
               << " responses for task " << response.id();
  }
  return true;
}

}  // namespace maidsafe

}  // namespace routing
