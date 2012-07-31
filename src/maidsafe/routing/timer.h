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
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "boost/asio/deadline_timer.hpp"
#include "boost/date_time/posix_time/posix_time_config.hpp"

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/utils.h"


namespace maidsafe {

namespace routing {

namespace protobuf { class Message; }

typedef std::function<void(int, std::vector<std::string>)> TaskResponseFunctor;
typedef uint32_t TaskId;

class Timer {
 public:
  explicit Timer(AsioService& io_service);
  TaskId AddTask(const boost::posix_time::time_duration& timeout,
                 const TaskResponseFunctor& response_functor);
  // Removes task from queue immediately without executing it.
  void KillTask(uint32_t task_id);
  // Executes and removes task.
  void ExecuteTask(protobuf::Message& message);

 private:
  typedef std::shared_ptr<boost::asio::deadline_timer> TimerPtr;
  Timer& operator=(const Timer&);
  Timer(const Timer&);
  Timer(const Timer&&);
  AsioService& io_service_;
  TaskId task_id_;
  std::mutex mutex_;
  std::map<uint32_t, std::pair<TimerPtr, TaskResponseFunctor>> queue_;
};

}  // namespace routing

}  // namespace maidsafe


#endif  // MAIDSAFE_ROUTING_TIMER_H_
