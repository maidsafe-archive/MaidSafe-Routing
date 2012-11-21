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

#ifndef MAIDSAFE_ROUTING_TIMER_H_
#define MAIDSAFE_ROUTING_TIMER_H_

#include <condition_variable>
#include <mutex>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "boost/asio/deadline_timer.hpp"
#include "boost/asio/error.hpp"
#include "boost/date_time/posix_time/posix_time_config.hpp"

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/utils.h"


namespace maidsafe {

namespace routing {

namespace protobuf { class Message; }

typedef std::function<void(std::vector<std::string>)> TaskResponseFunctor;
typedef int32_t TaskId;

class Timer {
 public:
  explicit Timer(AsioService& asio_service);
  // Cancels all tasks and blocks until all functors have been executed and all tasks removed.
  ~Timer();
  // Adds a task with a deadline, and returns a unique ID for the task.  If expected_response_count
  // responses are added before timeout duration elapses, response_functor is invoked with kSuccess.
  // If the task is cancelled, times out, or the wait fails in some other way, the response_functor
  // is invoked with an appropriate error code.
  TaskId AddTask(const boost::posix_time::time_duration& timeout,
                 TaskResponseFunctor response_functor,
                 uint16_t expected_response_count);
  // Removes the task and invokes its functor with kResponseCancelled and whatever responses have
  // been added up to that point.
  void CancelTask(TaskId task_id);
  // Registers a response against the task indicated by response.id().  Once expected_response_count
  // responses have been added, the task is removed and its functor invoked with kSuccess.
  void AddResponse(const protobuf::Message& response);
  // Cancels all tasks and blocks until all functors have been executed and all tasks removed.
  void CancelAllTask();

 private:
  struct Task {
    Task(const TaskId& id_in,
         boost::asio::io_service& io_service,
         const boost::posix_time::time_duration& timeout,
         TaskResponseFunctor functor_in,
         uint16_t expected_response_count_in);
    TaskId id;
    boost::asio::deadline_timer timer;
    TaskResponseFunctor functor;
    std::vector<std::string> responses;
    uint16_t expected_response_count;
  };
  typedef std::shared_ptr<Task> TaskPtr;

  Timer& operator=(const Timer&);
  Timer(const Timer&);
  Timer(const Timer&&);
  std::vector<TaskPtr>::iterator FindTask(const TaskId& task_id);
  void ExecuteTask(TaskId task_id, const boost::system::error_code& error);

  AsioService& asio_service_;
  TaskId task_id_;
  std::mutex mutex_;
  std::condition_variable cond_var_;
  std::vector<TaskPtr> tasks_;
};

}  // namespace routing

}  // namespace maidsafe


#endif  // MAIDSAFE_ROUTING_TIMER_H_
