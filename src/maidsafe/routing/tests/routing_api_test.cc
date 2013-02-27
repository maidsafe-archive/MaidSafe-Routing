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
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <vector>

#include "boost/asio.hpp"
#include "boost/exception/all.hpp"
#include "boost/filesystem/exception.hpp"
#include "boost/progress.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/parameters.h"

#include "maidsafe/passport/types.h"

#include "maidsafe/routing/bootstrap_file_handler.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/routing_impl.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

namespace bptime = boost::posix_time;
namespace fs = boost::filesystem;

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

const int kClientCount(4);
const int kServerCount(10);
const int kNetworkSize = kClientCount + kServerCount;

}  // anonymous namespace

TEST(APITest, BEH_API_ZeroState) {
  auto pmid1(MakePmid()), pmid2(MakePmid()), pmid3(MakePmid());
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeysWithPmid(pmid1));
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeysWithPmid(pmid2));
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeysWithPmid(pmid3));
  std::map<NodeId, asymm::PublicKey> key_map;
  key_map.insert(std::make_pair(node1.node_info.node_id, pmid1.public_key()));
  key_map.insert(std::make_pair(node2.node_info.node_id, pmid2.public_key()));
  key_map.insert(std::make_pair(node3.node_info.node_id, pmid3.public_key()));

  Functors functors1, functors2, functors3;
  Routing routing1(&pmid1);
  Routing routing2(&pmid2);
  Routing routing3(&pmid3);

  functors1.network_status = [](const int&) {};  // NOLINT (Fraser)
  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
      LOG(kWarning) << "node_validation called for " << DebugId(node_id);
      auto itr(key_map.find(node_id));
      if (key_map.end() != itr)
        give_key((*itr).second);
    };

  functors2.network_status = functors3.network_status = functors1.network_status;
  functors2.request_public_key = functors3.request_public_key = functors1.request_public_key;
  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
    endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  auto a1 = std::async(std::launch::async,
      [&] { return routing1.ZeroStateJoin(functors1, endpoint1, endpoint2, node2.node_info);
      });
  auto a2 = std::async(std::launch::async,
      [&] { return routing2.ZeroStateJoin(functors2, endpoint2, endpoint1, node1.node_info);
      });
  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !
  std::once_flag flag;
  std::promise<bool> join_promise;
  auto join_future = join_promise.get_future();

  functors3.network_status = [&flag, &join_promise] (int result) {
    if (result == NetworkStatus(false, 2)) {
        std::call_once(flag, [&join_promise]() { join_promise.set_value(true); }
       );
    }
  };

  routing3.Join(functors3, std::vector<Endpoint>(1, endpoint2));
  EXPECT_EQ(join_future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
  if (join_future.is_ready())
    EXPECT_TRUE(join_future.get());
  LOG(kInfo) << "done!!!";
}

TEST(APITest, BEH_API_ZeroStateWithDuplicateNode) {
  rudp::Parameters::bootstrap_connection_lifespan = boost::posix_time::seconds(5);
  auto pmid1(MakePmid()), pmid2(MakePmid()), pmid3(MakePmid());
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeysWithPmid(pmid1));
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeysWithPmid(pmid2));
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeysWithPmid(pmid3));
  NodeInfoAndPrivateKey node4(MakeNodeInfoAndKeysWithPmid(pmid3));
  std::map<NodeId, asymm::PublicKey> key_map;
  key_map.insert(std::make_pair(node1.node_info.node_id, pmid1.public_key()));
  key_map.insert(std::make_pair(node2.node_info.node_id, pmid2.public_key()));
  key_map.insert(std::make_pair(node3.node_info.node_id, pmid3.public_key()));

  Functors functors1, functors2, functors3, functors4;
  Routing routing1(&pmid1);
  Routing routing2(&pmid2);
  Routing routing3(&pmid3);
  Routing routing4(&pmid3);

  functors1.network_status = [](const int&) {};  // NOLINT (Fraser)
  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
      LOG(kWarning) << "node_validation called for " << DebugId(node_id);
      auto itr(key_map.find(node_id));
      if (key_map.end() != itr)
        give_key((*itr).second);
    };

  functors2.network_status = functors3.network_status = functors4.network_status =
      functors1.network_status;
  functors2.request_public_key = functors3.request_public_key = functors1.request_public_key;
  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
    endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  auto a1 = std::async(std::launch::async,
      [&] { return routing1.ZeroStateJoin(functors1, endpoint1, endpoint2, node2.node_info);
      });
  auto a2 = std::async(std::launch::async,
      [&] { return routing2.ZeroStateJoin(functors2, endpoint2, endpoint1, node1.node_info);
      });
  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  std::once_flag flag;
  std::promise<bool> join_promise;
  auto join_future = join_promise.get_future();
  functors3.network_status = [&flag, &join_promise](int result) {
    if (result == NetworkStatus(false, 2)) {
        std::call_once(flag, [&join_promise]() { join_promise.set_value(true); }
        );
    }
  };

  routing3.Join(functors3, std::vector<Endpoint>(1, endpoint2));
  EXPECT_EQ(join_future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
  if (IsReady(join_future))
    EXPECT_TRUE(join_future.get());

  std::atomic<int> final_result;
  functors4.network_status = [&final_result](int result) {
     final_result = result;
  };
  routing4.Join(functors4, std::vector<Endpoint>(1, endpoint2));
  Sleep(boost::posix_time::seconds(5));
  EXPECT_LT(final_result, 0);
  LOG(kInfo) << "done!!!";
  rudp::Parameters::bootstrap_connection_lifespan = boost::posix_time::minutes(10);
}

TEST(APITest, FUNC_API_AnonymousNode) {
  rudp::Parameters::bootstrap_connection_lifespan = boost::posix_time::seconds(10);
  auto pmid1(MakePmid()), pmid2(MakePmid());
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeysWithPmid(pmid1));
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeysWithPmid(pmid2));
  std::map<NodeId, asymm::PublicKey> key_map;
  key_map.insert(std::make_pair(node1.node_info.node_id, pmid1.public_key()));
  key_map.insert(std::make_pair(node2.node_info.node_id, pmid2.public_key()));

  Functors functors1, functors2, functors3;
  Routing routing1(&pmid1);
  Routing routing2(&pmid2);
  Routing routing3(nullptr);  // Anonymous node

  functors1.network_status = [](const int&) {};  // NOLINT (Fraser)
  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
      LOG(kWarning) << "node_validation called for " << DebugId(node_id);
      auto itr(key_map.find(node_id));
      if (key_map.end() != itr)
        give_key((*itr).second);
    };

  functors1.message_received = [&] (const std::string& message, const NodeId&, const bool&,
                                     ReplyFunctor reply_functor) {
      reply_functor("response to " + message);
      LOG(kVerbose) << "Message received and replied to message !!";
    };

  functors2.network_status = functors1.network_status;
  functors2.request_public_key = functors1.request_public_key;
  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
           endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  auto a1 = std::async(std::launch::async,
      [&] { return routing1.ZeroStateJoin(functors1, endpoint1, endpoint2, node2.node_info);
      });
  auto a2 = std::async(std::launch::async,
      [&] { return routing2.ZeroStateJoin(functors2, endpoint2, endpoint1, node1.node_info);
      });
  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  std::once_flag join_set_promise_flag;
  std::promise<bool> join_promise;
  auto join_future = join_promise.get_future();
  functors3.network_status = [&join_promise, &join_set_promise_flag](int result) {
    LOG(kVerbose) << "Network status for anonymous node called: " << result;
    std::call_once(join_set_promise_flag,
                   [&join_promise, &result] {
                     ASSERT_EQ(kSuccess, result);
                     join_promise.set_value(result == NetworkStatus(true, 0));
                     LOG(kVerbose) << "Anonymous Node joined";
                   });
    LOG(kVerbose) << "Recieved network status of : " << result;
  };

  routing3.Join(functors3, std::vector<Endpoint>(1, endpoint2));
  EXPECT_EQ(join_future.wait_for(std::chrono::seconds(10)), std::future_status::ready);

  // Test SendDirect (1)
  std::string message_1("message_from_anonymous node");
  std::mutex mutex_1;
  std::condition_variable cond_var_1;
  ResponseFunctor response_functor_1 = [&message_1, &mutex_1, &cond_var_1] (std::string string) {
    std::unique_lock<std::mutex> lock(mutex_1);
    ASSERT_EQ(("response to " + message_1), string);
    cond_var_1.notify_one();
  };
  routing3.SendDirect(node1.node_info.node_id, message_1, false, response_functor_1);
  std::unique_lock<std::mutex> lock_1(mutex_1);
  EXPECT_EQ(std::cv_status::no_timeout, cond_var_1.wait_for(lock_1, std::chrono::seconds(10)));

  Sleep(boost::posix_time::seconds(11));  // to allow disconnection

  // Test SendDirect (2)
  std::string message_2("message_2_from_anonymous node");
  std::mutex mutex_2;
  std::condition_variable cond_var_2;
  ResponseFunctor response_functor_2 = [&message_2, &mutex_2, &cond_var_2] (std::string string) {
    std::unique_lock<std::mutex> lock(mutex_2);
    ASSERT_EQ(0, string.size());
    cond_var_2.notify_one();
  };
  routing3.SendDirect(node1.node_info.node_id, message_2, false, response_functor_2);
  std::unique_lock<std::mutex> lock_2(mutex_2);
  EXPECT_EQ(std::cv_status::timeout, cond_var_1.wait_for(lock_2, std::chrono::seconds(1)));

  rudp::Parameters::bootstrap_connection_lifespan = boost::posix_time::minutes(10);
}

TEST(APITest, BEH_API_SendToSelf) {
  auto pmid1(MakePmid()), pmid2(MakePmid()), pmid3(MakePmid());
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeysWithPmid(pmid1));
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeysWithPmid(pmid2));
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeysWithPmid(pmid3));
  std::map<NodeId, asymm::PublicKey> key_map;
  key_map.insert(std::make_pair(node1.node_info.node_id, pmid1.public_key()));
  key_map.insert(std::make_pair(node2.node_info.node_id, pmid2.public_key()));
  key_map.insert(std::make_pair(node3.node_info.node_id, pmid3.public_key()));

  Functors functors1, functors2, functors3;
  Routing routing1(&pmid1);
  Routing routing2(&pmid2);
  Routing routing3(&pmid3);

  functors1.network_status = [](const int&) {};  // NOLINT (Fraser)
  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
      LOG(kWarning) << "node_validation called for " << DebugId(node_id);
      auto itr(key_map.find(node_id));
      if (key_map.end() != itr)
        give_key((*itr).second);
    };

  functors1.message_received = [&] (const std::string& message, const NodeId&, const bool&,
                                     ReplyFunctor reply_functor) {
      reply_functor("response to " + message);
      LOG(kVerbose) << "Message received and replied to message !!";
    };

  functors2.network_status = functors3.network_status = functors1.network_status;
  functors2.request_public_key = functors3.request_public_key = functors1.request_public_key;
  functors2.message_received = functors3.message_received = functors1.message_received;

  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
           endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  auto a1 = std::async(std::launch::async,
      [&] { return routing1.ZeroStateJoin(functors1, endpoint1, endpoint2, node2.node_info);
       });
  auto a2 = std::async(std::launch::async,
      [&] { return routing2.ZeroStateJoin(functors2, endpoint2, endpoint1, node1.node_info);
      });

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  std::once_flag join_set_promise_flag;
  std::promise<bool> join_promise;
  auto join_future = join_promise.get_future();
  functors3.network_status = [&join_set_promise_flag, &join_promise](int result) {
    ASSERT_GE(result, kSuccess);
    if (result == NetworkStatus(false, 2)) {
      std::call_once(join_set_promise_flag,
                     [&join_promise, &result] {
                       LOG(kVerbose) << "3rd node joined";
                       join_promise.set_value(true);
                     });
    }
  };

  routing3.Join(functors3, std::vector<Endpoint>(1, endpoint2));
  EXPECT_EQ(join_future.wait_for(std::chrono::seconds(10)), std::future_status::ready);

  // Test SendDirect
  std::string data("message from my node");
  std::mutex mutex;
  std::condition_variable cond_var;
  ResponseFunctor response_functor = [&data, &mutex, &cond_var] (std::string string) {
    std::unique_lock<std::mutex> lock(mutex);
    ASSERT_EQ("response to " + data, string);
    cond_var.notify_one();
  };
  routing3.SendDirect(node3.node_info.node_id, data, false, response_functor);
  std::unique_lock<std::mutex> lock(mutex);
  EXPECT_EQ(std::cv_status::no_timeout, cond_var.wait_for(lock, std::chrono::seconds(10)));
}


TEST(APITest, BEH_API_ClientNode) {
  auto pmid1(MakePmid()), pmid2(MakePmid());
  auto maid(MakeMaid());
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeysWithPmid(pmid1));
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeysWithPmid(pmid2));
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeysWithMaid(maid));
  std::map<NodeId, asymm::PublicKey> key_map;
  key_map.insert(std::make_pair(node1.node_info.node_id, pmid1.public_key()));
  key_map.insert(std::make_pair(node2.node_info.node_id, pmid2.public_key()));
  key_map.insert(std::make_pair(node3.node_info.node_id, maid.public_key()));

  Functors functors1, functors2, functors3;
  Routing routing1(&pmid1);
  Routing routing2(&pmid2);
  Routing routing3(&maid);

  functors1.network_status = [](const int&) {};  // NOLINT (Fraser)
  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
      LOG(kWarning) << "node_validation called for " << DebugId(node_id);
      auto itr(key_map.find(node_id));
      if (key_map.end() != itr)
        give_key((*itr).second);
    };

  functors1.message_received = [&] (const std::string& message, const NodeId&, const bool&,
                                    ReplyFunctor reply_functor) {
      reply_functor("response to " + message);
        LOG(kVerbose) << "Message received and replied to message !!";
    };

  functors2.network_status = functors3.network_status = functors1.network_status;
  functors2.request_public_key = functors3.request_public_key = functors1.request_public_key;
  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
           endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  auto a1 = std::async(std::launch::async,
      [&] { return routing1.ZeroStateJoin(functors1, endpoint1, endpoint2, node2.node_info);
       });
  auto a2 = std::async(std::launch::async,
      [&] { return routing2.ZeroStateJoin(functors2, endpoint2, endpoint1, node1.node_info);
      });
  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  std::once_flag join_set_promise_flag;
  std::promise<bool> join_promise;
  auto join_future = join_promise.get_future();
  functors3.network_status = [&join_set_promise_flag, &join_promise](int result) {
    ASSERT_GE(result, kSuccess);
    if (result == NetworkStatus(true, 2)) {
      LOG(kVerbose) << "3rd node joined";
      std::call_once(join_set_promise_flag,
                     [&join_promise, &result] {
                       join_promise.set_value(true);
                     });
    }
  };

  routing3.Join(functors3, std::vector<Endpoint>(1, endpoint2));
  EXPECT_EQ(join_future.wait_for(std::chrono::seconds(10)), std::future_status::ready);

  // Test SendDirect
  std::mutex mutex;
  std::condition_variable cond_var;
  std::string data(RandomAlphaNumericString(512 * 1024));
  ResponseFunctor response_functor = [&cond_var, &mutex, &data] (std::string string) {
      std::unique_lock<std::mutex> lock(mutex);
      ASSERT_EQ(("response to " + data), string);
      cond_var.notify_one();
  };
  routing3.SendDirect(node1.node_info.node_id,
                      data,
                      false,
                      response_functor);
  std::unique_lock<std::mutex> lock(mutex);
  EXPECT_EQ(std::cv_status::no_timeout, cond_var.wait_for(lock, std::chrono::seconds(10)));
}

TEST(APITest, BEH_API_ClientNodeSameId) {
  auto pmid1(MakePmid()), pmid2(MakePmid());
  auto maid(MakeMaid());
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeysWithPmid(pmid1));
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeysWithPmid(pmid2));
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeysWithMaid(maid));
  std::map<NodeId, asymm::PublicKey> key_map;
  key_map.insert(std::make_pair(node1.node_info.node_id, pmid1.public_key()));
  key_map.insert(std::make_pair(node2.node_info.node_id, pmid2.public_key()));
  key_map.insert(std::make_pair(node3.node_info.node_id, maid.public_key()));

  Functors functors1, functors2, functors3, functors4;
  Routing routing1(&pmid1);
  Routing routing2(&pmid2);
  Routing routing3(&maid);
  Routing routing4(&maid);

  functors1.network_status = [](const int&) {};  // NOLINT (Fraser)
  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
      LOG(kWarning) << "node_validation called for " << DebugId(node_id);
      auto itr(key_map.find(node_id));
      if (key_map.end() != itr)
        give_key((*itr).second);
    };

  functors1.message_received = [&] (const std::string& message, const NodeId&, const bool&,
                                    ReplyFunctor reply_functor) {
      reply_functor("response to " + message);
        LOG(kVerbose) << "Message received and replied to message !!";
    };

  functors4.network_status = functors2.network_status = functors3.network_status =
      functors1.network_status;
  functors4.request_public_key = functors2.request_public_key = functors3.request_public_key =
      functors1.request_public_key;
  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
           endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  auto a1 = std::async(std::launch::async,
      [&] { return routing1.ZeroStateJoin(functors1, endpoint1, endpoint2, node2.node_info);
       });
  auto a2 = std::async(std::launch::async,
      [&] { return routing2.ZeroStateJoin(functors2, endpoint2, endpoint1, node1.node_info);
      });
  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  std::once_flag join_set_promise_flag1;
  std::promise<bool> join_promise1;
  auto join_future1 = join_promise1.get_future();
  functors3.network_status = [&join_set_promise_flag1, &join_promise1](int result) {
    ASSERT_GE(result, kSuccess);
    if (result == NetworkStatus(true, 2)) {
      LOG(kVerbose) << "3rd node joined";
      std::call_once(join_set_promise_flag1,
                     [&join_promise1, &result] {
                       join_promise1.set_value(true);
                     });
    }
  };

  routing3.Join(functors3, std::vector<Endpoint>(1, endpoint1));
  EXPECT_EQ(join_future1.wait_for(std::chrono::seconds(10)), std::future_status::ready);
  std::once_flag join_set_promise_flag2;
  std::promise<bool> join_promise2;
  auto join_future2 = join_promise2.get_future();
  functors4.network_status = [&join_set_promise_flag2, &join_promise2](int result) {
    ASSERT_GE(result, kSuccess);
    if (result == NetworkStatus(true, 2)) {
      LOG(kVerbose) << "4th node joined";
      std::call_once(join_set_promise_flag2,
                     [&join_promise2, &result] {
                       join_promise2.set_value(true);
                     });
    }
  };

  routing4.Join(functors4, std::vector<Endpoint>(1, endpoint2));
  EXPECT_EQ(join_future2.wait_for(std::chrono::seconds(10)), std::future_status::ready);

  // Test SendDirect (1)
  std::string data_1("message 1 from client node");
  std::mutex mutex_1;
  std::condition_variable cond_var_1;
  ResponseFunctor response_functor_1 = [&data_1, &mutex_1, &cond_var_1] (std::string string) {
    std::unique_lock<std::mutex> lock_1;
    ASSERT_EQ("response to " + data_1, string);
    cond_var_1.notify_one();
  };
  routing3.SendDirect(node1.node_info.node_id, data_1, false, response_functor_1);
  std::unique_lock<std::mutex> lock_1(mutex_1);
  EXPECT_EQ(std::cv_status::no_timeout, cond_var_1.wait_for(lock_1, std::chrono::seconds(10)));

  // Test SendDirect (2)
  std::string data_2("message 2 from client node");
  std::mutex mutex_2;
  std::condition_variable cond_var_2;
  ResponseFunctor response_functor_2 = [&data_2, &mutex_2, &cond_var_2] (std::string string) {
    std::unique_lock<std::mutex> lock_2(mutex_2);
    ASSERT_EQ("response to " + data_2, string);
    cond_var_2.notify_one();
  };
  routing4.SendDirect(node1.node_info.node_id, data_2, false, response_functor_2);
  std::unique_lock<std::mutex> lock_2(mutex_2);
  EXPECT_EQ(std::cv_status::no_timeout, cond_var_2.wait_for(lock_2, std::chrono::seconds(10)));
}

TEST(APITest, BEH_API_NodeNetwork) {
  int min_join_status(8);  // TODO(Prakash): To decide
  std::vector<std::promise<bool>> join_promises(kNetworkSize - 2);
  std::vector<std::future<bool>> join_futures;
  std::deque<bool> promised;
  std::vector<NetworkStatusFunctor> status_vector;
  std::mutex mutex;
  Functors functors;

  std::map<NodeId, asymm::PublicKey> key_map;
  functors.network_status = [](const int&) {};  // NOLINT (Fraser)
  functors.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
      LOG(kWarning) << "node_validation called for " << DebugId(node_id);
      auto itr(key_map.find(node_id));
      if (key_map.end() != itr)
        give_key((*itr).second);
    };

  std::vector<NodeInfoAndPrivateKey> nodes;
  std::vector<std::shared_ptr<Routing>> routing_node;
  for (auto i(0); i != kNetworkSize; ++i) {
    auto pmid(MakePmid());
    NodeInfoAndPrivateKey node(MakeNodeInfoAndKeysWithPmid(pmid));
    nodes.push_back(node);
    key_map.insert(std::make_pair(node.node_info.node_id, pmid.public_key()));
    routing_node.push_back(std::make_shared<Routing>(&pmid));
  }

  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
           endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  auto a1 = std::async(std::launch::async, [&] {
      return routing_node[0]->ZeroStateJoin(functors, endpoint1, endpoint2,
                                             nodes[1].node_info);
       });
  auto a2 = std::async(std::launch::async, [&] {
      return routing_node[1]->ZeroStateJoin(functors, endpoint2, endpoint1,
                                            nodes[0].node_info);
      });

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  for (auto i(0); i != (kNetworkSize - 2); ++i) {
    join_futures.emplace_back(join_promises.at(i).get_future());
    promised.push_back(true);
    status_vector.emplace_back([=, &join_promises, &mutex, &promised](int result) {
        ASSERT_GE(result, kSuccess);
        if (result == NetworkStatus(false, std::min(i + 2, min_join_status))) {
          std::lock_guard<std::mutex> lock(mutex);
          if (promised.at(i)) {
            join_promises.at(i).set_value(true);
            promised.at(i) = false;
            LOG(kVerbose) << "node - " << i + 2 << "joined";
          }
        }
      });
  }

  for (auto i(0); i != (kNetworkSize - 2); ++i) {
    functors.network_status = status_vector.at(i);
    Endpoint endpoint((i % 2) ? endpoint1 : endpoint2);
    routing_node[i + 2]->Join(functors, std::vector<Endpoint>(1, endpoint));
    ASSERT_EQ(join_futures.at(i).wait_for(std::chrono::seconds(10)), std::future_status::ready);
    LOG(kVerbose) << "node ---------------------------- " << i + 2 << "joined";
  }
}

TEST(APITest, BEH_API_NodeNetworkWithClient) {
  int min_join_status(std::min(kServerCount, 8));
  std::vector<std::promise<bool>> join_promises(kNetworkSize);
  std::vector<std::future<bool>> join_futures;
  std::deque<bool> promised;
  std::vector<NetworkStatusFunctor> status_vector;
  std::mutex mutex;
  Functors functors;

  std::vector<NodeInfoAndPrivateKey> nodes;
  std::map<NodeId, asymm::PublicKey> key_map;
  std::vector<std::shared_ptr<Routing>> routing_node;
  int i(0);
  functors.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
      LOG(kWarning) << "node_validation called for " << DebugId(node_id);
      auto itr(key_map.find(node_id));
      if (key_map.end() != itr)
        give_key((*itr).second);
    };

  for (; i != kServerCount; ++i) {
    auto pmid(MakePmid());
    NodeInfoAndPrivateKey node(MakeNodeInfoAndKeysWithPmid(pmid));
    nodes.push_back(node);
    key_map.insert(std::make_pair(node.node_info.node_id, pmid.public_key()));
    routing_node.push_back(std::make_shared<Routing>(&pmid));
  }
  for (; i != kNetworkSize; ++i) {
    auto maid(MakeMaid());
    NodeInfoAndPrivateKey node(MakeNodeInfoAndKeysWithMaid(maid));
    nodes.push_back(node);
    key_map.insert(std::make_pair(node.node_info.node_id, maid.public_key()));
    routing_node.push_back(std::make_shared<Routing>(&maid));
  }

  functors.network_status = [](const int&) {};  // NOLINT (Fraser)


  functors.message_received = [&] (const std::string& message, const NodeId&, const bool&,
                                   ReplyFunctor reply_functor) {
     reply_functor("response to " + message);
      LOG(kVerbose) << "Message received and replied to message !!";
    };

  Functors client_functors;
  client_functors.network_status = [](const int&) {};  // NOLINT (Fraser)
  client_functors.request_public_key = functors.request_public_key;
  client_functors.message_received = [&] (const std::string &, const NodeId&, const bool&,
                                          ReplyFunctor /*reply_functor*/) {
      ASSERT_TRUE(false);  //  Client should not receive incoming message
    };
  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
           endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  auto a1 = std::async(std::launch::async, [&] {
      return routing_node[0]->ZeroStateJoin(functors, endpoint1, endpoint2,
                                            nodes[1].node_info);
      });
  auto a2 = std::async(std::launch::async, [&] {
      return routing_node[1]->ZeroStateJoin(functors, endpoint2, endpoint1,
                                            nodes[0].node_info);
      });

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  // Ignoring 2 zero state nodes
  promised.push_back(false);
  promised.push_back(false);
  status_vector.emplace_back([](int /*x*/) {});
  status_vector.emplace_back([](int /*x*/) {});
  std::promise<bool> promise1, promise2;
  join_futures.emplace_back(promise1.get_future());
  join_futures.emplace_back(promise2.get_future());

  // Joining remaining server & client nodes
  for (auto i(2); i != (kNetworkSize); ++i) {
    join_futures.emplace_back(join_promises.at(i).get_future());
    promised.push_back(true);
    status_vector.emplace_back([=, &join_promises, &mutex, &promised](int result) {
                                   ASSERT_GE(result, kSuccess);
                                   if (result == NetworkStatus((i < kServerCount)? false: true,
                                                               std::min(i, min_join_status))) {
                                     std::lock_guard<std::mutex> lock(mutex);
                                     if (promised.at(i)) {
                                       join_promises.at(i).set_value(true);
                                       promised.at(i) = false;
                                       LOG(kVerbose) << "node - " << i << "joined";
                                     }
                                   }
                                 });
  }

  for (auto i(2); i != (kNetworkSize); ++i) {
    functors.network_status = status_vector.at(i);
    routing_node[i]->Join(functors, std::vector<Endpoint>(1, endpoint1));
    ASSERT_EQ(join_futures.at(i).wait_for(std::chrono::seconds(10)), std::future_status::ready);
  }
}

TEST(APITest, BEH_API_SendGroup) {
  Parameters::default_send_timeout = boost::posix_time::seconds(200);
  const uint16_t kMessageCount(10);  // each vault will send kMessageCount message to other vaults
  const size_t kDataSize(512 * 1024);
  int min_join_status(std::min(kServerCount, 8));
  std::vector<std::promise<bool>> join_promises(kNetworkSize);
  std::vector<std::future<bool>> join_futures;
  std::deque<bool> promised;
  std::vector<NetworkStatusFunctor> status_vector;
  std::mutex mutex;
  Functors functors;

  std::vector<NodeInfoAndPrivateKey> nodes;
  std::map<NodeId, asymm::PublicKey> key_map;
  std::vector<std::shared_ptr<Routing>> routing_node;
  int i(0);
  functors.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
      LOG(kWarning) << "node_validation called for " << DebugId(node_id);
      auto itr(key_map.find(node_id));
      if (key_map.end() != itr)
        give_key((*itr).second);
    };

  for (; i != kServerCount; ++i) {
    auto pmid(MakePmid());
    NodeInfoAndPrivateKey node(MakeNodeInfoAndKeysWithPmid(pmid));
    nodes.push_back(node);
    key_map.insert(std::make_pair(node.node_info.node_id, pmid.public_key()));
    routing_node.push_back(std::make_shared<Routing>(&pmid));
  }

  functors.network_status = [](const int&) {};  // NOLINT (Alison)


  functors.message_received = [&] (const std::string& message, const NodeId&, const bool&,
                                   ReplyFunctor reply_functor) {
      reply_functor("response to " + message);
      LOG(kVerbose) << "Message received and replied to message !!";
    };

  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
           endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  auto a1 = std::async(std::launch::async, [&] {
      return routing_node[0]->ZeroStateJoin(functors, endpoint1, endpoint2,
                                            nodes[1].node_info);
    });
  auto a2 = std::async(std::launch::async, [&] {
      return routing_node[1]->ZeroStateJoin(functors, endpoint2, endpoint1,
                                            nodes[0].node_info);
    });

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  // Ignoring 2 zero state nodes
  promised.push_back(false);
  promised.push_back(false);
  status_vector.emplace_back([](int /*x*/) {});
  status_vector.emplace_back([](int /*x*/) {});
  std::promise<bool> promise1, promise2;
  join_futures.emplace_back(promise1.get_future());
  join_futures.emplace_back(promise2.get_future());

  // Joining remaining server nodes
  for (auto i(2); i != (kServerCount); ++i) {
    join_futures.emplace_back(join_promises.at(i).get_future());
    promised.push_back(true);
    status_vector.emplace_back([=, &join_promises, &mutex, &promised](int result) {
                                   ASSERT_GE(result, kSuccess);
                                   if (result == NetworkStatus(false,
                                                               std::min(i, min_join_status))) {
                                     std::lock_guard<std::mutex> lock(mutex);
                                     if (promised.at(i)) {
                                       join_promises.at(i).set_value(true);
                                       promised.at(i) = false;
                                       LOG(kVerbose) << "node - " << i << "joined";
                                     }
                                   }
                                 });
  }

  for (auto i(2); i != kServerCount; ++i) {
    functors.network_status = status_vector.at(i);
    routing_node[i]->Join(functors, std::vector<Endpoint>(1, endpoint1));
    EXPECT_EQ(join_futures.at(i).wait_for(std::chrono::seconds(10)), std::future_status::ready);
  }
  std::string data(RandomAlphaNumericString(kDataSize));
  // Call SendGroup repeatedly - measure duration to allow performance comparison
  boost::progress_timer t;

  std::mutex send_mutex;
  std::vector<std::promise<bool>> send_promises(kServerCount * kMessageCount);
  std::vector<uint16_t> send_counts(kServerCount * kMessageCount, 0);
  std::vector<std::future<bool>> send_futures;
  for (uint16_t i(0); i < send_promises.size(); ++i)
    send_futures.emplace_back(send_promises.at(i).get_future());
  for (uint16_t i(0); i < kServerCount; ++i) {
    NodeId dest_id(routing_node[i]->kNodeId());
    uint16_t count(0);
    while (count < kMessageCount) {
      uint16_t message_index(i * kServerCount + count);
      ResponseFunctor response_functor =
          [&send_mutex, &send_promises, &send_counts, &data, message_index] (std::string string) {
         std::unique_lock<std::mutex> lock(send_mutex);
         EXPECT_EQ("response to " + data, string) << "for message_index " << message_index;
         if (send_counts.at(message_index) >= Parameters::node_group_size)
           return;
         if (string != "response to " + data) {
           send_counts.at(message_index) = Parameters::node_group_size;
           send_promises.at(message_index).set_value(false);
         } else {
           send_counts.at(message_index) += 1;
           if (send_counts.at(message_index) == Parameters::node_group_size)
             send_promises.at(message_index).set_value(true);
         }
      };

      routing_node[i]->SendGroup(dest_id, data, false, response_functor);
      ++count;
    }
  }

  while (!send_futures.empty()) {
    send_futures.erase(std::remove_if(send_futures.begin(), send_futures.end(),
        [&data](std::future<bool>& future_bool)->bool {
            if (IsReady(future_bool)) {
                EXPECT_TRUE(future_bool.get());
               return true;
             } else  {
               return false;
             };
        }), send_futures.end());
    std::this_thread::yield();
  }
  auto time_taken(t.elapsed());
  std::cout << "\n Time taken: " << time_taken
            << "\n Total number of request messages :" << (kMessageCount * kServerCount)
            << "\n Total number of response messages :" << (kMessageCount * kServerCount * 4)
            << "\n Message size : " << (kDataSize / 1024) << "kB \n";
  Parameters::default_send_timeout = boost::posix_time::seconds(10);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
