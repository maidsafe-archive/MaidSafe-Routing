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

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/log.h"

#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_pb.h"

namespace maidsafe {

namespace routing {

maidsafe::routing::Timer::Timer(AsioService& io_service)
    : io_service_(io_service),
      task_id_(RandomUint32()),
      mutex_(),
      queue_() {}

TaskId Timer::AddTask(const boost::posix_time::time_duration& timeout,
                      const TaskResponseFunctor& response_functor) {
  TimerPtr timer(new boost::asio::deadline_timer(io_service_.service(), timeout));
  std::lock_guard<std::mutex> lock(mutex_);
  ++task_id_;
  LOG(kVerbose) << "AddTask added a task, with id : " << task_id_;
  queue_.insert(std::make_pair(task_id_, std::make_pair(timer, response_functor)));
  timer->async_wait(std::bind(&Timer::KillTask, this, task_id_));
  return task_id_;
}

// TODO(dirvine) we could change the find to iterate entire map if we want to be able to send
// multiple requests and accept the first one back, dropping the rest.
void Timer::KillTask(TaskId task_id) {
  TaskResponseFunctor task_response_functor;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto const it = queue_.find(task_id);
    if (it != queue_.end()) {
      // message timed out or task killed
      LOG(kVerbose) << "Killed task " << task_id;
      task_response_functor = (*it).second.second;
      queue_.erase(it);
    }
  }

  if (task_response_functor) {
    io_service_.service().dispatch([=] {
        task_response_functor(kResponseTimeout, std::vector<std::string>());
    });
  }
}

void Timer::ExecuteTask(protobuf::Message& message) {
  if (!message.has_id()) {
    LOG(kError) << "Received response with no ID.  Abort message handling.";
    return;
  }

  TaskResponseFunctor task_response_functor(nullptr);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto const it = queue_.find(message.id());
    if (it != queue_.end()) {
      task_response_functor = (*it).second.second;
      queue_.erase(it);
      LOG(kVerbose) << "Executing task " << message.id();
    } else {
      LOG(kError) << "Attempted to execute expired or non-existent task " << message.id();
    }
  }

  if (task_response_functor) {
    std::vector<std::string> data_vector;
    for (int index(0); index < message.data_size(); ++index)
      data_vector.emplace_back(message.data(index));
    io_service_.service().dispatch([=] { task_response_functor(kSuccess, data_vector); });
  }
}

}  // namespace maidsafe

}  // namespace routing
