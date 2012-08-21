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
#include <algorithm>

#include <memory>
#include <vector>

#include "boost/asio.hpp"
#include "boost/filesystem/exception.hpp"
#include "boost/thread/future.hpp"

#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/bootstrap_file_handler.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

namespace bptime = boost::posix_time;
namespace fs = boost::filesystem;

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

#ifdef FAKE_RUDP
  const int kClientCount(10);
  const int kServerCount(10);
#else
  const int kClientCount(8);
  const int kServerCount(8);
#endif

const int kNetworkSize = kClientCount + kServerCount;

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
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeys());
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeys());
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeys());
  std::map<NodeId, asymm::Keys> key_map;
  key_map.insert(std::make_pair(NodeId(node1.node_info.node_id), GetKeys(node1)));
  key_map.insert(std::make_pair(NodeId(node2.node_info.node_id), GetKeys(node2)));
  key_map.insert(std::make_pair(NodeId(node3.node_info.node_id), GetKeys(node3)));

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
      [&] { return R1.ZeroStateJoin(functors1, node1.node_info.endpoint, node2.node_info);
      });
  auto a2 = std::async(std::launch::async,
      [&] { return R2.ZeroStateJoin(functors2, node2.node_info.endpoint, node1.node_info);
      });
  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  boost::promise<bool> join_promise;
  auto join_future = join_promise.get_future();
  functors3.network_status = [&join_promise](int result) {
    ASSERT_GE(result, kSuccess);
    if (result == NetworkStatus(false, 2))
      join_promise.set_value(true);
  };

  R3.Join(functors3, node2.node_info.endpoint);
  EXPECT_TRUE(join_future.timed_wait(boost::posix_time::seconds(10)));
  LOG(kWarning) << "done!!!";
}

TEST(APITest, BEH_API_JoinWithBootstrapFile) {
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeys());
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeys());
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeys());
  std::map<NodeId, asymm::Keys> key_map;
  key_map.insert(std::make_pair(NodeId(node1.node_info.node_id), GetKeys(node1)));
  key_map.insert(std::make_pair(NodeId(node2.node_info.node_id), GetKeys(node2)));
  key_map.insert(std::make_pair(NodeId(node3.node_info.node_id), GetKeys(node3)));

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
      [&] { return R1.ZeroStateJoin(functors1, node1.node_info.endpoint, node2.node_info);
      });
  auto a2 = std::async(std::launch::async,
      [&] { return R2.ZeroStateJoin(functors2, node2.node_info.endpoint, node1.node_info);
      });
  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  //  Writing bootstrap file
  std::string file_id(
      EncodeToBase32(maidsafe::crypto::Hash<maidsafe::crypto::SHA1>(GetKeys(node3).identity)));
  std::string bootstrap_file_name("bootstrap-" + file_id + ".dat");
  std::vector<Endpoint> bootstrap_endpoints;
  bootstrap_endpoints.push_back(node1.node_info.endpoint);
  bootstrap_endpoints.push_back(node2.node_info.endpoint);
  fs::path bootstrap_file_path(fs::current_path() / bootstrap_file_name);
//  fs::path bootstrap_file_path(GetSystemAppDir() / bootstrap_file_name);
  ASSERT_TRUE(WriteBootstrapFile(bootstrap_endpoints, bootstrap_file_path));
  LOG(kInfo) << "Created bootstrap file at : " << bootstrap_file_path;

  //  Bootstraping with created file
  boost::promise<bool> join_promise;
  auto join_future = join_promise.get_future();
  functors3.network_status = [&join_promise](int result) {
    ASSERT_GE(result, kSuccess);
    if (result == NetworkStatus(false, 2))
      join_promise.set_value(true);
  };

  R3.Join(functors3);
  EXPECT_TRUE(join_future.timed_wait(boost::posix_time::seconds(10)));
  EXPECT_TRUE(fs::remove(bootstrap_file_path));
}

#ifndef FAKE_RUDP
TEST(APITest, FUNC_API_AnonymousNode) {
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeys());
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeys());
  std::map<NodeId, asymm::Keys> key_map;
  key_map.insert(std::make_pair(NodeId(node1.node_info.node_id), GetKeys(node1)));
  key_map.insert(std::make_pair(NodeId(node2.node_info.node_id), GetKeys(node2)));

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

  functors1.message_received = [&] (const std::string& message, const NodeId&,
                                    ReplyFunctor reply_functor) {
    reply_functor("response to " + message);
    LOG(kVerbose) << "Message received and replied to message !!";
  };

  functors2.request_public_key = functors1.request_public_key;

  auto a1 = std::async(std::launch::async,
      [&] { return R1.ZeroStateJoin(functors1, node1.node_info.endpoint, node2.node_info);
      });
  auto a2 = std::async(std::launch::async,
      [&] { return R2.ZeroStateJoin(functors2, node2.node_info.endpoint, node1.node_info);
      });
  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  boost::promise<bool> join_promise;
  auto join_future = join_promise.get_future();
  functors3.network_status = [&join_promise](int result) {
    ASSERT_EQ(result, kSuccess);
    if (result == NetworkStatus(true, 0)) {
      join_promise.set_value(true);
      LOG(kVerbose) << "Anonymous Node joined";
    }
  };
  R3.Join(functors3, node2.node_info.endpoint);
  ASSERT_TRUE(join_future.timed_wait(boost::posix_time::seconds(10)));

  ResponseFunctor response_functor = [=](const std::vector<std::string> &message) {
      ASSERT_EQ(1U, message.size());
      ASSERT_EQ("response to message_from_anonymous node", message[0]);
      LOG(kVerbose) << "Got response !!";
    };
  //  Testing Send
  R3.Send(NodeId(node1.node_info.node_id), NodeId(), "message_from_anonymous node",
          response_functor, boost::posix_time::seconds(10), true, false);

  Sleep(boost::posix_time::seconds(61));  // to allow disconnection
  ResponseFunctor failed_response = [=](const std::vector<std::string> &message) {
      ASSERT_TRUE(message.empty());
    };
  R3.Send(NodeId(node1.node_info.node_id), NodeId(), "message_2_from_anonymous node", 
          failed_response, boost::posix_time::seconds(60), true, false);
  Sleep(boost::posix_time::seconds(1));
}
#endif  // !FAKE_RUDP

TEST(APITest, BEH_API_SendToSelf) {
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeys());
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeys());
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeys());

  std::map<NodeId, asymm::Keys> key_map;
  key_map.insert(std::make_pair(NodeId(node1.node_info.node_id), GetKeys(node1)));
  key_map.insert(std::make_pair(NodeId(node2.node_info.node_id), GetKeys(node2)));
  key_map.insert(std::make_pair(NodeId(node3.node_info.node_id), GetKeys(node3)));

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

  functors1.message_received = [&] (const std::string& message, const NodeId&,
                                    ReplyFunctor reply_functor) {
      reply_functor("response to " + message);
      LOG(kVerbose) << "Message received and replied to message !!";
    };

  functors2.request_public_key = functors3.request_public_key = functors1.request_public_key;
  functors2.message_received = functors3.message_received = functors1.message_received;

  auto a1 = std::async(std::launch::async,
      [&] { return R1.ZeroStateJoin(functors1, node1.node_info.endpoint, node2.node_info);
      });
  auto a2 = std::async(std::launch::async,
      [&] { return R2.ZeroStateJoin(functors2, node2.node_info.endpoint, node1.node_info);
      });

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  boost::promise<bool> join_promise;
  auto join_future = join_promise.get_future();
  functors3.network_status = [&join_promise](int result) {
    ASSERT_GE(result, kSuccess);
    if (result == NetworkStatus(false, 2)) {
      LOG(kVerbose) << "3rd node joined";
      join_promise.set_value(true);
    }
  };

  R3.Join(functors3, node2.node_info.endpoint);
  ASSERT_TRUE(join_future.timed_wait(boost::posix_time::seconds(10)));

  //  Testing Send
  boost::promise<bool> response_promise;
  auto response_future = response_promise.get_future();
  ResponseFunctor response_functor = [&](const std::vector<std::string> &message) {
      ASSERT_EQ(1U, message.size());
      ASSERT_EQ("response to message from my node", message[0]);
      LOG(kVerbose) << "Got response !!";
      response_promise.set_value(true);
    };
  R3.Send(NodeId(node3.node_info.node_id), NodeId(), "message from my node", response_functor,
          boost::posix_time::seconds(10), true, false);
  EXPECT_TRUE(response_future.timed_wait(boost::posix_time::seconds(10)));
}

TEST(APITest, BEH_API_ClientNode) {
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeys());
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeys());
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeys());

  std::map<NodeId, asymm::Keys> key_map;
  key_map.insert(std::make_pair(NodeId(node1.node_info.node_id), GetKeys(node1)));
  key_map.insert(std::make_pair(NodeId(node2.node_info.node_id), GetKeys(node2)));
  key_map.insert(std::make_pair(NodeId(node3.node_info.node_id), GetKeys(node3)));

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

  functors1.message_received = [&] (const std::string& message, const NodeId&,
                                    ReplyFunctor reply_functor) {
      reply_functor("response to " + message);
      LOG(kVerbose) << "Message received and replied to message !!";
    };

  functors2.request_public_key = functors3.request_public_key = functors1.request_public_key;

  auto a1 = std::async(std::launch::async,
      [&] { return R1.ZeroStateJoin(functors1, node1.node_info.endpoint, node2.node_info);
      });
  auto a2 = std::async(std::launch::async,
      [&] { return R2.ZeroStateJoin(functors2, node2.node_info.endpoint, node1.node_info);
      });
  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  boost::promise<bool> join_promise;
  bool promised(true);
  auto join_future = join_promise.get_future();
  functors3.network_status = [&join_promise, &promised](int result) {
    ASSERT_GE(result, kSuccess);
    if (result == NetworkStatus(true, 2)) {
      LOG(kVerbose) << "3rd node joined";
      if (promised) {
        promised = false;
        join_promise.set_value(true);
      }
    }
  };

  R3.Join(functors3, node2.node_info.endpoint);  // NOLINT (Prakash)
  ASSERT_TRUE(join_future.timed_wait(boost::posix_time::seconds(10)));

  //  Testing Send
  boost::promise<bool> response_promise;
  auto response_future = response_promise.get_future();
  ResponseFunctor response_functor = [&](const std::vector<std::string> &message) {
      ASSERT_EQ(1U, message.size());
      ASSERT_EQ("response to message from client node", message[0]);
      LOG(kVerbose) << "Got response !!";
      response_promise.set_value(true);
    };
  R3.Send(NodeId(node1.node_info.node_id), NodeId(), "message from client node",
          response_functor, boost::posix_time::seconds(10), true, false);

  EXPECT_TRUE(response_future.timed_wait(boost::posix_time::seconds(10)));
}

TEST(APITest, BEH_API_ClientNodeWithBootstrapFile) {
  NodeInfoAndPrivateKey node1(MakeNodeInfoAndKeys());
  NodeInfoAndPrivateKey node2(MakeNodeInfoAndKeys());
  NodeInfoAndPrivateKey node3(MakeNodeInfoAndKeys());

  std::map<NodeId, asymm::Keys> key_map;
  key_map.insert(std::make_pair(NodeId(node1.node_info.node_id), GetKeys(node1)));
  key_map.insert(std::make_pair(NodeId(node2.node_info.node_id), GetKeys(node2)));
  key_map.insert(std::make_pair(NodeId(node3.node_info.node_id), GetKeys(node3)));

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

  functors1.message_received = [&] (const std::string& message,const NodeId&,
                                    ReplyFunctor reply_functor) {
      reply_functor("response to " + message);
      LOG(kVerbose) << "Message received and replied to message !!";
    };

  functors2.request_public_key = functors3.request_public_key = functors1.request_public_key;

  auto a1 = std::async(std::launch::async,
      [&] { return R1.ZeroStateJoin(functors1, node1.node_info.endpoint, node2.node_info);
      });
  auto a2 = std::async(std::launch::async,
      [&] { return R2.ZeroStateJoin(functors2, node2.node_info.endpoint, node1.node_info);
      });
  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !
  //  Writing bootstrap file
  std::string bootstrap_file_name("bootstrap");
  std::vector<Endpoint> bootstrap_endpoints;
  bootstrap_endpoints.push_back(node1.node_info.endpoint);
  bootstrap_endpoints.push_back(node2.node_info.endpoint);
  fs::path bootstrap_file_path(fs::current_path() / bootstrap_file_name);
  // fs::path bootstrap_file_path(GetUserAppDir() / bootstrap_file_name);
  ASSERT_TRUE(WriteBootstrapFile(bootstrap_endpoints, bootstrap_file_path));
  LOG(kInfo) << "Created bootstrap file at : " << bootstrap_file_path;

  //  Bootstraping with created file
  boost::promise<bool> join_promise;
  auto join_future = join_promise.get_future();
  functors3.network_status = [&join_promise](int result) {
    ASSERT_GE(result, kSuccess);
    if (result == NetworkStatus(true, 2)) {
      LOG(kVerbose) << "3rd node joined";
      join_promise.set_value(true);
    }
  };

  R3.Join(functors3);  // NOLINT (Prakash)
  ASSERT_TRUE(join_future.timed_wait(boost::posix_time::seconds(10)));

  //  Testing Send
  boost::promise<bool> response_promise;
  auto response_future = response_promise.get_future();
  ResponseFunctor response_functor = [&](const std::vector<std::string> &message) {
      ASSERT_EQ(1U, message.size());
      ASSERT_EQ("response to message from client node", message[0]);
      LOG(kVerbose) << "Got response !!";
      response_promise.set_value(true);
    };
  R3.Send(NodeId(node1.node_info.node_id), NodeId(), "message from client node",
          response_functor, boost::posix_time::seconds(10), true, false);

  EXPECT_TRUE(response_future.timed_wait(boost::posix_time::seconds(10)));
  EXPECT_TRUE(fs::remove(bootstrap_file_path));
}

TEST(APITest, BEH_API_NodeNetwork) {
  int min_join_status(8);  // TODO(Prakash): To decide
  std::vector<boost::promise<bool>> join_promises(kNetworkSize - 2);
  std::vector<boost::unique_future<bool>> join_futures;
  std::deque<bool> promised;
  std::vector<NetworkStatusFunctor> status_vector;
  boost::shared_mutex mutex;

  std::vector<NodeInfoAndPrivateKey> nodes;
  std::vector<std::shared_ptr<Routing>> routing_node;
  std::map<NodeId, asymm::Keys> key_map;
  for (auto i(0); i != kNetworkSize; ++i) {
    NodeInfoAndPrivateKey node(MakeNodeInfoAndKeys());
    nodes.push_back(node);
    key_map.insert(std::make_pair(NodeId(node.node_info.node_id), GetKeys(node)));
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
      return routing_node[0]->ZeroStateJoin(functors, nodes[0].node_info.endpoint,
                                          nodes[1].node_info);
      });
  auto a2 = std::async(std::launch::async, [&] {
      return routing_node[1]->ZeroStateJoin(functors, nodes[1].node_info.endpoint,
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
          boost::unique_lock< boost::shared_mutex> lock(mutex);
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
    routing_node[i + 2]->Join(functors, nodes[i % 2].node_info.endpoint);
    ASSERT_TRUE(join_futures.at(i).timed_wait(boost::posix_time::seconds(10)));
    LOG(kVerbose) << "node ---------------------------- " << i + 2 << "joined";
  }
}

TEST(APITest, BEH_API_NodeNetworkWithBootstrapFile) {
  int min_join_status(8);  // TODO(Prakash): To decide
  std::vector<boost::promise<bool>> join_promises(kNetworkSize - 2);
  std::vector<boost::unique_future<bool>> join_futures;
  std::deque<bool> promised;
  std::vector<NetworkStatusFunctor> status_vector;
  boost::shared_mutex mutex;

  std::vector<NodeInfoAndPrivateKey> nodes;
  std::vector<std::shared_ptr<Routing>> routing_node;
  std::map<NodeId, asymm::Keys> key_map;
  for (auto i(0); i != kNetworkSize; ++i) {
    NodeInfoAndPrivateKey node(MakeNodeInfoAndKeys());
    nodes.push_back(node);
    key_map.insert(std::make_pair(NodeId(node.node_info.node_id), GetKeys(node)));
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
      return routing_node[0]->ZeroStateJoin(functors, nodes[0].node_info.endpoint,
                                          nodes[1].node_info);
      });
  auto a2 = std::async(std::launch::async, [&] {
      return routing_node[1]->ZeroStateJoin(functors, nodes[1].node_info.endpoint,
                                          nodes[0].node_info);
      });

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !
  //  Writing bootstrap file
  std::vector<Endpoint> bootstrap_endpoints;
  bootstrap_endpoints.push_back(nodes.at(0).node_info.endpoint);
  bootstrap_endpoints.push_back(nodes.at(1).node_info.endpoint);

  std::vector<fs::path> paths;
  for (auto i(2); i != kNetworkSize; ++i) {
    std::string file_id(
        EncodeToBase32(
            maidsafe::crypto::Hash<maidsafe::crypto::SHA1>(GetKeys(nodes.at(i)).identity)));

    std::string bootstrap_file_name("bootstrap-" + file_id + ".dat");
    fs::path bootstrap_file_path(fs::current_path() / bootstrap_file_name);
    // fs::path bootstrap_file_path(GetSystemAppDir() / bootstrap_file_name);
    ASSERT_TRUE(WriteBootstrapFile(bootstrap_endpoints, bootstrap_file_path));
    LOG(kInfo) << "Created bootstrap file at : " << bootstrap_file_path;
    paths.push_back(bootstrap_file_path);
  }

  //  Bootstraping with created file
  for (auto i(0); i != (kNetworkSize - 2); ++i) {
    join_futures.emplace_back(join_promises.at(i).get_future());
    promised.push_back(true);
    status_vector.emplace_back([=, &join_promises, &mutex, &promised](int result) {
        ASSERT_GE(result, kSuccess);
        if (result == NetworkStatus(false, std::min(i + 2, min_join_status))) {
          boost::unique_lock< boost::shared_mutex> lock(mutex);
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
    routing_node[i + 2]->Join(functors);
    ASSERT_TRUE(join_futures.at(i).timed_wait(boost::posix_time::seconds(10)));
    LOG(kVerbose) << "node ---------------------------- " << i + 2 << "joined";
  }
  for (auto i : paths)
    EXPECT_TRUE(fs::remove(i));
}

TEST(APITest, BEH_API_NodeNetworkWithClient) {
  int min_join_status(std::min(kServerCount, 8));
  std::vector<boost::promise<bool>> join_promises(kNetworkSize);
  std::vector<boost::unique_future<bool>> join_futures;
  std::deque<bool> promised;
  std::vector<NetworkStatusFunctor> status_vector;
  boost::shared_mutex mutex;

  std::vector<NodeInfoAndPrivateKey> nodes;
  std::vector<std::shared_ptr<Routing>> routing_node;
  std::map<NodeId, asymm::Keys> key_map;
  for (auto i(0); i != kNetworkSize; ++i) {
    NodeInfoAndPrivateKey node(MakeNodeInfoAndKeys());
    nodes.push_back(node);
    key_map.insert(std::make_pair(NodeId(node.node_info.node_id), GetKeys(node)));
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

  functors.message_received = [&] (const std::string& message, const NodeId&,
                                   ReplyFunctor reply_functor) {
      reply_functor("response to " + message);
      LOG(kVerbose) << "Message received and replied to message !!";
    };

  Functors client_functors;
  client_functors.request_public_key = functors.request_public_key;

  client_functors.message_received = [&] (const std::string &, const NodeId&,
                                          ReplyFunctor /*reply_functor*/) {
      ASSERT_TRUE(false);  //  Client should not receive incoming message
    };

  auto a1 = std::async(std::launch::async, [&] {
      return routing_node[0]->ZeroStateJoin(functors, nodes[0].node_info.endpoint,
                                          nodes[1].node_info);
      });
  auto a2 = std::async(std::launch::async, [&] {
      return routing_node[1]->ZeroStateJoin(functors, nodes[1].node_info.endpoint,
                                          nodes[0].node_info);
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
                                   ASSERT_GE(result, kSuccess);
                                   if (result == NetworkStatus((i < kServerCount)? false: true,
                                                               std::min(i, min_join_status))) {
                                     boost::unique_lock< boost::shared_mutex> lock(mutex);
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
    routing_node[i]->Join(functors, nodes[0].node_info.endpoint);
    ASSERT_TRUE(join_futures.at(i).timed_wait(boost::posix_time::seconds(10)));
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
