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

#include <chrono>
#include <memory>
#include <vector>

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/log.h"
#include "maidsafe/common/test.h"

#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/timer.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST(TimerTest, BEH_SingleResponse) {
  AsioService asio_service(1);
  asio_service.Start();
  Timer timer(asio_service);

  TaskResponseFunctor response_functor = [&](int result, std::vector<std::string> response) {
      ASSERT_EQ(kSuccess, result);
      ASSERT_EQ(1, response.size());
      LOG(kInfo) << response.size() << " Response received !!";
    };

  protobuf::Message message;
  message.set_id(timer.AddTask(boost::posix_time::seconds(2), response_functor));
  message.set_type(-200);
  message.set_destination_id("destination_id");
  message.set_direct(static_cast<int32_t>(ConnectType::kGroup));
  message.add_data("response data");
  message.set_source_id("source_id");
  timer.ExecuteTask(message);
}

TEST(TimerTest, BEH_SingleResponseTimedOut) {
  AsioService asio_service(1);
  asio_service.Start();
  Timer timer(asio_service);

  TaskResponseFunctor response_functor = [&](int result, std::vector<std::string> response) {
      ASSERT_EQ(kResponseTimeout, result);
      ASSERT_EQ(0, response.size());
      LOG(kInfo) << response.size() << " Response timed out !!";
    };

  protobuf::Message message;
  message.set_id(timer.AddTask(boost::posix_time::milliseconds(100), response_functor));
  message.set_type(-200);
  message.set_destination_id("destination_id");
  message.set_direct(static_cast<int32_t>(ConnectType::kGroup));
  message.add_data("response data");
  message.set_source_id("source_id");
  timer.ExecuteTask(message);
  Sleep(boost::posix_time::milliseconds(200));
}

TEST(TimerTest, BEH_GroupResponse) {
  AsioService asio_service(1);
  asio_service.Start();
  Timer timer(asio_service);

  TaskResponseFunctor response_functor = [&](int result, std::vector<std::string> response) {
      ASSERT_EQ(kSuccess, result);
      ASSERT_EQ(4, response.size());
      LOG(kInfo) << response.size() << " Response received !!";
    };

  protobuf::Message message;
  message.set_id(timer.AddTask(boost::posix_time::seconds(2), response_functor, 4));
  message.set_type(-200);
  message.set_destination_id("destination_id");
  message.set_direct(static_cast<int32_t>(ConnectType::kGroup));
  message.add_data("response data");
  message.set_source_id("source_id");

  timer.ExecuteTask(message);
  timer.ExecuteTask(message);
  timer.ExecuteTask(message);
  timer.ExecuteTask(message);
}

TEST(TimerTest, BEH_GroupResponsePartialResult) {
  AsioService asio_service(1);
  asio_service.Start();
  Timer timer(asio_service);

  TaskResponseFunctor response_functor = [&](int result, std::vector<std::string> response) {
      ASSERT_EQ(kResponseTimeout, result);
      ASSERT_EQ(3, response.size());
      LOG(kInfo) << response.size() << " Response received !!";
    };

  protobuf::Message message;
  message.set_id(timer.AddTask(boost::posix_time::milliseconds(100), response_functor, 4));
  message.set_type(-200);
  message.set_destination_id("destination_id");
  message.set_direct(static_cast<int32_t>(ConnectType::kGroup));
  message.add_data("response data");
  message.set_source_id("source_id");

  timer.ExecuteTask(message);
  timer.ExecuteTask(message);
  timer.ExecuteTask(message);
  Sleep(boost::posix_time::milliseconds(500));
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
