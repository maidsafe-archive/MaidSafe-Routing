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
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
#include "boost/thread/future.hpp"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/parameters.h"

#include "maidsafe/passport/passport.h"

#include "maidsafe/routing/bootstrap_file_operations.h"
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
MessageReceivedFunctor no_ops_message_received_functor = [](const std::string&,
                                                            ReplyFunctor) {};  // NOLINT
}  // anonymous namespace

TEST(APITest, BEH_API_ZeroState) {
  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
    endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  ScopedBootstrapFile bootstrap_file({ endpoint1, endpoint2 });

  auto pmid1(passport::CreatePmidAndSigner().first), pmid2(passport::CreatePmidAndSigner().first),
    pmid3(passport::CreatePmidAndSigner().first);
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeysWithPmid(pmid1));
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeysWithPmid(pmid2));
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeysWithPmid(pmid3));
  std::map<NodeId, asymm::PublicKey> key_map;
  key_map.insert(std::make_pair(node1.node_info.id, pmid1.public_key()));
  key_map.insert(std::make_pair(node2.node_info.id, pmid2.public_key()));
  key_map.insert(std::make_pair(node3.node_info.id, pmid3.public_key()));

  Functors functors1, functors2, functors3;
  Routing routing1(pmid1);
  Routing routing2(pmid2);
  Routing routing3(pmid3);

  functors1.network_status = [](int) {};  // NOLINT (Fraser)
  functors1.message_and_caching.message_received = no_ops_message_received_functor;
  functors3 = functors2 = functors1;

  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
    LOG(kWarning) << "node_validation called for " << DebugId(node_id);
    auto itr(key_map.find(node_id));
    if (key_map.end() != itr)
      give_key((*itr).second);
  };

  functors2.network_status = functors3.network_status = functors1.network_status;
  functors2.request_public_key = functors3.request_public_key = functors1.request_public_key;

  auto a1 = boost::async(boost::launch::async, [&] {
    return routing1.ZeroStateJoin(functors1, endpoint1, endpoint2, node2.node_info);
  });
  auto a2 = boost::async(boost::launch::async, [&] {
    return routing2.ZeroStateJoin(functors2, endpoint2, endpoint1, node1.node_info);
  });
  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  std::once_flag flag;
  boost::promise<void> join_promise;
  auto join_future = join_promise.get_future();

  functors3.network_status = [&flag, &join_promise](int result) {
    if (result == NetworkStatus(false, 2)) {
      std::call_once(flag, [&join_promise]() { join_promise.set_value(); });
    }
  };

  routing3.Join(functors3);
  ASSERT_EQ(join_future.wait_for(boost::chrono::seconds(5)), boost::future_status::ready);
  LOG(kInfo) << "done!!!";
}

TEST(APITest, BEH_API_GetPublicKeyFailure) {
  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
    endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  ScopedBootstrapFile bootstrap_file({ endpoint1, endpoint2 });

  auto pmid1(passport::CreatePmidAndSigner().first), pmid2(passport::CreatePmidAndSigner().first),
    pmid3(passport::CreatePmidAndSigner().first);
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeysWithPmid(pmid1));
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeysWithPmid(pmid2));
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeysWithPmid(pmid3));
  std::map<NodeId, boost::optional<asymm::PublicKey>> key_map;
  key_map.insert(std::make_pair(node1.node_info.id,
                                boost::optional<asymm::PublicKey>(pmid1.public_key())));
  key_map.insert(std::make_pair(node2.node_info.id,
                                boost::optional<asymm::PublicKey>(pmid2.public_key())));
  key_map.insert(std::make_pair(node3.node_info.id,
                                boost::optional<asymm::PublicKey>(pmid3.public_key())));

  Functors functors1, functors2, functors3;
  Routing routing1(pmid1);
  Routing routing2(pmid2);
  Routing routing3(pmid3);

  functors1.network_status = [](int) {};  // NOLINT (Fraser)
  functors1.message_and_caching.message_received = no_ops_message_received_functor;
  functors3 = functors2 = functors1;

  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
    LOG(kInfo) << "node_validation called for " << node_id;
    auto itr(key_map.find(node_id));
    if (node_id == node3.node_info.id) {
      give_key(boost::optional<asymm::PublicKey>());
      return;
    }
    if (key_map.end() != itr)
      give_key((*itr).second);
  };

  functors2.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
    LOG(kInfo) << "node_validation called for " << node_id;
    auto itr(key_map.find(node_id));
    if (key_map.end() != itr)
      give_key((*itr).second);
  };

  functors2.network_status = functors3.network_status = functors1.network_status;
  functors3.request_public_key = functors2.request_public_key;

  auto a1 = boost::async(boost::launch::async, [&] {
    return routing1.ZeroStateJoin(functors1, endpoint1, endpoint2, node2.node_info);
  });
  auto a2 = boost::async(boost::launch::async, [&] {
    return routing2.ZeroStateJoin(functors2, endpoint2, endpoint1, node1.node_info);
  });
  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  std::once_flag flag;
  boost::promise<void> join_promise;
  auto join_future = join_promise.get_future();

  functors3.network_status = [&flag, &join_promise](int result) {
    if (result == NetworkStatus(false, 2)) {
      std::call_once(flag, [&join_promise]() { join_promise.set_value(); });
    }
  };

  routing3.Join(functors3);
  ASSERT_NE(join_future.wait_for(boost::chrono::seconds(5)), boost::future_status::ready);
  LOG(kInfo) << "done!!!";
}

TEST(APITest, DISABLED_BEH_API_ZeroStateWithDuplicateNode) {
  rudp::Parameters::bootstrap_connection_lifespan = boost::posix_time::seconds(5);
  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
      endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  ScopedBootstrapFile bootstrap_file({endpoint1, endpoint2});

  auto pmid1(passport::CreatePmidAndSigner().first), pmid2(passport::CreatePmidAndSigner().first),
      pmid3(passport::CreatePmidAndSigner().first);
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeysWithPmid(pmid1));
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeysWithPmid(pmid2));
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeysWithPmid(pmid3));
  NodeInfoAndPrivateKey node4(MakeNodeInfoAndKeysWithPmid(pmid3));
  std::map<NodeId, asymm::PublicKey> key_map;
  key_map.insert(std::make_pair(node1.node_info.id, pmid1.public_key()));
  key_map.insert(std::make_pair(node2.node_info.id, pmid2.public_key()));
  key_map.insert(std::make_pair(node3.node_info.id, pmid3.public_key()));

  Functors functors1, functors2, functors3, functors4;
  Routing routing1(pmid1);
  Routing routing2(pmid2);
  Routing routing3(pmid3);
  Routing routing4(pmid3);
  functors1.message_and_caching.message_received = no_ops_message_received_functor;
  functors4 = functors3 = functors2 = functors1;
  functors1.network_status = [](int) {};  // NOLINT (Fraser)
  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
    LOG(kWarning) << "node_validation called for " << DebugId(node_id);
    auto itr(key_map.find(node_id));
    if (key_map.end() != itr)
      give_key((*itr).second);
  };

  functors2.network_status = functors3.network_status = functors4.network_status =
      functors1.network_status;
  functors2.request_public_key = functors3.request_public_key = functors1.request_public_key;
  auto a1 = boost::async(boost::launch::async, [&] {
    return routing1.ZeroStateJoin(functors1, endpoint1, endpoint2, node2.node_info);
  });
  auto a2 = boost::async(boost::launch::async, [&] {
    return routing2.ZeroStateJoin(functors2, endpoint2, endpoint1, node1.node_info);
  });
  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  std::once_flag flag;
  boost::promise<void> join_promise;
  auto join_future = join_promise.get_future();
  functors3.network_status = [&flag, &join_promise](int result) {
    if (result == NetworkStatus(false, 2)) {
      std::call_once(flag, [&join_promise]() { join_promise.set_value(); });
    }
  };

  routing3.Join(functors3);
  ASSERT_EQ(join_future.wait_for(boost::chrono::seconds(5)), boost::future_status::ready);

  std::atomic<int> final_result;
  functors4.network_status = [&final_result](int result) { final_result = result; };  // NOLINT
  routing4.Join(functors4);
  Sleep(std::chrono::seconds(5));
  EXPECT_LT(final_result, 0);
  LOG(kInfo) << "done!!!";
  rudp::Parameters::bootstrap_connection_lifespan = boost::posix_time::minutes(10);
}

TEST(APITest, BEH_API_SendToSelf) {
  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
      endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  ScopedBootstrapFile bootstrap_file({endpoint1, endpoint2});


  auto pmid1(passport::CreatePmidAndSigner().first), pmid2(passport::CreatePmidAndSigner().first),
      pmid3(passport::CreatePmidAndSigner().first);
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeysWithPmid(pmid1));
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeysWithPmid(pmid2));
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeysWithPmid(pmid3));
  std::map<NodeId, asymm::PublicKey> key_map;
  key_map.insert(std::make_pair(node1.node_info.id, pmid1.public_key()));
  key_map.insert(std::make_pair(node2.node_info.id, pmid2.public_key()));
  key_map.insert(std::make_pair(node3.node_info.id, pmid3.public_key()));

  Functors functors1, functors2, functors3;
  Routing routing1(pmid1);
  Routing routing2(pmid2);
  Routing routing3(pmid3);

  functors1.network_status = [](int) {};  // NOLINT (Fraser)
  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
    LOG(kWarning) << "node_validation called for " << DebugId(node_id);
    auto itr(key_map.find(node_id));
    if (key_map.end() != itr)
      give_key((*itr).second);
  };

  functors1.message_and_caching.message_received = [&](const std::string& message,
                                                       ReplyFunctor reply_functor) {
    reply_functor("response to " + message);
    LOG(kVerbose) << "Message received and replied to message !!";
  };

  functors2.network_status = functors3.network_status = functors1.network_status;
  functors2.request_public_key = functors3.request_public_key = functors1.request_public_key;
  functors2.message_and_caching.message_received = functors3.message_and_caching.message_received =
      functors1.message_and_caching.message_received;

  auto a1 = boost::async(boost::launch::async, [&] {
    return routing1.ZeroStateJoin(functors1, endpoint1, endpoint2, node2.node_info);
  });
  auto a2 = boost::async(boost::launch::async, [&] {
    return routing2.ZeroStateJoin(functors2, endpoint2, endpoint1, node1.node_info);
  });

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  std::once_flag join_set_promise_flag;
  boost::promise<bool> join_promise;
  auto join_future = join_promise.get_future();
  functors3.network_status = [&join_set_promise_flag, &join_promise](int result) {
    ASSERT_GE(result, kSuccess);
    if (result == NetworkStatus(false, 2)) {
      std::call_once(join_set_promise_flag, [&join_promise, &result] {
        LOG(kVerbose) << "3rd node joined";
        join_promise.set_value(true);
      });
    }
  };

  routing3.Join(functors3);
  EXPECT_EQ(join_future.wait_for(boost::chrono::seconds(10)), boost::future_status::ready);

  // Test SendDirect
  std::string data("message from my node");
  std::once_flag flag;
  boost::promise<void> response_promise;
  auto response_future = response_promise.get_future();
  ResponseFunctor response_functor = [&data, &flag, &response_promise](std::string string) {
    EXPECT_EQ("response to " + data, string);
    std::call_once(flag, [&response_promise]() { response_promise.set_value(); });
    LOG(kVerbose) << "ResponseFunctor - end";
  };
  routing3.SendDirect(node3.node_info.id, data, false, response_functor);
  ASSERT_EQ(response_future.wait_for(boost::chrono::seconds(10)), boost::future_status::ready);
}

TEST(APITest, BEH_API_ClientNode) {
  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
      endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  ScopedBootstrapFile bootstrap_file({endpoint1, endpoint2});

  auto pmid1(passport::CreatePmidAndSigner().first), pmid2(passport::CreatePmidAndSigner().first);
  auto maid(passport::CreateMaidAndSigner().first);
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeysWithPmid(pmid1));
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeysWithPmid(pmid2));
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeysWithMaid(maid));
  std::map<NodeId, asymm::PublicKey> key_map;
  key_map.insert(std::make_pair(node1.node_info.id, pmid1.public_key()));
  key_map.insert(std::make_pair(node2.node_info.id, pmid2.public_key()));

  Functors functors1, functors2, functors3;
  Routing routing1(pmid1);
  Routing routing2(pmid2);
  Routing routing3(maid);
  functors1.message_and_caching.message_received = no_ops_message_received_functor;
  functors3 = functors2 = functors1;
  functors1.network_status = [](int) {};  // NOLINT (Fraser)
  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
    LOG(kWarning) << "node_validation called for " << DebugId(node_id);
    auto itr(key_map.find(node_id));
    if (key_map.end() != itr)
      give_key((*itr).second);
  };

  functors1.message_and_caching.message_received = [&](const std::string& message,
                                                       ReplyFunctor reply_functor) {
    reply_functor("response to " + message);
    LOG(kVerbose) << "Message received and replied to message !!";
  };

  functors2.network_status = functors3.network_status = functors1.network_status;
  functors2.request_public_key = functors3.request_public_key = functors1.request_public_key;
  auto a1 = boost::async(boost::launch::async, [&] {
    return routing1.ZeroStateJoin(functors1, endpoint1, endpoint2, node2.node_info);
  });
  auto a2 = boost::async(boost::launch::async, [&] {
    return routing2.ZeroStateJoin(functors2, endpoint2, endpoint1, node1.node_info);
  });
  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  std::once_flag join_set_promise_flag;
  boost::promise<bool> join_promise;
  auto join_future = join_promise.get_future();
  functors3.network_status = [&join_set_promise_flag, &join_promise](int result) {
    ASSERT_GE(result, kSuccess);
    if (result == NetworkStatus(true, 2)) {
      LOG(kVerbose) << "3rd node joined";
      std::call_once(join_set_promise_flag,
                     [&join_promise, &result] { join_promise.set_value(true); });
    }
  };

  routing3.Join(functors3);
  EXPECT_EQ(join_future.wait_for(boost::chrono::seconds(10)), boost::future_status::ready);

  // Test SendDirect
  std::mutex mutex;
  std::condition_variable cond_var;
  std::string data(RandomAlphaNumericString(512 * 1024));
  ResponseFunctor response_functor = [&cond_var, &mutex, &data](std::string string) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      ASSERT_EQ(("response to " + data), string);
    }
    cond_var.notify_one();
  };
  routing3.SendDirect(node1.node_info.id, data, false, response_functor);
  std::unique_lock<std::mutex> lock(mutex);
  EXPECT_EQ(std::cv_status::no_timeout, cond_var.wait_for(lock, std::chrono::seconds(10)));
}

TEST(APITest, BEH_API_NonMutatingClientNode) {
  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
      endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  ScopedBootstrapFile bootstrap_file({endpoint1, endpoint2});

  auto pmid1(passport::CreatePmidAndSigner().first), pmid2(passport::CreatePmidAndSigner().first);
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeysWithPmid(pmid1));
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeysWithPmid(pmid2));
  std::map<NodeId, asymm::PublicKey> key_map;
  key_map.insert(std::make_pair(node1.node_info.id, pmid1.public_key()));
  key_map.insert(std::make_pair(node2.node_info.id, pmid2.public_key()));

  Functors functors1, functors2, functors3;
  Routing routing1(pmid1);
  Routing routing2(pmid2);
  Routing routing3{};
  functors1.message_and_caching.message_received = no_ops_message_received_functor;
  functors3 = functors2 = functors1;

  functors1.network_status = [](int) {};  // NOLINT (Fraser)
  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
    LOG(kWarning) << "node_validation called for " << DebugId(node_id);
    auto itr(key_map.find(node_id));
    if (key_map.end() != itr)
      give_key((*itr).second);
  };

  functors1.message_and_caching.message_received = [&](const std::string& message,
                                                       ReplyFunctor reply_functor) {
    reply_functor("response to " + message);
    LOG(kVerbose) << "Message received and replied to message !!";
  };

  functors2.network_status = functors3.network_status = functors1.network_status;
  functors2.request_public_key = functors3.request_public_key = functors1.request_public_key;
  auto a1 = boost::async(boost::launch::async, [&] {
    return routing1.ZeroStateJoin(functors1, endpoint1, endpoint2, node2.node_info);
  });
  auto a2 = boost::async(boost::launch::async, [&] {
    return routing2.ZeroStateJoin(functors2, endpoint2, endpoint1, node1.node_info);
  });
  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  std::once_flag join_set_promise_flag;
  boost::promise<bool> join_promise;
  auto join_future = join_promise.get_future();
  functors3.network_status = [&join_set_promise_flag, &join_promise](int result) {
    ASSERT_GE(result, kSuccess);
    if (result == NetworkStatus(true, 2)) {
      LOG(kVerbose) << "3rd node joined";
      std::call_once(join_set_promise_flag,
                     [&join_promise, &result] { join_promise.set_value(true); });
    }
  };

  routing3.Join(functors3);
  EXPECT_EQ(join_future.wait_for(boost::chrono::seconds(10)), boost::future_status::ready);

  // Test SendDirect
  std::mutex mutex;
  std::condition_variable cond_var;
  std::string data(RandomAlphaNumericString(512 * 1024));
  ResponseFunctor response_functor = [&cond_var, &mutex, &data](std::string string) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      ASSERT_EQ(("response to " + data), string);
    }
    cond_var.notify_one();
  };
  routing3.SendDirect(node1.node_info.id, data, false, response_functor);
  std::unique_lock<std::mutex> lock(mutex);
  EXPECT_EQ(std::cv_status::no_timeout, cond_var.wait_for(lock, std::chrono::seconds(10)));
}

TEST(APITest, BEH_API_ClientNodeSameId) {
  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
      endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  ScopedBootstrapFile bootstrap_file({endpoint1, endpoint2});


  auto pmid1(passport::CreatePmidAndSigner().first), pmid2(passport::CreatePmidAndSigner().first);
  auto maid(passport::CreateMaidAndSigner().first);
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeysWithPmid(pmid1));
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeysWithPmid(pmid2));
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeysWithMaid(maid));
  std::map<NodeId, asymm::PublicKey> key_map;
  key_map.insert(std::make_pair(node1.node_info.id, pmid1.public_key()));
  key_map.insert(std::make_pair(node2.node_info.id, pmid2.public_key()));

  Functors functors1, functors2, functors3, functors4;
  Routing routing1(pmid1);
  Routing routing2(pmid2);
  Routing routing3(maid);
  Routing routing4(maid);
  functors1.message_and_caching.message_received = no_ops_message_received_functor;
  functors4 = functors3 = functors2 = functors1;
  functors1.network_status = [](int) {};  // NOLINT (Fraser)
  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
    LOG(kWarning) << "node_validation called for " << DebugId(node_id);
    auto itr(key_map.find(node_id));
    if (key_map.end() != itr)
      give_key((*itr).second);
  };

  functors1.message_and_caching.message_received = [&](const std::string& message,
                                                       ReplyFunctor reply_functor) {
    reply_functor("response to " + message);
    LOG(kVerbose) << "Message received and replied to message !!";
  };

  functors4.network_status = functors2.network_status = functors3.network_status =
      functors1.network_status;
  functors4.request_public_key = functors2.request_public_key = functors3.request_public_key =
      functors1.request_public_key;
  auto a1 = boost::async(boost::launch::async, [&] {
    return routing1.ZeroStateJoin(functors1, endpoint1, endpoint2, node2.node_info);
  });
  auto a2 = boost::async(boost::launch::async, [&] {
    return routing2.ZeroStateJoin(functors2, endpoint2, endpoint1, node1.node_info);
  });
  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  std::once_flag join_set_promise_flag1;
  boost::promise<bool> join_promise1;
  auto join_future1 = join_promise1.get_future();
  functors3.network_status = [&join_set_promise_flag1, &join_promise1](int result) {
    ASSERT_GE(result, kSuccess);
    if (result == NetworkStatus(true, 2)) {
      LOG(kVerbose) << "3rd node joined";
      std::call_once(join_set_promise_flag1,
                     [&join_promise1, &result] { join_promise1.set_value(true); });
    }
  };

  routing3.Join(functors3);
  EXPECT_EQ(join_future1.wait_for(boost::chrono::seconds(10)), boost::future_status::ready);
  std::once_flag join_set_promise_flag2;
  boost::promise<bool> join_promise2;
  auto join_future2 = join_promise2.get_future();
  functors4.network_status = [&join_set_promise_flag2, &join_promise2](int result) {
    ASSERT_GE(result, kSuccess);
    if (result == NetworkStatus(true, 2)) {
      LOG(kVerbose) << "4th node joined";
      std::call_once(join_set_promise_flag2,
                     [&join_promise2, &result] { join_promise2.set_value(true); });
    }
  };

  Sleep(std::chrono::seconds(2));
  routing4.Join(functors4);
  EXPECT_EQ(join_future2.wait_for(boost::chrono::seconds(10)), boost::future_status::ready);

  // Test SendDirect (1)
  std::string data_1("message 1 from client node");
  std::mutex mutex_1;
  std::condition_variable cond_var_1;
  ResponseFunctor response_functor_1 = [&data_1, &mutex_1, &cond_var_1](std::string string) {
    {
      std::lock_guard<std::mutex> lock_1(mutex_1);
      ASSERT_EQ("response to " + data_1, string);
    }
    cond_var_1.notify_one();
  };
  routing3.SendDirect(node1.node_info.id, data_1, false, response_functor_1);
  std::unique_lock<std::mutex> lock_1(mutex_1);
  EXPECT_EQ(std::cv_status::no_timeout, cond_var_1.wait_for(lock_1, std::chrono::seconds(10)));

  // Test SendDirect (2)
  std::string data_2("message 2 from client node");
  std::mutex mutex_2;
  std::condition_variable cond_var_2;
  ResponseFunctor response_functor_2 = [&data_2, &mutex_2, &cond_var_2](std::string string) {
    {
      std::lock_guard<std::mutex> lock_2(mutex_2);
      ASSERT_EQ("response to " + data_2, string);
    }
    cond_var_2.notify_one();
  };
  routing4.SendDirect(node1.node_info.id, data_2, false, response_functor_2);
  std::unique_lock<std::mutex> lock_2(mutex_2);
  EXPECT_EQ(std::cv_status::no_timeout, cond_var_2.wait_for(lock_2, std::chrono::seconds(10)));
}

TEST(APITest, BEH_API_NodeNetwork) {
  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
      endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  ScopedBootstrapFile bootstrap_file({endpoint1, endpoint2});

  int min_join_status(8);  // TODO(Prakash): To decide
  Functors functors;
  functors.message_and_caching.message_received = no_ops_message_received_functor;
  std::map<NodeId, asymm::PublicKey> key_map;
  functors.network_status = [](int) {};  // NOLINT (Fraser)
  functors.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
    LOG(kWarning) << "node_validation called for " << DebugId(node_id);
    auto itr(key_map.find(node_id));
    if (key_map.end() != itr)
      give_key((*itr).second);
  };

  std::vector<NodeInfoAndPrivateKey> nodes;
  std::vector<std::shared_ptr<Routing>> routing_node;
  for (auto i(0); i != kServerCount; ++i) {
    auto pmid(passport::CreatePmidAndSigner().first);
    NodeInfoAndPrivateKey node(MakeNodeInfoAndKeysWithPmid(pmid));
    nodes.push_back(node);
    key_map.insert(std::make_pair(node.node_info.id, pmid.public_key()));
    routing_node.push_back(std::make_shared<Routing>(pmid));
    LOG(kVerbose) << "node.node_info.id " << node.node_info.id;
  }

  auto a1 = boost::async(boost::launch::async, [&] {
    return routing_node[0]->ZeroStateJoin(functors, endpoint1, endpoint2, nodes[1].node_info);
  });
  auto a2 = boost::async(boost::launch::async, [&] {
    return routing_node[1]->ZeroStateJoin(functors, endpoint2, endpoint1, nodes[0].node_info);
  });

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  for (auto i(0); i != (kServerCount - 2); ++i) {
    std::shared_ptr<boost::promise<bool>> join_promise_ptr(
        std::make_shared<boost::promise<bool>>());
    std::shared_ptr<bool> promised(std::make_shared<bool>(false));
    boost::future<bool> join_future((*join_promise_ptr).get_future());
    functors.network_status = [i, min_join_status, join_promise_ptr, promised](int result) {
      if (result == NetworkStatus(false, std::min(i + 2, min_join_status))) {
        if (!(*promised)) {
          (*join_promise_ptr).set_value(true);
          (*promised) = true;
        }
        LOG(kVerbose) << "node - " << i + 2 << "joined";
      }
    };

    routing_node[i + 2]->Join(functors);
    ASSERT_EQ(join_future.wait_for(boost::chrono::seconds(10)), boost::future_status::ready);
    LOG(kVerbose) << "node ---------------------------- " << i + 2 << "joined";
  }
}

TEST(APITest, BEH_API_NodeNetworkWithClient) {
  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
      endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  ScopedBootstrapFile bootstrap_file({endpoint1, endpoint2});


  int min_join_status(std::min(kServerCount, 8));
  std::vector<boost::promise<bool>> join_promises(kNetworkSize);
  std::vector<boost::future<bool>> join_futures;
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
    auto pmid(passport::CreatePmidAndSigner().first);
    NodeInfoAndPrivateKey node(MakeNodeInfoAndKeysWithPmid(pmid));
    nodes.push_back(node);
    key_map.insert(std::make_pair(node.node_info.id, pmid.public_key()));
    routing_node.push_back(std::make_shared<Routing>(pmid));
  }
  for (; i != kNetworkSize; ++i) {
    auto maid(passport::CreateMaidAndSigner().first);
    NodeInfoAndPrivateKey node(MakeNodeInfoAndKeysWithMaid(maid));
    nodes.push_back(node);
    routing_node.push_back(std::make_shared<Routing>(maid));
  }

  functors.network_status = [](int) {};  // NOLINT (Fraser)

  functors.message_and_caching.message_received = [&](const std::string& message,
                                                      ReplyFunctor reply_functor) {
    reply_functor("response to " + message);
    LOG(kVerbose) << "Message received and replied to message !!";
  };

  Functors client_functors;
  client_functors.network_status = [](int) {};  // NOLINT (Fraser)
  client_functors.request_public_key = functors.request_public_key;
  client_functors.message_and_caching.message_received = [&](const std::string&,
                                                             ReplyFunctor /*reply_functor*/) {
    ASSERT_TRUE(false);  //  Client should not receive incoming message
  };
  auto a1 = boost::async(boost::launch::async, [&] {
    return routing_node[0]->ZeroStateJoin(functors, endpoint1, endpoint2, nodes[1].node_info);
  });
  auto a2 = boost::async(boost::launch::async, [&] {
    return routing_node[1]->ZeroStateJoin(functors, endpoint2, endpoint1, nodes[0].node_info);
  });

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  // Ignoring 2 zero state nodes
  promised.push_back(false);
  promised.push_back(false);
  status_vector.emplace_back([](int /*x*/) {});
  status_vector.emplace_back([](int /*x*/) {});
  boost::promise<bool> promise1, promise2;
  join_futures.emplace_back(promise1.get_future());
  join_futures.emplace_back(promise2.get_future());

  // Joining remaining server & client nodes
  for (auto i(2); i != (kNetworkSize); ++i) {
    join_futures.emplace_back(join_promises.at(i).get_future());
    promised.push_back(true);
    status_vector.emplace_back([=, &join_promises, &mutex, &promised](int result) {
      if (result < kSuccess)
        return;
      if (result == NetworkStatus((i >= kServerCount), std::min(i, min_join_status))) {
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
    routing_node[i]->Join(functors);
    ASSERT_EQ(join_futures.at(i).wait_for(boost::chrono::seconds(10)), boost::future_status::ready);
  }
}

TEST(APITest, BEH_API_PartiallyJoinedSend) {
  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
      endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  ScopedBootstrapFile bootstrap_file({endpoint1, endpoint2});

  auto pmid1(passport::CreatePmidAndSigner().first), pmid2(passport::CreatePmidAndSigner().first),
      pmid3(passport::CreatePmidAndSigner().first);
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeysWithPmid(pmid1));
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeysWithPmid(pmid2));
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeysWithPmid(pmid3));
  std::map<NodeId, asymm::PublicKey> key_map;
  key_map.insert(std::make_pair(node1.node_info.id, pmid1.public_key()));
  key_map.insert(std::make_pair(node2.node_info.id, pmid2.public_key()));
  key_map.insert(std::make_pair(node3.node_info.id, pmid3.public_key()));

  Functors functors1, functors2, functors3;
  Routing routing1(pmid1);
  Routing routing2(pmid2);
  Routing routing3(pmid3);

  functors1.network_status = [](int) {};  // NOLINT (Fraser)
  functors1.message_and_caching.message_received =  [&](const std::string& message,
                                                       ReplyFunctor reply_functor) {
    reply_functor("response to " + message);
    LOG(kVerbose) << "Message received and replied to message !!";
  };

  functors3 = functors2 = functors1;

  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
    LOG(kWarning) << "node_validation called for " << DebugId(node_id);
    auto itr(key_map.find(node_id));
    if (key_map.end() != itr)
      give_key((*itr).second);
  };

  functors2.network_status = functors3.network_status = functors1.network_status;
  functors2.request_public_key = functors3.request_public_key = functors1.request_public_key;

  auto a1 = boost::async(boost::launch::async, [&] {
    return routing1.ZeroStateJoin(functors1, endpoint1, endpoint2, node2.node_info);
  });
  auto a2 = boost::async(boost::launch::async, [&] {
    return routing2.ZeroStateJoin(functors2, endpoint2, endpoint1, node1.node_info);
  });
  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !
  std::once_flag flag;
  boost::promise<void> join_promise;
  auto join_future = join_promise.get_future();

  functors3.network_status = [&flag, &join_promise](int result) {
    if (result == NetworkStatus(false, 2)) {
      std::call_once(flag, [&join_promise]() { join_promise.set_value(); });
    }
  };
  routing3.Join(functors3);

  // Test PartiallyJoinedSend
  std::string data("message from my node");
  std::once_flag response_flag;
  boost::promise<void> response_promise;
  auto response_future = response_promise.get_future();
  std::atomic<int> count(0);
  ResponseFunctor response_functor = [&data, &response_flag, &response_promise, &count](
                                         std::string str) {
    EXPECT_EQ("response to " + data, str);
    ++count;
    if (count == 2)
      std::call_once(response_flag, [&response_promise]() { response_promise.set_value(); });
    LOG(kVerbose) << "ResponseFunctor - end";
  };
  routing3.SendDirect(node1.node_info.id, data, false, response_functor);
  routing3.SendDirect(node2.node_info.id, data, false, response_functor);
  EXPECT_EQ(response_future.wait_for(boost::chrono::seconds(10)), boost::future_status::ready);
  EXPECT_EQ(join_future.wait_for(boost::chrono::seconds(20)), boost::future_status::ready);

  EXPECT_EQ(count, 2);
}

TEST(APITest, BEH_API_TypedMessageSend) {
  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
      endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  ScopedBootstrapFile bootstrap_file({endpoint1, endpoint2});

  auto pmid1(passport::CreatePmidAndSigner().first), pmid2(passport::CreatePmidAndSigner().first),
      pmid3(passport::CreatePmidAndSigner().first);
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeysWithPmid(pmid1));
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeysWithPmid(pmid2));
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeysWithPmid(pmid3));
  std::map<NodeId, asymm::PublicKey> key_map;
  key_map.insert(std::make_pair(node1.node_info.id, pmid1.public_key()));
  key_map.insert(std::make_pair(node2.node_info.id, pmid2.public_key()));
  key_map.insert(std::make_pair(node3.node_info.id, pmid3.public_key()));

  Functors functors1, functors2, functors3;
  boost::promise<bool> single_to_single_promise, single_to_group_promise, group_to_single_promise,
      group_to_group_promise;
  size_t responses(0);
  std::mutex mutex;
  Routing routing1(pmid1);
  Routing routing2(pmid2);
  Routing routing3(pmid3);

  functors1.network_status = [](int) {};  // NOLINT (Fraser)
  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
    LOG(kWarning) << "node_validation called for " << DebugId(node_id);
    auto itr(key_map.find(node_id));
    if (key_map.end() != itr)
      give_key((*itr).second);
  };

  functors1.typed_message_and_caching.group_to_group.message_received = [&](
      const GroupToGroupMessage& /*g2g*/) {
    LOG(kVerbose) << "group to group message received!!";
    std::lock_guard<std::mutex> lock(mutex);
    if (++responses == 3)
      group_to_group_promise.set_value(true);
  };

  functors1.typed_message_and_caching.group_to_single.message_received = [&](
      const GroupToSingleMessage& /*g2s*/) {
    LOG(kVerbose) << "group to single message received!!";
    group_to_single_promise.set_value(true);
  };

  functors1.typed_message_and_caching.single_to_group.message_received = [&](
      const SingleToGroupMessage& /*s2g*/) {
    LOG(kVerbose) << "single to group message received!!";
    std::lock_guard<std::mutex> lock(mutex);
    if (++responses == 3)
      single_to_group_promise.set_value(true);
  };

  functors1.typed_message_and_caching.single_to_single.message_received = [&](
      const SingleToSingleMessage& /*s2s*/) {
    LOG(kVerbose) << "single to single message received!!";
    single_to_single_promise.set_value(true);
  };

  functors1.typed_message_and_caching.single_to_group_relay.message_received = [&](
      const SingleToGroupRelayMessage& /*s2g_relay*/) {
    LOG(kVerbose) << "single to group relay message received!!";
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

  functors2.typed_message_and_caching.single_to_group_relay.message_received =
      functors3.typed_message_and_caching.single_to_group_relay.message_received =
          functors1.typed_message_and_caching.single_to_group_relay.message_received;

  auto a1 = boost::async(boost::launch::async, [&] {
    return routing1.ZeroStateJoin(functors1, endpoint1, endpoint2, node2.node_info);
  });
  auto a2 = boost::async(boost::launch::async, [&] {
    return routing2.ZeroStateJoin(functors2, endpoint2, endpoint1, node1.node_info);
  });

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  std::once_flag join_set_promise_flag;
  boost::promise<bool> join_promise;
  auto join_future = join_promise.get_future();
  functors3.network_status = [&join_set_promise_flag, &join_promise](int result) {
    ASSERT_GE(result, kSuccess);
    if (result == NetworkStatus(false, 2)) {
      std::call_once(join_set_promise_flag, [&join_promise, &result] {
        LOG(kVerbose) << "3rd node joined";
        join_promise.set_value(true);
      });
    }
  };

  routing3.Join(functors3);
  EXPECT_EQ(join_future.wait_for(boost::chrono::seconds(10)), boost::future_status::ready);

  {  // Test Group To Group
    GroupToGroupMessage group_to_group_message;
    GroupSource group_source;
    group_source.sender_id = SingleId(routing3.kNodeId());
    group_source.group_id = GroupId(GenerateUniqueRandomId(routing3.kNodeId(), 30));
    group_to_group_message.contents = "Dummy content for test puepose";
    group_to_group_message.receiver = GroupId(GenerateUniqueRandomId(routing1.kNodeId(), 30));
    group_to_group_message.sender = group_source;
    routing3.Send(group_to_group_message);
    auto group_to_group_future(group_to_group_promise.get_future());
    ASSERT_EQ(group_to_group_future.wait_for(boost::chrono::seconds(10)),
              boost::future_status::ready);
  }

  {  // Test Single To Single
    SingleToSingleMessage single_to_single_message;
    single_to_single_message.receiver = SingleId(routing1.kNodeId());
    single_to_single_message.sender = SingleSource(SingleId(routing3.kNodeId()));
    single_to_single_message.contents = "Dummy content for test puepose";
    routing3.Send(single_to_single_message);
    auto single_to_single_future(single_to_single_promise.get_future());
    ASSERT_EQ(single_to_single_future.wait_for(boost::chrono::seconds(10)),
              boost::future_status::ready);
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
    ASSERT_EQ(group_to_single_future.wait_for(boost::chrono::seconds(10)),
              boost::future_status::ready);
  }

  {  //  Test Single To Group
    responses = 0;
    SingleToGroupMessage single_to_group_message;
    single_to_group_message.sender = SingleSource(SingleId(routing3.kNodeId()));
    single_to_group_message.receiver = GroupId(GenerateUniqueRandomId(routing1.kNodeId(), 30));
    routing3.Send(single_to_group_message);
    auto single_to_group_future(single_to_group_promise.get_future());
    ASSERT_EQ(single_to_group_future.wait_for(boost::chrono::seconds(10)),
              boost::future_status::ready);
  }
}


TEST(APITest, BEH_API_TypedMessagePartiallyJoinedSendReceive) {
  Endpoint endpoint1(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort()),
      endpoint2(maidsafe::GetLocalIp(), maidsafe::test::GetRandomPort());
  ScopedBootstrapFile bootstrap_file({endpoint1, endpoint2});

  const int kMessageCount = 10;
  int min_join_status(std::min(kServerCount, 8));
  std::vector<boost::promise<bool>> join_promises(kNetworkSize);
  std::vector<boost::future<bool>> join_futures;
  std::deque<bool> promised;
  std::vector<NetworkStatusFunctor> status_vector;
  Functors functors;

  std::vector<NodeInfoAndPrivateKey> nodes;
  std::map<NodeId, asymm::PublicKey> key_map;
  std::mutex mutex;
  std::vector<std::pair<int, SingleToGroupRelayMessage>> received_relay_messages;
  std::vector<std::shared_ptr<Routing>> routing_nodes;
  std::condition_variable cv;
  std::vector<RelayMessageFunctorType<SingleToGroupRelayMessage>> single_to_group_relay_functors;
  int i(0);
  functors.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key) {
    LOG(kWarning) << "node_validation called for " << DebugId(node_id);
    auto itr(key_map.find(node_id));
    if (key_map.end() != itr)
      give_key((*itr).second);
  };

  for (; i != kServerCount; ++i) {
    auto pmid(passport::CreatePmidAndSigner().first);
    NodeInfoAndPrivateKey node(MakeNodeInfoAndKeysWithPmid(pmid));
    nodes.push_back(node);
    key_map.insert(std::make_pair(node.node_info.id, pmid.public_key()));
    routing_nodes.push_back(std::make_shared<Routing>(pmid));
  }
  for (; i != kNetworkSize; ++i) {  // clients
    auto maid(passport::CreateMaidAndSigner().first);
    NodeInfoAndPrivateKey node(MakeNodeInfoAndKeysWithMaid(maid));
    nodes.push_back(node);
    routing_nodes.push_back(std::make_shared<Routing>(maid));
  }

  functors.network_status = [](int) {};  // NOLINT (Fraser)

  functors.typed_message_and_caching.group_to_group.message_received = [&](
      const GroupToGroupMessage& /*g2g*/) { LOG(kVerbose) << "group to group message received!!"; };

  functors.typed_message_and_caching.group_to_single.message_received = [&](
      const GroupToSingleMessage& /*g2s*/) {
    LOG(kVerbose) << "group to single message received!!";
  };

  functors.typed_message_and_caching.single_to_group.message_received = [&](
      const SingleToGroupMessage& /*s2g*/) {
    LOG(kVerbose) << "single to group message received!!";
  };

  functors.typed_message_and_caching.single_to_single.message_received = [&](
      const SingleToSingleMessage& /*s2s*/) {
    LOG(kVerbose) << "single to single message received!!";
  };


  // response to single to group relay messages
  for (int j = 0; j != kNetworkSize; ++j) {
    auto node_id = routing_nodes.at(j)->kNodeId();
    LOG(kError) << "Preparing functor for node --" << j << " , " << DebugId(node_id);
    RelayMessageFunctorType<SingleToGroupRelayMessage> relay_message_functor_struct;
    relay_message_functor_struct.message_received =
        [node_id, j, &cv, &mutex, &received_relay_messages](
            const SingleToGroupRelayMessage& message) {
      LOG(kVerbose) << "single to group relay message received at node index : " << j
                    << " , node id : " << DebugId(node_id);
      {
        std::lock_guard<std::mutex> lock(mutex);
        received_relay_messages.push_back(std::pair<int, SingleToGroupRelayMessage>(j, message));
      }
      cv.notify_one();
    };
    single_to_group_relay_functors.push_back(relay_message_functor_struct);
  }


  Functors functors_1 = functors;
  functors_1.typed_message_and_caching.single_to_group_relay = single_to_group_relay_functors.at(0);
  auto a1 = boost::async(boost::launch::async, [&] {
    return routing_nodes[0]->ZeroStateJoin(functors_1, endpoint1, endpoint2, nodes[1].node_info);
  });
  Functors functors_2 = functors;
  functors_2.typed_message_and_caching.single_to_group_relay = single_to_group_relay_functors.at(1);

  auto a2 = boost::async(boost::launch::async, [&] {
    return routing_nodes[1]->ZeroStateJoin(functors_2, endpoint2, endpoint1, nodes[0].node_info);
  });

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  // Ignoring 2 zero state nodes
  promised.push_back(false);
  promised.push_back(false);
  status_vector.emplace_back([](int /*x*/) {});
  status_vector.emplace_back([](int /*x*/) {});
  boost::promise<bool> promise1, promise2;
  join_futures.emplace_back(promise1.get_future());
  join_futures.emplace_back(promise2.get_future());

  //  // Joining remaining server & client nodes
  for (auto i(2); i != (kNetworkSize); ++i) {
    join_futures.emplace_back(join_promises.at(i).get_future());
    promised.push_back(true);
    status_vector.emplace_back([=, &join_promises, &mutex, &promised](int result) {
      if (result < kSuccess)
        return;
      if (result == NetworkStatus((i >= kServerCount), std::min(i, min_join_status))) {
        std::lock_guard<std::mutex> lock(mutex);
        if (promised.at(i)) {
          join_promises.at(i).set_value(true);
          promised.at(i) = false;
          LOG(kVerbose) << "node - " << i << "joined";
        }
      }
    });
  }

  for (i = 2; i != (kNetworkSize); ++i) {
    functors.network_status = status_vector.at(i);
    functors.typed_message_and_caching.single_to_group_relay = single_to_group_relay_functors.at(i);
    routing_nodes[i]->Join(functors);
    ASSERT_EQ(join_futures.at(i).wait_for(boost::chrono::seconds(10)), boost::future_status::ready);
  }

  //  -----  Join new vault and send messages ----------
  auto pmid(passport::CreatePmidAndSigner().first);
  Routing test_node(pmid);
  bool test_node_joined(false);
  Functors test_functors;

  test_functors.network_status = [min_join_status, &test_node_joined, &cv, &mutex](int result) {
    ASSERT_GE(result, kSuccess);
    {
      std::lock_guard<std::mutex> lock(mutex);
      test_node_joined = true;
    }
    cv.notify_one();
    LOG(kVerbose) << "test node bootstrapped";
  };

  test_functors.typed_message_and_caching.group_to_group.message_received = [](
      const GroupToGroupMessage& /*g2g*/) {};  // NOLINT
  unsigned int response_count(0);
  test_functors.typed_message_and_caching.group_to_single.message_received =
      [&mutex, &response_count, &cv](const GroupToSingleMessage& /*g2s*/) {
    LOG(kVerbose) << "group to single message received at test node";
    {
      std::lock_guard<std::mutex> lock(mutex);
      ++response_count;
    }
    cv.notify_one();
  };
  test_functors.typed_message_and_caching.single_to_group.message_received = [](
      const SingleToGroupMessage& /*s2g*/) {};  // NOLINT
  test_functors.typed_message_and_caching.single_to_single.message_received = [](
      const SingleToSingleMessage& /*s2s*/) {};  // NOLINT
  test_functors.typed_message_and_caching.single_to_group_relay.message_received = [&](
      const SingleToGroupRelayMessage& /*s2s*/) {};  // NOLINT

  test_node.Join(test_functors);
  {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&test_node_joined] { return test_node_joined; });
  }

  auto send_message = [&test_node](const NodeId& target_id) {
    SingleToGroupMessage single_to_group_message;
    single_to_group_message.sender = SingleSource(SingleId(test_node.kNodeId()));
    single_to_group_message.receiver = GroupId(target_id);
    test_node.Send(single_to_group_message);
  };

  //  Test Single To Group own id
  send_message(test_node.kNodeId());
  {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&received_relay_messages] {
      return received_relay_messages.size() == Parameters::group_size;
    });
    ASSERT_EQ(received_relay_messages.size(), Parameters::group_size);
  };

  //  Test Single To Group random id
  for (auto message_count(0); message_count < kMessageCount; ++message_count)
    send_message(test_node.kNodeId());
  {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [kMessageCount, &received_relay_messages] {
      return (received_relay_messages.size() == (Parameters::group_size * (kMessageCount + 1U)));
    });
    ASSERT_EQ(received_relay_messages.size(), (Parameters::group_size * (kMessageCount + 1U)));
  };

  // Send response
  for (const auto& recipiant : received_relay_messages) {
    auto& node = routing_nodes.at(recipiant.first);
    SingleIdRelay single_id_relay(detail::GetRelayIdToReply(recipiant.second.sender));
    GroupSource group_source(GroupId(recipiant.second.receiver), SingleId(node->kNodeId()));
    GroupToSingleRelayMessage response_message("response", group_source, single_id_relay);
    LOG(kWarning) << "Sending response from index " << recipiant.first;
    node->Send(response_message);
  }
  {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait_for(lock, std::chrono::seconds(20), [&response_count, kMessageCount] {
      return (response_count == Parameters::group_size * (kMessageCount + 1));
    });
  }
  ASSERT_EQ(response_count, Parameters::group_size * (kMessageCount + 1));
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
