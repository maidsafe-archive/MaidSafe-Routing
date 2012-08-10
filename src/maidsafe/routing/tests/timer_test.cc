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
      : asio_service_(1),
        timer_(asio_service_),
        kGroupSize_((RandomUint32() % 97) + 4),
        single_good_response_functor_(),
        single_failed_response_functor_(),
        group_good_response_functor_(),
        group_failed_response_functor_(),
        message_() {
    asio_service_.Start();

    single_good_response_functor_ = [=](int result, std::vector<std::string> response) {
      ASSERT_EQ(kSuccess, result);
      ASSERT_EQ(1U, response.size());
    };
    single_failed_response_functor_ = [=](int result, std::vector<std::string> response) {
      ASSERT_EQ(kResponseTimeout, result);
      ASSERT_TRUE(response.empty());
    };
    group_good_response_functor_ = [=](int result, std::vector<std::string> response) {
      ASSERT_EQ(kSuccess, result);
      ASSERT_EQ(kGroupSize_, response.size());
    };
    group_failed_response_functor_ = [=](int result, std::vector<std::string> response) {
      ASSERT_EQ(kResponseTimeout, result);
      ASSERT_EQ(kGroupSize_ - 1, response.size());
    };

    message_.set_type(-200);
    message_.set_destination_id("destination_id");
    message_.set_direct(static_cast<int32_t>(ConnectType::kGroup));
    message_.add_data("response data");
    message_.set_source_id("source_id");
  }

 protected:
  AsioService asio_service_;
  Timer timer_;
  const uint16_t kGroupSize_;
  TaskResponseFunctor single_good_response_functor_, single_failed_response_functor_;
  TaskResponseFunctor group_good_response_functor_, group_failed_response_functor_;
  protobuf::Message message_;
};

TEST_F(TimerTest, BEH_SingleResponse) {
  message_.set_id(timer_.AddTask(bptime::seconds(2), single_good_response_functor_, 1));
  timer_.AddResponse(message_);
}

TEST_F(TimerTest, BEH_SingleResponseTimedOut) {
  timer_.AddTask(bptime::milliseconds(100), single_failed_response_functor_, 1);
  boost::this_thread::disable_interruption disable_interruption;
  Sleep(bptime::milliseconds(200));
}

TEST_F(TimerTest, BEH_GroupResponse) {
  message_.set_id(timer_.AddTask(bptime::seconds(2), group_good_response_functor_, kGroupSize_));
  for (uint16_t i(0); i != kGroupSize_; ++i)
    timer_.AddResponse(message_);
}

TEST_F(TimerTest, BEH_GroupResponsePartialResult) {
  message_.set_id(timer_.AddTask(bptime::milliseconds(100), group_failed_response_functor_,
                                 kGroupSize_));
  for (uint16_t i(0); i != kGroupSize_ - 1; ++i)
    timer_.AddResponse(message_);

  boost::this_thread::disable_interruption disable_interruption;
  Sleep(bptime::milliseconds(500));
}

TEST_F(TimerTest, BEH_VariousResults) {
  std::vector<protobuf::Message> messages_to_be_added;
  messages_to_be_added.reserve(100 * kGroupSize_ * 2);
  for (int i(0); i != 100; ++i) {
    // Single message with response
    message_.set_id(timer_.AddTask(bptime::seconds(10), single_good_response_functor_, 1));
    messages_to_be_added.push_back(message_);
    // Single message without response
    timer_.AddTask(bptime::seconds(5), single_failed_response_functor_, 1);
    // Group message with all responses
    message_.set_id(timer_.AddTask(bptime::seconds(10), group_good_response_functor_, kGroupSize_));
    for (uint16_t i(0); i != kGroupSize_; ++i)
      messages_to_be_added.push_back(message_);
    // Group message with all bar one responses
    message_.set_id(timer_.AddTask(bptime::seconds(5), group_failed_response_functor_,
                                   kGroupSize_));
    for (uint16_t i(0); i != kGroupSize_ - 1; ++i)
      messages_to_be_added.push_back(message_);
  }

  std::random_shuffle(messages_to_be_added.begin(), messages_to_be_added.end());

  for (const protobuf::Message& message : messages_to_be_added)
    timer_.AddResponse(message);

  boost::this_thread::disable_interruption disable_interruption;
  Sleep(bptime::seconds(5));
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
