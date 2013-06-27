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
#include <mutex>
#include <cstdint>
#include <functional>
#include <future>
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

typedef std::function<void(std::string)> TaskResponseFunctor;
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
                 const TaskResponseFunctor& response_functor,
                 uint16_t expected_response_count);
  // Removes the task and invokes its functor with kResponseCancelled and whatever responses have
  // been added up to that point.
  void CancelTask(TaskId task_id);
  // Registers a response against the task indicated by response.id().  Once expected_response_count
  // responses have been added, the task is removed and its functor invoked with kSuccess.
  bool AddResponse(const protobuf::Message& response);
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
    uint16_t expected_response_count;
  };
  typedef std::shared_ptr<Task> TaskPtr;

  Timer& operator=(const Timer&);
  Timer(const Timer&);
  Timer(const Timer&&);
  std::vector<TaskPtr>::iterator FindTask(const TaskId& task_id);
  void ExecuteTask(TaskId task_id, const boost::system::error_code& error);
  void SetExceptions(std::vector<std::shared_ptr<std::promise<std::string>>> promises);

  AsioService& asio_service_;
  TaskId task_id_;
  std::mutex mutex_;
  std::condition_variable cond_var_;
  std::vector<TaskPtr> tasks_;
};

}  // namespace routing

}  // namespace maidsafe


#endif  // MAIDSAFE_ROUTING_TIMER_H_
