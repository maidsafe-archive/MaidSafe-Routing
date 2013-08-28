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

#include <algorithm>
#include <atomic>
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
  Routing routing1(pmid1);
  Routing routing2(pmid2);
  Routing routing3(pmid3);

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
  std::promise<void> join_promise;
  auto join_future = join_promise.get_future();

  functors3.network_status = [&flag, &join_promise] (int result) {
    if (result == NetworkStatus(false, 2)) {
        std::call_once(flag, [&join_promise]() { join_promise.set_value(); }
       );
    }
  };

  routing3.Join(functors3, std::vector<Endpoint>(1, endpoint2));
  ASSERT_EQ(join_future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
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
  Routing routing1(pmid1);
  Routing routing2(pmid2);
  Routing routing3(pmid3);
  Routing routing4(pmid3);

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
  std::promise<void> join_promise;
  auto join_future = join_promise.get_future();
  functors3.network_status = [&flag, &join_promise](int result) {
    if (result == NetworkStatus(false, 2)) {
        std::call_once(flag, [&join_promise]() { join_promise.set_value(); }
        );
    }
  };

  routing3.Join(functors3, std::vector<Endpoint>(1, endpoint2));
  ASSERT_EQ(join_future.wait_for(std::chrono::seconds(5)), std::future_status::ready);

  std::atomic<int> final_result;
  functors4.network_status = [&final_result](int result) {
     final_result = result;
  };
  routing4.Join(functors4, std::vector<Endpoint>(1, endpoint2));
  Sleep(std::chrono::seconds(5));
  EXPECT_LT(final_result, 0);
  LOG(kInfo) << "done!!!";
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
  Routing routing1(pmid1);
  Routing routing2(pmid2);
  Routing routing3(pmid3);

  functors1.network_status = [](const int&) {};  // NOLINT (Fraser)
  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
      LOG(kWarning) << "node_validation called for " << DebugId(node_id);
      auto itr(key_map.find(node_id));
      if (key_map.end() != itr)
        give_key((*itr).second);
    };

  functors1.message_and_caching.message_received = [&] (const std::string& message, const bool&,
      ReplyFunctor reply_functor) {
      reply_functor("response to " + message);
      LOG(kVerbose) << "Message received and replied to message !!";
    };

  functors2.network_status = functors3.network_status = functors1.network_status;
  functors2.request_public_key = functors3.request_public_key = functors1.request_public_key;
  functors2.message_and_caching.message_received =
      functors3.message_and_caching.message_received =
          functors1.message_and_caching.message_received;

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
  std::once_flag flag;
  std::promise<void> response_promise;
  auto response_future = response_promise.get_future();
  ResponseFunctor response_functor = [&data, &flag, &response_promise] (std::string string) {
    EXPECT_EQ("response to " + data, string);
    std::call_once(flag, [&response_promise]() { response_promise.set_value(); });
    LOG(kVerbose) << "ResponseFunctor - end";
  };
  routing3.SendDirect(node3.node_info.node_id, data, false, response_functor);
  ASSERT_EQ(response_future.wait_for(std::chrono::seconds(10)), std::future_status::ready);
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

  Functors functors1, functors2, functors3;
  Routing routing1(pmid1);
  Routing routing2(pmid2);
  Routing routing3(maid);

  functors1.network_status = [](const int&) {};  // NOLINT (Fraser)
  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
      LOG(kWarning) << "node_validation called for " << DebugId(node_id);
      auto itr(key_map.find(node_id));
      if (key_map.end() != itr)
        give_key((*itr).second);
    };

  functors1.message_and_caching.message_received = [&] (const std::string& message, const bool&,
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

TEST(APITest, BEH_API_NonMutatingClientNode) {
  auto pmid1(MakePmid()), pmid2(MakePmid());
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeysWithPmid(pmid1));
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeysWithPmid(pmid2));
  std::map<NodeId, asymm::PublicKey> key_map;
  key_map.insert(std::make_pair(node1.node_info.node_id, pmid1.public_key()));
  key_map.insert(std::make_pair(node2.node_info.node_id, pmid2.public_key()));

  Functors functors1, functors2, functors3;
  Routing routing1(pmid1);
  Routing routing2(pmid2);
  Routing routing3((NodeId(NodeId::kRandomId)));

  functors1.network_status = [](const int&) {};  // NOLINT (Fraser)
  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
      LOG(kWarning) << "node_validation called for " << DebugId(node_id);
      auto itr(key_map.find(node_id));
      if (key_map.end() != itr)
        give_key((*itr).second);
    };

  functors1.message_and_caching.message_received = [&] (const std::string& message, const bool&,
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

  Functors functors1, functors2, functors3, functors4;
  Routing routing1(pmid1);
  Routing routing2(pmid2);
  Routing routing3(maid);
  Routing routing4(maid);

  functors1.network_status = [](const int&) {};  // NOLINT (Fraser)
  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
      LOG(kWarning) << "node_validation called for " << DebugId(node_id);
      auto itr(key_map.find(node_id));
      if (key_map.end() != itr)
        give_key((*itr).second);
    };

  functors1.message_and_caching.message_received = [&] (const std::string& message, const bool&,
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
  Functors functors;

  std::map<NodeId, asymm::PublicKey> key_map;
  functors.network_status = [](const int&) {}; // NOLINT (Fraser)
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
    routing_node.push_back(std::make_shared<Routing>(pmid));
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
    std::shared_ptr<std::promise<bool>> join_promise_ptr(std::make_shared<std::promise<bool>>());
    std::shared_ptr<bool> promised(std::make_shared<bool>(false));
    std::future<bool> join_future((*join_promise_ptr).get_future());
    functors.network_status = [i, min_join_status, join_promise_ptr, promised](int result) {
         if (result == NetworkStatus(false, std::min(i + 2, min_join_status))) {
           if (!(*promised)) {
             (*join_promise_ptr).set_value(true);
             (*promised) = true;
           }
           LOG(kVerbose) << "node - "<< i + 2 << "joined";
        }
      };
    Endpoint endpoint((i % 2) ? endpoint1 : endpoint2);
    routing_node[i + 2]->Join(functors, std::vector<Endpoint>(1, endpoint));
    ASSERT_EQ(join_future.wait_for(std::chrono::seconds(10)), std::future_status::ready);
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
    routing_node.push_back(std::make_shared<Routing>(pmid));
  }
  for (; i != kNetworkSize; ++i) {
    auto maid(MakeMaid());
    NodeInfoAndPrivateKey node(MakeNodeInfoAndKeysWithMaid(maid));
    nodes.push_back(node);
    routing_node.push_back(std::make_shared<Routing>(maid));
  }

  functors.network_status = [](const int&) {};  // NOLINT (Fraser)


  functors.message_and_caching.message_received = [&] (const std::string& message, const bool&,
     ReplyFunctor reply_functor) {
     reply_functor("response to " + message);
      LOG(kVerbose) << "Message received and replied to message !!";
    };

  Functors client_functors;
  client_functors.network_status = [](const int&) {};  // NOLINT (Fraser)
  client_functors.request_public_key = functors.request_public_key;
  client_functors.message_and_caching.message_received = [&] (const std::string &, const bool&,
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
                                   if (result == NetworkStatus((i >= kServerCount),
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
  Parameters::default_response_timeout = std::chrono::seconds(200);
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
    routing_node.push_back(std::make_shared<Routing>(pmid));
  }

  functors.network_status = [](const int&) {};


  functors.message_and_caching.message_received = [&] (const std::string& message, const bool&,
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
  bool result(false);
  for (uint16_t i(0); i < kServerCount; ++i) {
    NodeId dest_id(routing_node[i]->kNodeId());
    uint16_t count(0);
    while (count < kMessageCount) {
      uint16_t message_index(i * kServerCount + count);
      ResponseFunctor response_functor =
          [&send_mutex, &send_promises, &send_counts, &data, message_index, &result]
          (std::string string) {
         std::unique_lock<std::mutex> lock(send_mutex);
         EXPECT_EQ("response to " + data, string) << "for message_index " << message_index;
         if (send_counts.at(message_index) >= Parameters::node_group_size)
           return;
         if (string != "response to " + data) {
           result = false;
         } else {
           result = true;
         }
         send_counts.at(message_index) += 1;
         if (send_counts.at(message_index) == Parameters::node_group_size)
           send_promises.at(message_index).set_value(result);
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
  Parameters::default_response_timeout = std::chrono::seconds(10);
}

TEST(APITest, BEH_API_PartiallyJoinedSend) {
  // N.B. 5sec sleep in functors3.request_public_key causes delay in joining, giving opportunity for
  // routing3's impl to use PartiallyJoinedSend when SendDirect is called
  auto pmid1(MakePmid()), pmid2(MakePmid()), pmid3(MakePmid());
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeysWithPmid(pmid1));
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeysWithPmid(pmid2));
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeysWithPmid(pmid3));
  std::map<NodeId, asymm::PublicKey> key_map;
  key_map.insert(std::make_pair(node1.node_info.node_id, pmid1.public_key()));
  key_map.insert(std::make_pair(node2.node_info.node_id, pmid2.public_key()));
  key_map.insert(std::make_pair(node3.node_info.node_id, pmid3.public_key()));

  Functors functors1, functors2, functors3;
  Routing routing1(pmid1);
  Routing routing2(pmid2);
  Routing routing3(pmid3);

  functors1.network_status = [](const int&) {};  // NOLINT (Fraser)
  functors1.message_and_caching.message_received = [&](const std::string& message, const bool&,
      ReplyFunctor reply_functor) {
      reply_functor("response to " + message);
      LOG(kVerbose) << "Message received and replied to message !!";
    };
  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
      LOG(kWarning) << "node_validation called for " << DebugId(node_id);
      auto itr(key_map.find(node_id));
      if (key_map.end() != itr)
        give_key((*itr).second);
    };

  functors2.network_status = functors3.network_status = functors1.network_status;
  functors2.message_and_caching.message_received = functors1.message_and_caching.message_received;
  functors2.request_public_key = functors1.request_public_key;
  functors3.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
      Sleep(std::chrono::seconds(5));
      LOG(kWarning) << "node_validation called for " << DebugId(node_id);
      auto itr(key_map.find(node_id));
      if (key_map.end() != itr)
        give_key((*itr).second);
    };
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
  std::once_flag join_flag;
  std::promise<void> join_promise;
  auto join_future = join_promise.get_future();

  functors3.network_status = [&join_flag, &join_promise] (int result) {
      if (result == NetworkStatus(false, 2)) {
        std::call_once(join_flag, [&join_promise]() { join_promise.set_value(); });
      }
    };

  routing3.Join(functors3, std::vector<Endpoint>(1, endpoint2));

  // Test PartiallyJoinedSend
  std::string data("message from my node");
  std::once_flag flag;
  std::promise<void> response_promise;
  auto response_future = response_promise.get_future();
  std::atomic<int> count(0);
  ResponseFunctor response_functor = [&data, &flag, &response_promise, &count] (std::string str) {
      EXPECT_EQ("response to " + data, str);
      ++count;
      if (count == 2)
        std::call_once(flag, [&response_promise]() { response_promise.set_value(); });
      LOG(kVerbose) << "ResponseFunctor - end";
    };
  routing3.SendDirect(node1.node_info.node_id, data, false, response_functor);
  routing3.SendDirect(node2.node_info.node_id, data, false, response_functor);
  EXPECT_EQ(response_future.wait_for(std::chrono::seconds(10)), std::future_status::ready);

  EXPECT_EQ(join_future.wait_for(std::chrono::seconds(20)), std::future_status::ready);
  EXPECT_EQ(count, 2);
}

TEST(APITest, BEH_API_TypedMessageSend) {
  auto pmid1(MakePmid()), pmid2(MakePmid()), pmid3(MakePmid());
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeysWithPmid(pmid1));
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeysWithPmid(pmid2));
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeysWithPmid(pmid3));
  std::map<NodeId, asymm::PublicKey> key_map;
  key_map.insert(std::make_pair(node1.node_info.node_id, pmid1.public_key()));
  key_map.insert(std::make_pair(node2.node_info.node_id, pmid2.public_key()));
  key_map.insert(std::make_pair(node3.node_info.node_id, pmid3.public_key()));

  Functors functors1, functors2, functors3;
  std::promise<bool> single_to_single_promise, single_to_group_promise, group_to_single_promise,
                     group_to_group_promise;
  size_t responses(0);
  std::mutex mutex;
  Routing routing1(pmid1);
  Routing routing2(pmid2);
  Routing routing3(pmid3);

  functors1.network_status = [](const int&) {};  // NOLINT (Fraser)
  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
      LOG(kWarning) << "node_validation called for " << DebugId(node_id);
      auto itr(key_map.find(node_id));
      if (key_map.end() != itr)
        give_key((*itr).second);
    };

  functors1.typed_message_and_caching.group_to_group.message_received =
      [&] (const GroupToGroupMessage& /*g2g*/) {
        LOG(kVerbose) << "group to group message received!!";
        std::lock_guard<std::mutex> lock(mutex);
        if (++responses == 3)
           group_to_group_promise.set_value(true);
      };

  functors1.typed_message_and_caching.group_to_single.message_received =
      [&] (const GroupToSingleMessage& /*g2s*/) {
        LOG(kVerbose) << "group to single message received!!";
        group_to_single_promise.set_value(true);
      };

  functors1.typed_message_and_caching.single_to_group.message_received =
      [&] (const SingleToGroupMessage& /*s2g*/) {
        LOG(kVerbose) << "single to group message received!!";
        std::lock_guard<std::mutex> lock(mutex);
        if (++responses == 3)
          single_to_group_promise.set_value(true);
      };

  functors1.typed_message_and_caching.single_to_single.message_received =
      [&] (const SingleToSingleMessage& /*s2s*/) {
        LOG(kVerbose) << "single to single message received!!";
        single_to_single_promise.set_value(true);
      };

  functors2.network_status = functors3.network_status = functors1.network_status;
  functors2.request_public_key = functors3.request_public_key = functors1.request_public_key;
  functors2.typed_message_and_caching.group_to_group.message_received =
      functors3.typed_message_and_caching.group_to_group.message_received =
          functors1.typed_message_and_caching.group_to_group.message_received;

  functors2.typed_message_and_caching.group_to_single.message_received =
      functors3.typed_message_and_caching.group_to_single.message_received =
          functors1.typed_message_and_caching.group_to_single.message_received;

  functors2.typed_message_and_caching.single_to_group.message_received =
      functors3.typed_message_and_caching.single_to_group.message_received =
          functors1.typed_message_and_caching.single_to_group.message_received;

  functors2.typed_message_and_caching.single_to_single.message_received =
      functors3.typed_message_and_caching.single_to_single.message_received =
          functors1.typed_message_and_caching.single_to_single.message_received;

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


  {  // Test Group To Group
    GroupToGroupMessage group_to_group_message;
    GroupSource group_source;
    group_source.sender_id = SingleId(routing3.kNodeId());
    group_source.group_id = GroupId(GenerateUniqueRandomId(routing3.kNodeId(), 30));
    group_to_group_message.contents = "Dummy content for test puepose";
    group_to_group_message.receiver  = GroupId(GenerateUniqueRandomId(routing1.kNodeId(), 30));
    group_to_group_message.sender = group_source;
    routing3.Send(group_to_group_message);
    auto group_to_group_future(group_to_group_promise.get_future());
    ASSERT_EQ(group_to_group_future.wait_for(std::chrono::seconds(10)), std::future_status::ready);
  }

  {  // Test Single To Single
    SingleToSingleMessage single_to_single_message;
    single_to_single_message.receiver = SingleId(routing1.kNodeId());
    single_to_single_message.sender = SingleSource(SingleId(routing3.kNodeId()));
    single_to_single_message.contents = "Dummy content for test puepose";
    routing3.Send(single_to_single_message);
    auto single_to_single_future(single_to_single_promise.get_future());
    ASSERT_EQ(single_to_single_future.wait_for(std::chrono::seconds(10)),
              std::future_status::ready);
  }

  {  // Test Group To Single
    GroupToSingleMessage group_to_single_message;
    GroupSource group_source;
    group_source.sender_id = SingleId(routing3.kNodeId());
    group_source.group_id = GroupId(GenerateUniqueRandomId(routing3.kNodeId(), 30));
    group_to_single_message.sender = group_source;
    group_to_single_message.receiver = SingleId(routing1.kNodeId());
    group_to_single_message.contents = "Dummy content for test puepose";
    routing3.Send(group_to_single_message);
    auto group_to_single_future(group_to_single_promise.get_future());
    ASSERT_EQ(group_to_single_future.wait_for(std::chrono::seconds(10)), std::future_status::ready);
  }

  {  //  Test Single To Group
    responses = 0;
    SingleToGroupMessage single_to_group_message;
    single_to_group_message.sender = SingleSource(SingleId(routing3.kNodeId()));
    single_to_group_message.receiver = GroupId(GenerateUniqueRandomId(routing1.kNodeId(), 30));
    routing3.Send(single_to_group_message);
    auto single_to_group_future(single_to_group_promise.get_future());
    ASSERT_EQ(single_to_group_future.wait_for(std::chrono::seconds(10)), std::future_status::ready);
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
