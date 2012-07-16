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

#include <boost/exception/all.hpp>
#include <chrono>
#include <future>

#include <memory>
#include <vector>

#include "boost/asio.hpp"
#include "boost/filesystem/exception.hpp"
#include "boost/thread/future.hpp"

#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {
namespace routing {
namespace test {
namespace bptime = boost::posix_time;

namespace {

#ifdef FAKE_RUDP
  const int32_t kClientCount(10);
  const int32_t kServerCount(10);
#else
  const int32_t kClientCount(4);
  const int32_t kServerCount(4);
#endif

const uint32_t kNetworkSize = kClientCount + kServerCount;

NodeInfo MakeNodeInfo() {
  NodeInfo node;
  node.node_id = NodeId(RandomString(64));
  asymm::Keys keys;
  asymm::GenerateKeyPair(&keys);
  node.public_key = keys.public_key;
  node.endpoint.address(GetLocalIp());
  node.endpoint.port(GetRandomPort());
  return node;
}

asymm::Keys MakeKeys() {
  NodeInfo node(MakeNodeInfo());
  asymm::Keys keys;
  keys.identity = node.node_id.String();
  keys.public_key = node.public_key;
  return keys;
}

asymm::Keys GetKeys(const NodeInfo &node_info) {
  asymm::Keys keys;
  keys.identity = node_info.node_id.String();
  keys.public_key = node_info.public_key;
  return keys;
}

}  // anonymous namespace

// TEST(APITest, BEH_BadConfigFile) {
//  // See bootstrap file tests for further interrogation of these files
//  asymm::Keys keys(MakeKeys());
//  boost::filesystem::path bad_file("/bad file/ not found/ I hope/");
//  boost::filesystem::path good_file
//              (fs::unique_path(fs::temp_directory_path() / "test"));
//  Functors functors;
//  EXPECT_THROW({Routing RtAPI(keys, bad_file, functors, false);},
//              boost::filesystem::filesystem_error)  << "should not accept invalid files";
//  EXPECT_NO_THROW({
//    Routing RtAPI(keys, good_file, functors, false);
//  });
//  EXPECT_TRUE(WriteFile(good_file, "not a vector of endpoints"));
//  EXPECT_NO_THROW({
//    Routing RtAPI(keys, good_file, functors, false);
//  }) << "cannot handle corrupt files";
//  EXPECT_TRUE(boost::filesystem::remove(good_file));
// }

TEST(APITest, DISABLED_BEH_API_StandAloneNodeNotConnected) {
  asymm::Keys keys(MakeKeys());
  Functors functors;
  EXPECT_NO_THROW({
    Routing RtAPI(keys, false);
  });
  Routing RAPI(keys, false);
  Endpoint empty_endpoint;
//  EXPECT_EQ(RAPI.GetStatus(), kNotJoined);
//  EXPECT_TRUE(boost::filesystem::remove(good_file));
}

TEST(APITest, DISABLED_BEH_API_ManualBootstrap) {
  asymm::Keys keys1(MakeKeys());
  asymm::Keys keys2(MakeKeys());
  Functors functors;
  EXPECT_NO_THROW({
    Routing RtAPI(keys1, false);
  });
  EXPECT_NO_THROW({
    Routing RtAPI(keys2, false);
  });
  Routing R1(keys1, false);
  Routing R2(keys2, false);
  boost::asio::ip::udp::endpoint empty_endpoint;
  EXPECT_EQ(kNotJoined, R1.GetStatus());
  EXPECT_EQ(kNotJoined, R2.GetStatus());
  Endpoint endpoint1g(GetLocalIp(), 5000);
  Endpoint endpoint2g(GetLocalIp(), 5001);
  R1.Join(functors, endpoint2g);
  R2.Join(functors, endpoint1g);
  EXPECT_EQ(kSuccess, R1.GetStatus());
  EXPECT_EQ(kSuccess, R2.GetStatus());
}

TEST(APITest, BEH_API_ZeroState) {
  NodeInfo node1(MakeNodeInfo());
  NodeInfo node2(MakeNodeInfo());
  NodeInfo node3(MakeNodeInfo());
  std::map<NodeId, asymm::Keys> key_map;
  key_map.insert(std::make_pair(NodeId(node1.node_id), GetKeys(node1)));
  key_map.insert(std::make_pair(NodeId(node2.node_id), GetKeys(node2)));
  key_map.insert(std::make_pair(NodeId(node3.node_id), GetKeys(node3)));

  Routing R1(GetKeys(node1), false);
  Routing R2(GetKeys(node2), false);
  Routing R3(GetKeys(node3), false);
  Functors functors1, functors2, functors3;

  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key ) {
      LOG(kWarning) << "node_validation called for " << HexSubstr(node_id.String());
      auto itr(key_map.find(NodeId(node_id)));
      if (key_map.end() != itr)
        give_key((*itr).second.public_key);
    };

  functors2.request_public_key = functors3.request_public_key = functors1.request_public_key;

  auto a1 = std::async(std::launch::async,
      [&]{return R1.ZeroStateJoin(functors1, node1.endpoint, node2);});  // NOLINT (Prakash)
  auto a2 = std::async(std::launch::async,
      [&]{return R2.ZeroStateJoin(functors2, node2.endpoint, node1);});  // NOLINT (Prakash)

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  boost::promise<bool> join_promise;
  auto join_future = join_promise.get_future();
  functors3.network_status = [&join_promise](int result) {
    ASSERT_GE(result, kSuccess);
    if (result == 2)
      join_promise.set_value(true);
  };

  R3.Join(functors3, node2.endpoint);  // NOLINT (Prakash)
  EXPECT_TRUE(join_future.timed_wait(boost::posix_time::seconds(10)));
  LOG(kWarning) << "Done";
}

TEST(APITest, FUNC_API_AnonymousNode) {
  NodeInfo node1(MakeNodeInfo());
  NodeInfo node2(MakeNodeInfo());
  std::map<NodeId, asymm::Keys> key_map;
  key_map.insert(std::make_pair(NodeId(node1.node_id), GetKeys(node1)));
  key_map.insert(std::make_pair(NodeId(node2.node_id), GetKeys(node2)));

  Routing R1(GetKeys(node1), false);
  Routing R2(GetKeys(node2), false);
  Routing R3(asymm::Keys(), true);  // Anonymous node
  Functors functors1, functors2, functors3;

  functors1.request_public_key = [=](const NodeId& node_id, GivePublicKeyFunctor give_key) {
      LOG(kWarning) << "node_validation called for " << HexSubstr(node_id.String());
      auto itr(key_map.find(NodeId(node_id)));
      if (key_map.end() != itr)
        give_key((*itr).second.public_key);
    };

  functors1.message_received = [&] (const int32_t&, const std::string &message, const NodeId &,
    ReplyFunctor reply_functor) {
    reply_functor("response to " + message);
    LOG(kVerbose) << "Message received and replied to message !!";
  };

  functors2.request_public_key = functors1.request_public_key;

  auto a1 = std::async(std::launch::async,
      [&]{return R1.ZeroStateJoin(functors1, node1.endpoint, node2);});  // NOLINT (Prakash)
  auto a2 = std::async(std::launch::async,
      [&]{return R2.ZeroStateJoin(functors2, node2.endpoint, node1);});  // NOLINT (Prakash)

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  boost::promise<bool> join_promise;
  auto join_future = join_promise.get_future();
  functors3.network_status = [&join_promise](int result) {
    ASSERT_EQ(result, kSuccess);
    if (result == 0) {
      join_promise.set_value(true);
      LOG(kVerbose) << "Anonymous Node joined";
    }
  };
  R3.Join(functors3, node2.endpoint);
  ASSERT_TRUE(join_future.timed_wait(boost::posix_time::seconds(10)));

  ResponseFunctor response_functor = [=](const int& return_code, const std::string &message) {
      ASSERT_EQ(kSuccess, return_code);
      ASSERT_EQ("response to message_from_anonymous node", message);
      LOG(kVerbose) << "Got response !!";
    };
  //  Testing Send
  R3.Send(NodeId(node1.node_id), NodeId(), "message_from_anonymous node", 101, response_functor,
          boost::posix_time::seconds(10), ConnectType::kSingle);

  Sleep(boost::posix_time::seconds(61));  // to allow disconnection
  ResponseFunctor failed_response = [=](const int& return_code, const std::string &message) {
      EXPECT_EQ(kResponseTimeout, return_code);
      ASSERT_EQ("", message);
    };
  R3.Send(NodeId(node1.node_id), NodeId(), "message_2_from_anonymous node", 101, failed_response,
          boost::posix_time::seconds(60), ConnectType::kSingle);
  Sleep(boost::posix_time::seconds(1));
}

TEST(APITest, BEH_API_SendToSelf) {
  NodeInfo node1(MakeNodeInfo());
  NodeInfo node2(MakeNodeInfo());
  NodeInfo node3(MakeNodeInfo());

  std::map<NodeId, asymm::Keys> key_map;
  key_map.insert(std::make_pair(NodeId(node1.node_id), GetKeys(node1)));
  key_map.insert(std::make_pair(NodeId(node2.node_id), GetKeys(node2)));
  key_map.insert(std::make_pair(NodeId(node3.node_id), GetKeys(node3)));

  Routing R1(GetKeys(node1), false);
  Routing R2(GetKeys(node2), false);
  Routing R3(GetKeys(node3), false);  // client mode
  Functors functors1, functors2, functors3;

  functors1.request_public_key = [=](const NodeId& node_id, GivePublicKeyFunctor give_key ) {
      LOG(kWarning) << "node_validation called for " << HexSubstr(node_id.String());
      auto itr(key_map.find(NodeId(node_id)));
      if (key_map.end() != itr)
        give_key((*itr).second.public_key);
    };

  functors1.message_received = [&] (const int32_t&, const std::string &message, const NodeId &,
    ReplyFunctor reply_functor) {
      reply_functor("response to " + message);
      LOG(kVerbose) << "Message received and replied to message !!";
    };

  functors2.request_public_key = functors3.request_public_key = functors1.request_public_key;
  functors2.message_received = functors3.message_received = functors1.message_received;

  auto a1 = std::async(std::launch::async,
      [&]{return R1.ZeroStateJoin(functors1, node1.endpoint, node2);});  // NOLINT (Prakash)
  auto a2 = std::async(std::launch::async,
      [&]{return R2.ZeroStateJoin(functors2, node2.endpoint, node1);});  // NOLINT (Prakash)

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  auto a3 = std::async(std::launch::async,
                       [&]{return R3.Join(functors3, node2.endpoint);});  // NOLINT (Prakash)
  EXPECT_EQ(kSuccess, a3.get());  // wait for promise !

  //  Testing Send
  boost::promise<bool> response_promise;
  auto response_future = response_promise.get_future();
  ResponseFunctor response_functor = [&](const int& return_code, const std::string &message) {
      ASSERT_EQ(kSuccess, return_code);
      ASSERT_EQ("response to message from my node", message);
      LOG(kVerbose) << "Got response !!";
      response_promise.set_value(true);
    };
  R3.Send(NodeId(node3.node_id), NodeId(), "message from my node", 101, response_functor,
          boost::posix_time::seconds(10), ConnectType::kSingle);
  EXPECT_TRUE(response_future.timed_wait(boost::posix_time::seconds(10)));
}

TEST(APITest, BEH_API_ClientNode) {
  NodeInfo node1(MakeNodeInfo());
  NodeInfo node2(MakeNodeInfo());
  NodeInfo node3(MakeNodeInfo());

  std::map<NodeId, asymm::Keys> key_map;
  key_map.insert(std::make_pair(NodeId(node1.node_id), GetKeys(node1)));
  key_map.insert(std::make_pair(NodeId(node2.node_id), GetKeys(node2)));
  key_map.insert(std::make_pair(NodeId(node3.node_id), GetKeys(node3)));

  Routing R1(GetKeys(node1), false);
  Routing R2(GetKeys(node2), false);
  Routing R3(GetKeys(node3), true);  // client mode
  Functors functors1, functors2, functors3;

  functors1.request_public_key = [=](const NodeId& node_id, GivePublicKeyFunctor give_key ) {
      LOG(kWarning) << "node_validation called for " << HexSubstr(node_id.String());
      auto itr(key_map.find(NodeId(node_id)));
      if (key_map.end() != itr)
        give_key((*itr).second.public_key);
    };

  functors1.message_received = [&] (const int32_t&, const std::string &message, const NodeId &,
    ReplyFunctor reply_functor) {
      reply_functor("response to " + message);
      LOG(kVerbose) << "Message received and replied to message !!";
    };

  functors2.request_public_key = functors3.request_public_key = functors1.request_public_key;

  auto a1 = std::async(std::launch::async,
      [&]{return R1.ZeroStateJoin(functors1, node1.endpoint, node2);});  // NOLINT (Prakash)
  auto a2 = std::async(std::launch::async,
      [&]{return R2.ZeroStateJoin(functors2, node2.endpoint, node1);});  // NOLINT (Prakash)

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  auto a3 = std::async(std::launch::async,
                       [&]{return R3.Join(functors3, node2.endpoint);});  // NOLINT (Prakash)
  EXPECT_EQ(kSuccess, a3.get());  // wait for promise !

  //  Testing Send
  boost::promise<bool> response_promise;
  auto response_future = response_promise.get_future();
  ResponseFunctor response_functor = [&](const int& return_code, const std::string &message) {
      ASSERT_EQ(kSuccess, return_code);
      ASSERT_EQ("response to message from client node", message);
      LOG(kVerbose) << "Got response !!";
      response_promise.set_value(true);
    };
  R3.Send(NodeId(node1.node_id), NodeId(), "message from client node", 101, response_functor,
          boost::posix_time::seconds(10), ConnectType::kSingle);

  EXPECT_TRUE(response_future.timed_wait(boost::posix_time::seconds(10)));
}

TEST(APITest, BEH_API_NodeNetwork) {
  std::vector<NodeInfo> node_infos;
  std::vector<std::shared_ptr<Routing>> routing_node;
  std::map<NodeId, asymm::Keys> key_map;
  for (auto i(0); i != kNetworkSize; ++i) {
    NodeInfo node(MakeNodeInfo());
    node_infos.push_back(node);
    key_map.insert(std::make_pair(NodeId(node.node_id), GetKeys(node)));
    routing_node.push_back(std::make_shared<Routing>(GetKeys(node), false));
  }
  Functors functors;

  functors.request_public_key = [=](const NodeId& node_id, GivePublicKeyFunctor give_key ) {
      LOG(kInfo) << "node_validation called for " << HexSubstr(node_id.String());
      auto itr(key_map.find(NodeId(node_id)));
      if (key_map.end() != itr)
        give_key((*itr).second.public_key);
  };

  auto a1 = std::async(std::launch::async, [&] {
    return routing_node[0]->ZeroStateJoin(functors, node_infos[0].endpoint,
                                          node_infos[1]);});  // NOLINT (Prakash)
  auto a2 = std::async(std::launch::async, [&] {
    return routing_node[1]->ZeroStateJoin(functors, node_infos[1].endpoint,
                                          node_infos[0]);});  // NOLINT (Prakash)

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  for (auto i(2); i != kNetworkSize; ++i) {
//    std::async(std::launch::async, [&] {
    ASSERT_EQ(kSuccess, routing_node[i]->Join(functors, node_infos[i%2].endpoint));
    LOG(kVerbose) << "Joined !!!!!!!!!!!!!!!!! " << i + 1 << " nodes";
//  });
  }
  Sleep(boost::posix_time::seconds(2));
}

TEST(APITest, BEH_API_NodeNetworkWithClient) {
  std::vector<NodeInfo> node_infos;
  std::vector<std::shared_ptr<Routing>> routing_node;
  std::map<NodeId, asymm::Keys> key_map;
  for (auto i(0); i != kNetworkSize; ++i) {
    NodeInfo node(MakeNodeInfo());
    node_infos.push_back(node);
    key_map.insert(std::make_pair(NodeId(node.node_id), GetKeys(node)));
    routing_node.push_back(
        std::make_shared<Routing>(GetKeys(node), ((i < kServerCount)? false: true)));
  }

  Functors functors;

  functors.request_public_key = [=](const NodeId& node_id, GivePublicKeyFunctor give_key ) {
      LOG(kInfo) << "node_validation called for " << HexSubstr(node_id.String());
      auto itr(key_map.find(NodeId(node_id)));
      if (key_map.end() != itr)
        give_key((*itr).second.public_key);
  };

  functors.message_received = [&] (const int32_t&, const std::string &message, const NodeId &,
    ReplyFunctor reply_functor) {
      reply_functor("response to " + message);
      LOG(kVerbose) << "Message received and replied to message !!";
    };

  Functors client_functors;
  client_functors.request_public_key = functors.request_public_key;

  client_functors.message_received = [&] (const int32_t&, const std::string &, const NodeId &,
    ReplyFunctor reply_functor) {
      ASSERT_TRUE(false);  //  Client should not receive incoming message
    };

  auto a1 = std::async(std::launch::async, [&] {
    return routing_node[0]->ZeroStateJoin(functors, node_infos[0].endpoint,
                                          node_infos[1]);});  // NOLINT (Prakash)
  auto a2 = std::async(std::launch::async, [&] {
    return routing_node[1]->ZeroStateJoin(functors, node_infos[1].endpoint,
                                          node_infos[0]);});  // NOLINT (Prakash)

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  for (auto i(2); i != kServerCount; ++i) {
    ASSERT_EQ(kSuccess, routing_node[i]->Join(functors, node_infos[i%2].endpoint));
    LOG(kVerbose) << "Server - " << i
                  << " joined !!!!!!!!!!!!!!!!! " << i + 1 << " nodes";
  }

  for (auto i(kServerCount); i != kNetworkSize; ++i) {
    ASSERT_EQ(kSuccess, routing_node[i]->Join(functors, node_infos[0].endpoint));
    LOG(kVerbose) << "Client - " << i - kServerCount << " joined !!!!!!!!!!!!!!!!! ";
    LOG(kVerbose) << " joined !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! ";
  }

  Sleep(boost::posix_time::seconds(2));
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
