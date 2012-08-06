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

typedef std::function<void(int, std::vector<std::string>)> TaskResponseFunctor;
typedef int32_t TaskId;

class Timer {
 public:
  explicit Timer(AsioService& asio_service);
  TaskId AddTask(const boost::posix_time::time_duration& timeout,
                 const TaskResponseFunctor& response_functor);
  // Executes task with return code kResponseTimeout or kResponseCancelled and removes task.
  void CancelTask(TaskId task_id,
                  const boost::system::error_code& error = boost::asio::error::operation_aborted);
  // Executes task with return code kSuccess and removes task.
  void ExecuteTask(protobuf::Message& message);

 private:
  struct Task {
    Task(const TaskId& id_in,
         boost::asio::io_service& io_service,
         const boost::posix_time::time_duration& timeout,
         TaskResponseFunctor functor_in);
    TaskId id;
    boost::asio::deadline_timer timer;
    TaskResponseFunctor functor;
    std::vector<std::string> responses;
  };
  typedef std::shared_ptr<Task> TaskPtr;

  Timer& operator=(const Timer&);
  Timer(const Timer&);
  Timer(const Timer&&);

  AsioService& asio_service_;
  TaskId task_id_;
  std::mutex mutex_;
  std::vector<TaskPtr> tasks_;
};

}  // namespace routing

}  // namespace maidsafe


#endif  // MAIDSAFE_ROUTING_TIMER_H_
