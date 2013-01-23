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

    single_good_response_functor_ = [=](std::vector<std::string> response) {
      ASSERT_EQ(1U, response.size());
    };
    single_failed_response_functor_ = [=](std::vector<std::string> response) {
      ASSERT_TRUE(response.empty());
    };
    group_good_response_functor_ = [=](std::vector<std::string> response) {
      ASSERT_EQ(kGroupSize_, response.size());
    };
    group_failed_response_functor_ = [=](std::vector<std::string> response) {
      ASSERT_EQ(kGroupSize_ - 1, response.size());
    };

    message_.set_type(-200);
    message_.set_destination_id("destination_id");
    message_.set_direct(false);
    message_.add_data("response data");
    message_.set_source_id("source_id");
  }

  void CheckResponse(std::vector<std::future<std::string>> &future_in,
                      const uint32_t& response_size,
                      const uint32_t& exceptions) {
    uint32_t count = 0;
    for (; count < response_size; ++count)
      future_in.at(count).get();
    ASSERT_EQ(count, response_size);
    for (count = 0; count < exceptions; ++count) {
      EXPECT_THROW(future_in.at(response_size + count).get(), std::exception);
    }
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

TEST_F(TimerTest, BEH_Promise_SingleResponse) {
  uint32_t response_size = 1;
  std::vector<std::shared_ptr<std::promise<std::string>>> promises_in;
  std::vector<std::future<std::string>> future_in;
  for (uint32_t count = 0; count < response_size; ++count) {
    promises_in.push_back(std::make_shared<std::promise<std::string>>());
    future_in.push_back(promises_in[count]->get_future());
  }
  message_.set_id(timer_.AddTask(bptime::seconds(2), promises_in));
  timer_.AddResponse(message_);
  CheckResponse(future_in, response_size, 0);
}

TEST_F(TimerTest, BEH_MultipleResponse) {
  const int kMessageCount(400);
  boost::progress_timer t;
  std::atomic<int> count(0);
  TaskResponseFunctor response_functor = [&count](std::vector<std::string> response) {
    ASSERT_EQ(1U, response.size());
    ++count;
  };

  auto add_tasks = [&](const int& number)->std::vector<protobuf::Message> {
      std::vector<protobuf::Message> messages;
      for (int i(0); i != number; ++i)
        messages.push_back(std::move(CreateMessage(timer_.AddTask(bptime::seconds(50),
                                                   response_functor, 1))));
      return messages;
    };

  auto add_response = [&](std::vector<protobuf::Message>&& messages) {
      for (size_t i(0); i != messages.size(); ++i)
        timer_.AddResponse(messages.at(i));
    };

  auto add_tasks1_future = std::async(std::launch::async, add_tasks, kMessageCount/2);
  auto add_tasks2_future = std::async(std::launch::async, add_tasks, kMessageCount/2);
  std::vector<protobuf::Message> messages1(std::move(add_tasks1_future.get()));
  std::vector<protobuf::Message> messages2(std::move(add_tasks2_future.get()));
//  std::cout << "Task enqueued in " << t.elapsed();

  auto add_response1_future = std::async(std::launch::async, add_response, std::move(messages1));
  auto add_response2_future = std::async(std::launch::async, add_response, std::move(messages2));
  add_response1_future.get();
  add_response2_future.get();

  while (count != kMessageCount);
}

TEST_F(TimerTest, BEH_MultipleGroupResponse) {
  const int kMessageCount(400);
  boost::progress_timer t;
  std::atomic<int> count(0);
  TaskResponseFunctor response_functor = [&count](std::vector<std::string> response) {
    ASSERT_EQ(4U, response.size());
    ++count;
//    std::cout << "\n count" << count;
  };

  auto add_tasks = [&](const int& number)->std::vector<protobuf::Message> {
      std::vector<protobuf::Message> messages;
      for (int i(0); i != number; ++i)
        messages.push_back(std::move(CreateMessage(timer_.AddTask(bptime::seconds(50),
                                                   response_functor, 4))));
      return messages;
    };

  auto add_response = [&](std::vector<protobuf::Message>&& messages) {
      for (size_t i(0); i != messages.size(); ++i)
        for (size_t j(0); j != 4; ++j)
          timer_.AddResponse(messages.at(i));
    };

  auto add_tasks1_future = std::async(std::launch::async, add_tasks, kMessageCount/2);
  auto add_tasks2_future = std::async(std::launch::async, add_tasks, kMessageCount/2);
  std::vector<protobuf::Message> messages1(std::move(add_tasks1_future.get()));
  std::vector<protobuf::Message> messages2(std::move(add_tasks2_future.get()));
  //  std::cout << "Task enqueued in " << t.elapsed();

  auto add_response1_future = std::async(std::launch::async, add_response, std::move(messages1));
  auto add_response2_future = std::async(std::launch::async, add_response, std::move(messages2));
  add_response1_future.get();
  add_response2_future.get();

  while (count != (kMessageCount));
}

TEST_F(TimerTest, BEH_Promise_MultipleResponse) {
  const int kMessageCount(400);
  boost::progress_timer t;
  std::atomic<int> count(0);
  std::shared_ptr<std::vector<std::future<std::string>>>
     futures1 = std::make_shared<std::vector<std::future<std::string>>>();
  std::shared_ptr<std::vector<std::future<std::string>>>
     futures2 = std::make_shared<std::vector<std::future<std::string>>>();

  auto add_tasks = [&](const int& number,
      std::shared_ptr<std::vector<std::future<std::string>>>
                       futures)->std::vector<protobuf::Message> {
        std::vector<protobuf::Message> messages;
        for (int i(0); i != number; ++i) {
          std::vector<std::shared_ptr<std::promise<std::string>>> promises_in;
          promises_in.push_back(std::make_shared<std::promise<std::string>>());
          auto future(promises_in[0]->get_future());
          futures->push_back(std::move(future));
          messages.push_back(std::move(CreateMessage(timer_.AddTask(bptime::seconds(50),
                                                     promises_in))));
        }
        return messages;
      };

  auto add_response = [&](std::vector<protobuf::Message>&& messages) {
        for (size_t i(0); i != messages.size(); ++i)
          timer_.AddResponse(messages.at(i));
      };

  auto add_tasks1_future = std::async(std::launch::async, add_tasks, kMessageCount / 2, futures1);
  auto add_tasks2_future = std::async(std::launch::async, add_tasks, kMessageCount / 2, futures2);

  std::vector<protobuf::Message> messages1(std::move(add_tasks1_future.get()));
  std::vector<protobuf::Message> messages2(std::move(add_tasks2_future.get()));

//  std::cout << "Task enqueued in " << t.elapsed();
  auto add_response1_future = std::async(std::launch::async, add_response, std::move(messages1));
  auto add_response2_future = std::async(std::launch::async, add_response, std::move(messages2));
  add_response1_future.get();
  add_response2_future.get();

  for (auto itr(futures2->begin()); itr != futures2->end(); ++itr)
    futures1->push_back(std::move(*itr));

  while (!futures1->empty()) {
    futures1->erase(std::remove_if(futures1->begin(), futures1->end(),
        [&count](std::future<std::string>& str)->bool {
            if (IsReady(str)) {
              try {
                auto response(str.get());
                EXPECT_FALSE(response.empty());
                ++count;
              } catch(std::exception& ex) {
                LOG(kError) << "Exception : " << ex.what();
                EXPECT_TRUE(false) << ex.what();
              }
                return true;
              } else  {
                return false;
              };
        }), futures1->end());
    std::this_thread::yield();
  }
  ASSERT_EQ(kMessageCount, count);
}


TEST_F(TimerTest, BEH_Promise_MultipleGroupResponse) {
  const int kMessageCount(400);
  boost::progress_timer t;
  std::atomic<int> count(0);
  std::shared_ptr<std::vector<std::future<std::string>>>
     futures1 = std::make_shared<std::vector<std::future<std::string>>>();
  std::shared_ptr<std::vector<std::future<std::string>>>
     futures2 = std::make_shared<std::vector<std::future<std::string>>>();
  auto add_tasks = [&](const int& number,
      std::shared_ptr<std::vector<std::future<std::string>>>
                       futures)->std::vector<protobuf::Message> {
      std::vector<protobuf::Message> messages;
      for (int i(0); i != number; ++i) {
        std::vector<std::shared_ptr<std::promise<std::string>>> promises_in;
        for (size_t j(0); j != 4; ++j) {
          promises_in.push_back(std::make_shared<std::promise<std::string>>());
          auto future(promises_in[j]->get_future());
          futures->push_back(std::move(future));
        }
        messages.push_back(std::move(CreateMessage(timer_.AddTask(bptime::seconds(50),
                                                   promises_in))));
      }
      return messages;
    };

  auto add_response = [&](std::vector<protobuf::Message>&& messages) {
      for (size_t i(0); i != messages.size(); ++i)
        for (size_t j(0); j != 4; ++j)
          timer_.AddResponse(messages.at(i));
    };

  auto add_tasks1_future = std::async(std::launch::async, add_tasks, kMessageCount / 2, futures1);
  auto add_tasks2_future = std::async(std::launch::async, add_tasks, kMessageCount / 2, futures2);

  std::vector<protobuf::Message> messages1(std::move(add_tasks1_future.get()));
  std::vector<protobuf::Message> messages2(std::move(add_tasks2_future.get()));

  //  std::cout << "Task enqueued in " << t.elapsed();
  auto add_response1_future = std::async(std::launch::async, add_response, std::move(messages1));
  auto add_response2_future = std::async(std::launch::async, add_response, std::move(messages2));
  add_response1_future.get();
  add_response2_future.get();

  for (auto itr(futures2->begin()); itr != futures2->end(); ++itr)
    futures1->push_back(std::move(*itr));

  while (!futures1->empty()) {
    futures1->erase(std::remove_if(futures1->begin(), futures1->end(),
        [&count](std::future<std::string>& str)->bool {
            if (IsReady(str)) {
              try {
                auto response(str.get());
                EXPECT_FALSE(response.empty());
                ++count;
              } catch(std::exception& ex) {
                LOG(kError) << "Exception : " << ex.what();
                EXPECT_TRUE(false) << ex.what();
              }
                return true;
              } else  {
                return false;
              };
        }), futures1->end());
    std::this_thread::yield();
  }
  ASSERT_EQ((kMessageCount * 4), count);
}

TEST_F(TimerTest, BEH_Promise_SingleResponseTimedOut) {
  uint32_t response_size = 1;
  std::vector<std::shared_ptr<std::promise<std::string>>> promises_in;
  std::vector<std::future<std::string>> future_in;
  for (uint32_t count = 0; count < response_size; ++count) {
    promises_in.push_back(std::make_shared<std::promise<std::string>>());
  }
  timer_.AddTask(bptime::milliseconds(100), promises_in);
  Sleep(bptime::milliseconds(200));
  CheckResponse(future_in, 0, 1);
}

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

TEST_F(TimerTest, BEH_Promise_GroupResponse) {
  uint32_t response_size = kGroupSize_;
  std::vector<std::shared_ptr<std::promise<std::string>>> promises_in;
  std::vector<std::future<std::string>> future_in;
  for (uint32_t count = 0; count < response_size; ++count) {
    promises_in.push_back(std::make_shared<std::promise<std::string>>());
    future_in.push_back(promises_in[count]->get_future());
  }
  message_.set_id(timer_.AddTask(bptime::seconds(2), promises_in));
  for (uint32_t count = 0; count < response_size; ++count)
    timer_.AddResponse(message_);
  CheckResponse(future_in, response_size, 0);
}

TEST_F(TimerTest, BEH_GroupResponsePartialResult) {
  message_.set_id(timer_.AddTask(bptime::milliseconds(100),
                                 group_failed_response_functor_,
                                 kGroupSize_));
  for (uint16_t i(0); i != kGroupSize_ - 1; ++i)
    timer_.AddResponse(message_);

  Sleep(bptime::milliseconds(500));
}

TEST_F(TimerTest, BEH_Promise_GroupResponsePartialResult) {
  uint32_t response_size = kGroupSize_;
  std::vector<std::shared_ptr<std::promise<std::string>>> promises_in;
  std::vector<std::future<std::string>> future_in;
  for (uint32_t count = 0; count < response_size -1; ++count) {
    promises_in.push_back(std::make_shared<std::promise<std::string>>());
    future_in.push_back(promises_in[count]->get_future());
  }
  message_.set_id(timer_.AddTask(bptime::milliseconds(200), promises_in));
  for (uint32_t count = 0; count < response_size - 1; ++count)
    timer_.AddResponse(message_);

  Sleep(bptime::milliseconds(500));

  CheckResponse(future_in, response_size - 1, 1);
}

TEST_F(TimerTest, BEH_VariousResults) {
  std::vector<protobuf::Message> messages_to_be_added;
  messages_to_be_added.reserve(100 * kGroupSize_ * 2);
  for (int i(0); i != 100; ++i) {
    // Single message with response
    message_.set_id(timer_.AddTask(bptime::seconds(10),
                                   single_good_response_functor_, 1));
    messages_to_be_added.push_back(message_);
    // Single message without response
    timer_.AddTask(bptime::seconds(5), single_failed_response_functor_, 1);
    // Group message with all responses
    message_.set_id(timer_.AddTask(bptime::seconds(10),
                                   group_good_response_functor_,
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
}

TEST_F(TimerTest, BEH_Promise_VariousResults) {
  uint32_t response_size = kGroupSize_;
  std::vector<std::shared_ptr<std::promise<std::string>>> single_promises_in;
  std::vector<std::shared_ptr<std::promise<std::string>>> group_promises_in;
  std::vector<std::shared_ptr<std::promise<std::string>>> partial_promises_in;
  std::vector<std::shared_ptr<std::promise<std::string>>> failed_promises_in;

  std::vector<std::future<std::string>> single_future_in;
  std::vector<std::future<std::string>> group_future_in;
  std::vector<std::future<std::string>> partial_future_in;
  std::vector<std::future<std::string>> failed_future_in;

  single_promises_in.push_back(std::make_shared<std::promise<std::string>>());
  single_future_in.push_back(single_promises_in[0]->get_future());

  for (uint32_t count = 0; count < response_size; ++count) {
    group_promises_in.push_back(std::make_shared<std::promise<std::string>>());
    group_future_in.push_back(group_promises_in[count]->get_future());
  }
  for (uint32_t count = 0; count < response_size - 1; ++count) {
    partial_promises_in.push_back(std::make_shared<std::promise<std::string>>());
    partial_future_in.push_back(partial_promises_in[count]->get_future());
  }
  std::vector<protobuf::Message> messages_to_be_added;
  messages_to_be_added.reserve(kGroupSize_ * 2);
  // Single message with response
  message_.set_id(timer_.AddTask(bptime::seconds(10), single_promises_in));
  messages_to_be_added.push_back(message_);
  // Single message without response
  timer_.AddTask(bptime::seconds(5), failed_promises_in);
  // Group message with all responses
  message_.set_id(timer_.AddTask(bptime::seconds(10), group_promises_in));
  for (uint16_t i(0); i != kGroupSize_; ++i)
  messages_to_be_added.push_back(message_);
  // Group message with all bar one responses
  message_.set_id(timer_.AddTask(bptime::seconds(5), partial_promises_in));
  for (uint16_t i(0); i != kGroupSize_ - 1; ++i)
    messages_to_be_added.push_back(message_);

  std::random_shuffle(messages_to_be_added.begin(), messages_to_be_added.end());

  for (const protobuf::Message& message : messages_to_be_added)
    timer_.AddResponse(message);

  Sleep(bptime::seconds(5));
  CheckResponse(single_future_in, 1, 0);
  CheckResponse(group_future_in, response_size, 0);
  CheckResponse(partial_future_in, response_size - 1, 1);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
