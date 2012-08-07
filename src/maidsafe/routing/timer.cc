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
                  int expected_count_in)
    : id(id_in),
      timer(io_service, timeout),
      functor(functor_in),
      responses(),
      expected_count(expected_count_in) {}

Timer::Timer(AsioService& asio_service)
    : asio_service_(asio_service),
      task_id_(RandomInt32()),
      mutex_(),
      tasks_() {}

TaskId Timer::AddTask(const boost::posix_time::time_duration& timeout,
                      const TaskResponseFunctor& response_functor,
                      int expected_count) {
  std::lock_guard<std::mutex> lock(mutex_);
  TaskId task_id = ++task_id_;
  tasks_.push_back(TaskPtr(new Task(task_id, asio_service_.service(), timeout, response_functor,
                                    expected_count)));
  tasks_.back()->timer.async_wait([this, task_id](const boost::system::error_code& error) {
    CancelTask(task_id, error);
  });
  LOG(kVerbose) << "AddTask added a task, with id " << task_id;
  return task_id;
}

// TODO(dirvine) we could change the find to iterate entire map if we want to be able to send
// multiple requests and accept the first one back, dropping the rest.
void Timer::CancelTask(TaskId task_id, const boost::system::error_code& error) {
  TaskPtr task;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto const itr = std::find_if(tasks_.begin(),
                                  tasks_.end(),
                                  [task_id](const TaskPtr& task) { return task->id == task_id; });  // NOLINT (Fraser)
    if (itr == tasks_.end()) {
      LOG(kWarning) << "Task " << task_id << " not held by Timer.";
      return;
    }
    // message timed out or task killed
    LOG(kVerbose) << "Timed out task " << task_id;
    task = *itr;
    tasks_.erase(itr);
  }

  if (!task->functor)
    return;

  int return_code(error == boost::asio::error::operation_aborted ? kResponseCancelled :
                                                                    kResponseTimeout);
  if (error && error != boost::asio::error::operation_aborted)
    LOG(kError) << "Error waiting for task " << task_id << " - " << error.message();
  asio_service_.service().dispatch([=] { task->functor(return_code, task->responses); });  // NOLINT (Fraser)
}

void Timer::ExecuteTask(protobuf::Message& message) {
  if (!message.has_id()) {
    LOG(kError) << "Received response with no ID.  Abort message handling.";
    return;
  }

  TaskPtr task;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto const itr =
        std::find_if(tasks_.begin(),
                    tasks_.end(),
                    [&message](const TaskPtr& task) { return task->id == message.id(); });  // NOLINT (Fraser)
    if (itr == tasks_.end()) {
      LOG(kWarning) << "Attempted to execute expired or non-existent task " << message.id();
      return;
    }
    task = *itr;

    task->responses.emplace_back(message.data(0));

    if (task->responses.size() >= task->expected_count) {
      LOG(kVerbose) << "Executing task " << message.id();
      tasks_.erase(itr);
    } else {
      LOG(kVerbose) << "Recieved " << task->responses.size() << " response(s). Waiting for "
                    << (task->expected_count - task->responses.size())
                    << " responses for Task id "
                    << message.id();
    }
  }

  if (!task->functor)
    return;

  if (task->responses.size() >= task->expected_count) {
    asio_service_.service().dispatch([=] { task->functor(kSuccess, task->responses); });  // NOLINT (Fraser)
  }
}

}  // namespace maidsafe

}  // namespace routing
