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

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iterator>
#include <map>
#include <mutex>
#include <string>

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/error.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/timer.h"

namespace maidsafe {

namespace routing {

namespace test {

class TimerTest : public testing::Test {
 public:
  TimerTest()
      : asio_service_(8),
        timer_(asio_service_),
        cond_var_(),
        mutex_(),
        kGroupSize_((RandomUint32() % 97) + 4),
        pass_response_functor_([=](std::string response) {
          {
            std::lock_guard<std::mutex> lock(mutex_);
            ++pass_response_count_;
          }
          ASSERT_FALSE(response.empty());
          cond_var_.notify_one();
        }),
        failed_response_functor_([=](std::string response) {
          {
            std::lock_guard<std::mutex> lock(mutex_);
            ++failed_response_count_;
          }
          ASSERT_TRUE(response.empty());
          cond_var_.notify_one();
        }),
        variable_response_functor_([=](std::string response) {
          {
            std::lock_guard<std::mutex> lock(mutex_);
            response.empty() ? ++failed_response_count_ : ++pass_response_count_;
          }
          cond_var_.notify_one();
        }),
        message_(RandomAlphaNumericString(30)),
        pass_response_count_(0),
        failed_response_count_(0) {}

  void TearDown() override {
    asio_service_.Stop();
    EXPECT_TRUE(timer_.tasks_.empty());
  }

 protected:
  typedef Timer<std::string>::ResponseFunctor TaskResponseFunctor;
  AsioService asio_service_;
  Timer<std::string> timer_;
  std::condition_variable cond_var_;
  std::mutex mutex_;
  const uint32_t kGroupSize_;
  TaskResponseFunctor pass_response_functor_, failed_response_functor_, variable_response_functor_;
  std::string message_;
  uint32_t pass_response_count_, failed_response_count_;
};


TEST_F(TimerTest, BEH_InvalidParameters) {
  EXPECT_THROW(timer_.AddTask(std::chrono::seconds(1), nullptr, 1, timer_.NewTaskId()),
               maidsafe_error);
  EXPECT_THROW(
      timer_.AddTask(std::chrono::seconds(1), pass_response_functor_, 0, timer_.NewTaskId()),
      maidsafe_error);
  EXPECT_THROW(
      timer_.AddTask(std::chrono::seconds(1), pass_response_functor_, -1, timer_.NewTaskId()),
      maidsafe_error);
  auto task_id(timer_.NewTaskId());
  timer_.AddTask(std::chrono::milliseconds(100), failed_response_functor_, 1, task_id);
  EXPECT_THROW(timer_.CancelTask(task_id + 1), maidsafe_error);
  EXPECT_THROW(timer_.AddResponse(task_id + 1, message_), maidsafe_error);
  std::unique_lock<std::mutex> lock(mutex_);
  cond_var_.wait_for(lock, std::chrono::milliseconds(200),
                     [&] { return failed_response_count_ == 1U; });
}

TEST_F(TimerTest, BEH_SingleResponse) {
  auto task_id(timer_.NewTaskId());
  timer_.AddTask(std::chrono::seconds(2), pass_response_functor_, 1, task_id);
  timer_.AddResponse(task_id, message_);
  std::unique_lock<std::mutex> lock(mutex_);
  EXPECT_TRUE(cond_var_.wait_for(lock, std::chrono::seconds(10),
                                 [&] { return pass_response_count_ == 1U; }));
}

TEST_F(TimerTest, BEH_SingleResponseTimedOut) {
  timer_.AddTask(std::chrono::milliseconds(100), failed_response_functor_, 1, timer_.NewTaskId());
  std::unique_lock<std::mutex> lock(mutex_);
  EXPECT_TRUE(cond_var_.wait_for(lock, std::chrono::milliseconds(200),
                                 [&] { return failed_response_count_ == 1U; }));
}

TEST_F(TimerTest, BEH_SingleResponseWithMoreChecks) {
  pass_response_functor_ = [=](std::string response) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      ++pass_response_count_;
    }
    ASSERT_EQ(response, message_);
    cond_var_.notify_one();
  };
  auto task_id(timer_.NewTaskId());
  timer_.AddTask(std::chrono::seconds(2), pass_response_functor_, 1, task_id);
  timer_.AddResponse(task_id, message_);
  std::unique_lock<std::mutex> lock(mutex_);
  EXPECT_TRUE(cond_var_.wait_for(lock, std::chrono::seconds(10),
                                 [&] { return pass_response_count_ == 1U; }));
}

TEST_F(TimerTest, BEH_GroupResponse) {
  auto task_id(timer_.NewTaskId());
  timer_.AddTask(std::chrono::seconds(2), pass_response_functor_, kGroupSize_, task_id);
  for (uint32_t i(0); i != kGroupSize_; ++i)
    timer_.AddResponse(task_id, message_);
  std::unique_lock<std::mutex> lock(mutex_);
  EXPECT_TRUE(cond_var_.wait_for(lock, std::chrono::seconds(10),
                                 [&] { return pass_response_count_ == kGroupSize_; }));
}

TEST_F(TimerTest, BEH_GroupResponsePartialResult) {
  auto task_id(timer_.NewTaskId());
  timer_.AddTask(std::chrono::milliseconds(100), variable_response_functor_, kGroupSize_, task_id);
  for (uint32_t i(0); i != kGroupSize_ - 1; ++i)
    timer_.AddResponse(task_id, message_);
  std::unique_lock<std::mutex> lock(mutex_);
  EXPECT_TRUE(cond_var_.wait_for(lock, std::chrono::milliseconds(200), [&] {
    return pass_response_count_ + failed_response_count_ == kGroupSize_;
  }));
  EXPECT_EQ(pass_response_count_, kGroupSize_ - 1);
  EXPECT_EQ(failed_response_count_, 1);
}

TEST_F(TimerTest, BEH_MultipleResponse) {
  const uint32_t kMessageCount((RandomUint32() % 1000) + 500);

  auto add_tasks = [&](int number) -> std::map<TaskId, std::string> {
    std::map<TaskId, std::string> messages;
    for (int i(0); i != number; ++i) {
      auto task_id(timer_.NewTaskId());
      timer_.AddTask(std::chrono::seconds(10), pass_response_functor_, 1, task_id);
      messages.insert(std::make_pair(task_id, RandomAlphaNumericString(30)));
    }
    return messages;
  };

  auto add_response = [&](std::map<TaskId, std::string> messages) {
    for (const auto& message : messages)
      timer_.AddResponse(message.first, message.second);
  };

  auto add_tasks1_future = std::async(std::launch::async, add_tasks, kMessageCount / 2);
  auto add_tasks2_future =
      std::async(std::launch::async, add_tasks, kMessageCount / 2 + kMessageCount % 2);

  auto add_response1_future = std::async(std::launch::async, add_response, add_tasks1_future.get());
  auto add_response2_future = std::async(std::launch::async, add_response, add_tasks2_future.get());
  add_response1_future.get();
  add_response2_future.get();

  std::unique_lock<std::mutex> lock(mutex_);
  EXPECT_TRUE(cond_var_.wait_for(lock, std::chrono::seconds(10),
                                 [&] { return pass_response_count_ == kMessageCount; }));
}

TEST_F(TimerTest, BEH_MultipleGroupResponse) {
  const uint32_t kMessageCount((RandomUint32() % 100) + 500);

  auto add_tasks = [&](int number) -> std::map<TaskId, std::string> {
    std::map<TaskId, std::string> messages;
    TaskId task_id;
    for (int i(0); i != number; ++i) {
      task_id = timer_.NewTaskId();
      timer_.AddTask(std::chrono::seconds(40), pass_response_functor_, kGroupSize_, task_id);
      messages.insert(std::make_pair(task_id, RandomAlphaNumericString(30)));
    }
    return messages;
  };

  auto add_response = [&](std::map<TaskId, std::string> messages) {
    for (const auto& message : messages) {
      for (uint32_t i(0); i != kGroupSize_; ++i)
        timer_.AddResponse(message.first, message.second);
    }
  };

  auto add_tasks1_future = std::async(std::launch::async, add_tasks, kMessageCount / 2);
  auto add_tasks2_future =
      std::async(std::launch::async, add_tasks, kMessageCount / 2 + kMessageCount % 2);

  auto add_response1_future = std::async(std::launch::async, add_response, add_tasks1_future.get());
  auto add_response2_future = std::async(std::launch::async, add_response, add_tasks2_future.get());
  add_response1_future.get();
  add_response2_future.get();

  std::unique_lock<std::mutex> lock(mutex_);
  EXPECT_TRUE(cond_var_.wait_for(lock, std::chrono::seconds(10), [&] {
    return pass_response_count_ == kMessageCount * kGroupSize_;
  }));
}

TEST_F(TimerTest, BEH_CancelTask) {
  auto task_id(timer_.NewTaskId());
  timer_.AddTask(std::chrono::seconds(10), variable_response_functor_, kGroupSize_, task_id);
  timer_.AddResponse(task_id, message_);
  timer_.CancelTask(task_id);
  std::unique_lock<std::mutex> lock(mutex_);
  EXPECT_TRUE(cond_var_.wait_for(lock, std::chrono::milliseconds(200), [&] {
    return pass_response_count_ + failed_response_count_ == kGroupSize_;
  }));
  EXPECT_EQ(pass_response_count_, 1);
  EXPECT_EQ(failed_response_count_, kGroupSize_ - 1);
}

struct MessageDetails {
  MessageDetails()
      : message(RandomAlphaNumericString(30)),
        expected_success_count(RandomUint32() % 5),
        success_count(0),
        expected_failure_count(RandomUint32() % 5),
        failure_count(0),
        task_id(0),
        timeout((RandomUint32() % 1000) + 2000) {
    if (expected_success_count == 0 && expected_failure_count == 0)
      ++expected_success_count;
  }
  std::string message;
  uint32_t expected_success_count, success_count, expected_failure_count, failure_count;
  TaskId task_id;
  std::chrono::milliseconds timeout;
};

TEST_F(TimerTest, BEH_VariousResults) {
  const uint32_t kMessageCount((RandomUint32() % 100) + 500);

  std::map<uint32_t, MessageDetails> messages_details;
  uint32_t total_expected_functor_calls(0), functor_calls(0);
  for (uint32_t i(0); i != kMessageCount; ++i) {
    MessageDetails details;
    uint32_t expected_count(details.expected_success_count + details.expected_failure_count);
    total_expected_functor_calls += expected_count;
    TaskResponseFunctor functor([&messages_details, &functor_calls, i, this](std::string response) {
      std::lock_guard<std::mutex> lock(mutex_);
      auto itr(messages_details.find(i));
      ASSERT_NE(itr, std::end(messages_details));
      if (response.empty()) {
        ++itr->second.failure_count;
      } else {
        ASSERT_EQ(itr->second.message, response);
        ++itr->second.success_count;
      }
      ++functor_calls;
      cond_var_.notify_one();
    });
    details.task_id = timer_.NewTaskId();
    timer_.AddTask(details.timeout, functor, expected_count, details.task_id);
    messages_details.insert(std::make_pair(i, details));
  }

  for (const auto& details : messages_details) {
    for (uint32_t i(0); i != details.second.expected_success_count; ++i) {
      auto task_id(details.second.task_id);
      auto message(details.second.message);
      asio_service_.service().post(
          [this, task_id, message]() { timer_.AddResponse(task_id, message); });
    }
  }

  std::unique_lock<std::mutex> lock(mutex_);
  cond_var_.wait_for(lock, std::chrono::seconds(5),
                     [&] { return total_expected_functor_calls == functor_calls; });

  for (const auto& details : messages_details) {
    EXPECT_EQ(details.second.expected_success_count, details.second.success_count);
    EXPECT_EQ(details.second.expected_failure_count, details.second.failure_count);
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
