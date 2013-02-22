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

#include <algorithm>
#include <memory>
#include <vector>
#include <set>

#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/progress.hpp"

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/log.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/timer.h"


namespace bptime = boost::posix_time;

namespace maidsafe {

namespace routing {

namespace test {

class TimerTest : public testing::Test {
 public:
  TimerTest()
      : asio_service_(2),
        timer_(asio_service_),
        kGroupSize_((RandomUint32() % 97) + 4),
        pass_response_functor_(),
        failed_response_functor_(),
        group_failed_response_functor_(),
        message_(),
        pass_response_count_(0),
        failed_response_count_(0) {
    asio_service_.Start();

    pass_response_functor_ = [=](std::string response) {
      ++pass_response_count_;
      ASSERT_FALSE(response.empty());
    };
    failed_response_functor_ = [=](std::string response) {
      ASSERT_TRUE(response.empty());
      ++failed_response_count_;
    };
    group_failed_response_functor_ = [=](std::string response) {
      if (response.empty())
        ++failed_response_count_;
      else
        ++pass_response_count_;
    };

    message_.set_type(-200);
    message_.set_destination_id("destination_id");
    message_.set_direct(false);
    message_.add_data("response data");
    message_.set_source_id("source_id");
  }

    protobuf::Message  CreateMessage(const int id) {
    protobuf::Message message;
    message.set_type(-200);
    message.set_destination_id("destination_id");
    message.set_direct(false);
    message.add_data(RandomAlphaNumericString(1024 * 512)/*"response data"*/);
    message.set_source_id("source_id");
    message.set_id(id);
    return message;
  }

 protected:
  AsioService asio_service_;
  Timer timer_;
  const uint16_t kGroupSize_;
  TaskResponseFunctor pass_response_functor_, failed_response_functor_;
  TaskResponseFunctor group_failed_response_functor_;
  protobuf::Message message_;
  std::atomic<int> pass_response_count_;
  std::atomic<int> failed_response_count_;
};

TEST_F(TimerTest, BEH_SingleResponse) {
  message_.set_id(timer_.AddTask(bptime::seconds(2),
                                 pass_response_functor_, 1));
  timer_.AddResponse(message_);
  while (pass_response_count_ != 1U);
}

TEST_F(TimerTest, BEH_SingleResponseWithMoreChecks) {
  std::string in_response = RandomAlphaNumericString(1024 * 512);
  std::atomic<int> count(0);
  TaskResponseFunctor response_functor = [&count, in_response](std::string response) {
    ++count;
    EXPECT_FALSE(response.empty());
    EXPECT_EQ(in_response, response);
  };
  message_.clear_data();
  message_.add_data(in_response);
  message_.set_id(timer_.AddTask(bptime::seconds(2),
                                 response_functor, 1));
  timer_.AddResponse(message_);
  while (count != 1U);
}

TEST_F(TimerTest, BEH_MultipleResponse) {
  const int kMessageCount(400);
  boost::progress_timer t;
  std::atomic<int> count(0);
  TaskResponseFunctor response_functor = [&count](std::string response) {
    ++count;
    EXPECT_FALSE(response.empty());
  };

  auto add_tasks = [&](const int& number)->std::vector<protobuf::Message> {
      std::vector<protobuf::Message> messages;
      for (int i(0); i != number; ++i)
        messages.push_back(CreateMessage(timer_.AddTask(bptime::seconds(80),
                                                        response_functor, 1)));
      return messages;
    };

  auto add_response = [&](std::vector<protobuf::Message>&& messages) {
      for (size_t i(0); i != messages.size(); ++i)
        timer_.AddResponse(messages.at(i));
    };

  auto add_tasks1_future = std::async(std::launch::async, add_tasks, kMessageCount/2);
  auto add_tasks2_future = std::async(std::launch::async, add_tasks, kMessageCount/2);

  auto add_response1_future = std::async(std::launch::async, add_response, add_tasks1_future.get());
  auto add_response2_future = std::async(std::launch::async, add_response, add_tasks2_future.get());
  add_response1_future.get();
  add_response2_future.get();

  while (count != kMessageCount);
}

TEST_F(TimerTest, BEH_MultipleGroupResponse) {
  const int kMessageCount(400);
  boost::progress_timer t;
  std::atomic<int> count(0);
  TaskResponseFunctor response_functor = [&count](std::string response) {
    EXPECT_FALSE(response.empty());
    ++count;
  };

  auto add_tasks = [&](const int& number)->std::vector<protobuf::Message> {
      std::vector<protobuf::Message> messages;
      for (int i(0); i != number; ++i)
        messages.push_back(CreateMessage(timer_.AddTask(bptime::seconds(80),
                                                        response_functor, 4)));
      return messages;
    };

  auto add_response = [&](std::vector<protobuf::Message>&& messages) {
      for (size_t i(0); i != messages.size(); ++i)
        for (size_t j(0); j != 4; ++j)
          timer_.AddResponse(messages.at(i));
    };

  auto add_tasks1_future = std::async(std::launch::async, add_tasks, kMessageCount/2);
  auto add_tasks2_future = std::async(std::launch::async, add_tasks, kMessageCount/2);
  //  std::cout << "Task enqueued in " << t.elapsed();

  auto add_response1_future = std::async(std::launch::async, add_response, add_tasks1_future.get());
  auto add_response2_future = std::async(std::launch::async, add_response, add_tasks2_future.get());
  add_response1_future.get();
  add_response2_future.get();

  while (count != (kMessageCount * 4));
}

TEST_F(TimerTest, BEH_SingleResponseTimedOut) {
  timer_.AddTask(bptime::milliseconds(100), failed_response_functor_,
                 1);
  boost::this_thread::disable_interruption disable_interruption;
  Sleep(bptime::milliseconds(200));
  EXPECT_EQ(failed_response_count_, 1U);
}

TEST_F(TimerTest, BEH_GroupResponseWithMoreChecks) {
  std::mutex mutex;
  int count(0);
  std::vector<std::string> in_responses;
  std::set<std::string> out_responses;
  for (uint16_t i(0); i != kGroupSize_; ++i) {
    std::string response_str = RandomAlphaNumericString(1024 * 512);
    in_responses.push_back(response_str);
  }
  TaskResponseFunctor response_functor = [&count, &in_responses, &out_responses, &mutex]
      (std::string response) {
    std::lock_guard<std::mutex> lock(mutex);
    ++count;
    EXPECT_FALSE(response.empty());
    auto itr = std::find(in_responses.begin(), in_responses.end(), response);
    EXPECT_TRUE(itr != in_responses.end());
    auto set_itr = out_responses.insert(response);
    EXPECT_TRUE(set_itr.second);
  };
  message_.set_id(timer_.AddTask(bptime::seconds(3),
                                 response_functor,
                                 kGroupSize_));
  for (uint16_t i(0); i != kGroupSize_; ++i) {
    message_.clear_data();
    message_.add_data(in_responses.at(i));
    timer_.AddResponse(message_);
  }
  while (count != kGroupSize_);
}

TEST_F(TimerTest, BEH_GroupResponse) {
  message_.set_id(timer_.AddTask(bptime::seconds(2),
                                 pass_response_functor_,
                                 kGroupSize_));
  for (uint16_t i(0); i != kGroupSize_; ++i)
    timer_.AddResponse(message_);
  while (pass_response_count_ != kGroupSize_);
}

TEST_F(TimerTest, BEH_GroupResponsePartialResult) {
  message_.set_id(timer_.AddTask(bptime::milliseconds(100),
                                 group_failed_response_functor_,
                                 kGroupSize_));
  for (uint16_t i(0); i != kGroupSize_ - 1; ++i)
    timer_.AddResponse(message_);

  Sleep(bptime::milliseconds(500));
  while ((pass_response_count_ + failed_response_count_) != kGroupSize_);

  EXPECT_EQ(pass_response_count_, (kGroupSize_ - 1));
  EXPECT_EQ(failed_response_count_, 1);
}

TEST_F(TimerTest, BEH_VariousResults) {
  std::vector<protobuf::Message> messages_to_be_added;
  messages_to_be_added.reserve(100 * kGroupSize_ * 2);
  for (int i(0); i != 100; ++i) {
    // Single message with response
    message_.set_id(timer_.AddTask(bptime::seconds(10),
                                   pass_response_functor_, 1));
    messages_to_be_added.push_back(message_);
    // Single message without response
    timer_.AddTask(bptime::seconds(5), failed_response_functor_, 1);
    // Group message with all responses
    message_.set_id(timer_.AddTask(bptime::seconds(10),
                                   pass_response_functor_,
                                   kGroupSize_));
    for (uint16_t i(0); i != kGroupSize_; ++i)
      messages_to_be_added.push_back(message_);
    // Group message with all bar one responses
    message_.set_id(timer_.AddTask(bptime::seconds(5),
                                   group_failed_response_functor_,
                                   kGroupSize_));
    for (uint16_t i(0); i != kGroupSize_ - 1; ++i)
      messages_to_be_added.push_back(message_);
  }

  std::random_shuffle(messages_to_be_added.begin(),
                      messages_to_be_added.end());

  for (const protobuf::Message& message : messages_to_be_added)
    timer_.AddResponse(message);

  boost::this_thread::disable_interruption disable_interruption;
  Sleep(bptime::seconds(5));
  //  Pass count comparison calculation [100(single response msgs),
  //  100 *kGroupSize_(passed group response msgs),
  //  100 * (kGroupSize_ - 1)(one group failed response msg and rest passed group response msgs)
  EXPECT_EQ(pass_response_count_, (100 + (100 * kGroupSize_) + (100 * (kGroupSize_ - 1))));
  //  100(failed single response msgs), 100(failed group response msgs)
  EXPECT_EQ(failed_response_count_, 100 + 100);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
