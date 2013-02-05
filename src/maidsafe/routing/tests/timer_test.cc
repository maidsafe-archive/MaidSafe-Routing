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
        single_good_response_functor_(),
        single_failed_response_functor_(),
        group_good_response_functor_(),
        group_failed_response_functor_(),
        message_() {
    asio_service_.Start();

    single_good_response_functor_ = [=](std::string response) {
            ASSERT_FALSE(response.empty());
    };
    single_failed_response_functor_ = [=](std::string response) {
      ASSERT_TRUE(response.empty());
    };
//    group_good_response_functor_ = [=](std::vector<std::string> response) {
//      ASSERT_EQ(kGroupSize_, response.size());
//    };
//    group_failed_response_functor_ = [=](std::vector<std::string> response) {
//      ASSERT_EQ(kGroupSize_ - 1, response.size());
//    };

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
  TaskResponseFunctor single_good_response_functor_,
      single_failed_response_functor_;
  TaskResponseFunctor group_good_response_functor_,
      group_failed_response_functor_;
  protobuf::Message message_;
};

TEST_F(TimerTest, BEH_SingleResponse) {
  message_.set_id(timer_.AddTask(bptime::seconds(2),
                                 single_good_response_functor_, 1));
  timer_.AddResponse(message_);
}

//TEST_F(TimerTest, BEH_MultipleResponse) {
//  const int kMessageCount(400);
//  boost::progress_timer t;
//  std::atomic<int> count(0);
//  TaskResponseFunctor response_functor = [&count](std::string response) {
//    ASSERT_EQ(1U, response.size());
//    ++count;
//  };

//  auto add_tasks = [&](const int& number)->std::vector<protobuf::Message> {
//      std::vector<protobuf::Message> messages;
//      for (int i(0); i != number; ++i)
//        messages.push_back(std::move(CreateMessage(timer_.AddTask(bptime::seconds(50),
//                                                   response_functor, 1))));
//      return messages;
//    };

//  auto add_response = [&](std::vector<protobuf::Message>&& messages) {
//      for (size_t i(0); i != messages.size(); ++i)
//        timer_.AddResponse(messages.at(i));
//    };

//  auto add_tasks1_future = std::async(std::launch::async, add_tasks, kMessageCount/2);
//  auto add_tasks2_future = std::async(std::launch::async, add_tasks, kMessageCount/2);
//  std::vector<protobuf::Message> messages1(std::move(add_tasks1_future.get()));
//  std::vector<protobuf::Message> messages2(std::move(add_tasks2_future.get()));

//  auto add_response1_future = std::async(std::launch::async, add_response, std::move(messages1));
//  auto add_response2_future = std::async(std::launch::async, add_response, std::move(messages2));
//  add_response1_future.get();
//  add_response2_future.get();

//  while (count != kMessageCount);
//}

//TEST_F(TimerTest, BEH_MultipleGroupResponse) {
//  const int kMessageCount(400);
//  boost::progress_timer t;
//  std::atomic<int> count(0);
//  TaskResponseFunctor response_functor = [&count](std::vector<std::string> response) {
//    ASSERT_EQ(4U, response.size());
//    ++count;
//  };

//  auto add_tasks = [&](const int& number)->std::vector<protobuf::Message> {
//      std::vector<protobuf::Message> messages;
//      for (int i(0); i != number; ++i)
//        messages.push_back(std::move(CreateMessage(timer_.AddTask(bptime::seconds(50),
//                                                   response_functor, 4))));
//      return messages;
//    };

//  auto add_response = [&](std::vector<protobuf::Message>&& messages) {
//      for (size_t i(0); i != messages.size(); ++i)
//        for (size_t j(0); j != 4; ++j)
//          timer_.AddResponse(messages.at(i));
//    };

//  auto add_tasks1_future = std::async(std::launch::async, add_tasks, kMessageCount/2);
//  auto add_tasks2_future = std::async(std::launch::async, add_tasks, kMessageCount/2);
//  std::vector<protobuf::Message> messages1(std::move(add_tasks1_future.get()));
//  std::vector<protobuf::Message> messages2(std::move(add_tasks2_future.get()));
//  //  std::cout << "Task enqueued in " << t.elapsed();

//  auto add_response1_future = std::async(std::launch::async, add_response, std::move(messages1));
//  auto add_response2_future = std::async(std::launch::async, add_response, std::move(messages2));
//  add_response1_future.get();
//  add_response2_future.get();

//  while (count != (kMessageCount));
//}

TEST_F(TimerTest, BEH_SingleResponseTimedOut) {
  timer_.AddTask(bptime::milliseconds(100), single_failed_response_functor_,
                 1);
  boost::this_thread::disable_interruption disable_interruption;
  Sleep(bptime::milliseconds(200));
}

TEST_F(TimerTest, BEH_GroupResponse) {
  message_.set_id(timer_.AddTask(bptime::seconds(2),
                                 group_good_response_functor_,
                                 kGroupSize_));
  for (uint16_t i(0); i != kGroupSize_; ++i)
    timer_.AddResponse(message_);
}

//TEST_F(TimerTest, BEH_GroupResponsePartialResult) {
//  message_.set_id(timer_.AddTask(bptime::milliseconds(100),
//                                 group_failed_response_functor_,
//                                 kGroupSize_));
//  for (uint16_t i(0); i != kGroupSize_ - 1; ++i)
//    timer_.AddResponse(message_);

//  Sleep(bptime::milliseconds(500));
//}

//TEST_F(TimerTest, BEH_VariousResults) {
//  std::vector<protobuf::Message> messages_to_be_added;
//  messages_to_be_added.reserve(100 * kGroupSize_ * 2);
//  for (int i(0); i != 100; ++i) {
//    // Single message with response
//    message_.set_id(timer_.AddTask(bptime::seconds(10),
//                                   single_good_response_functor_, 1));
//    messages_to_be_added.push_back(message_);
//    // Single message without response
//    timer_.AddTask(bptime::seconds(5), single_failed_response_functor_, 1);
//    // Group message with all responses
//    message_.set_id(timer_.AddTask(bptime::seconds(10),
//                                   group_good_response_functor_,
//                                   kGroupSize_));
//    for (uint16_t i(0); i != kGroupSize_; ++i)
//      messages_to_be_added.push_back(message_);
//    // Group message with all bar one responses
//    message_.set_id(timer_.AddTask(bptime::seconds(5),
//                                   group_failed_response_functor_,
//                                   kGroupSize_));
//    for (uint16_t i(0); i != kGroupSize_ - 1; ++i)
//      messages_to_be_added.push_back(message_);
//  }

//  std::random_shuffle(messages_to_be_added.begin(),
//                      messages_to_be_added.end());

//  for (const protobuf::Message& message : messages_to_be_added)
//    timer_.AddResponse(message);

//  boost::this_thread::disable_interruption disable_interruption;
//  Sleep(bptime::seconds(5));
//}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
