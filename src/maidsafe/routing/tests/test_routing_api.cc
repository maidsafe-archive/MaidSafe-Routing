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
static unsigned short test_routing_api_node_port(6000);

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

//TEST(APITest, BEH_BadConfigFile) {
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
//}

TEST(APITest, BEH_API_StandAloneNodeNotConnected) {
  asymm::Keys keys(MakeKeys());
//  boost::filesystem::path good_file(fs::unique_path(fs::temp_directory_path() / "test"));
  Functors functors;
  EXPECT_NO_THROW({
    Routing RtAPI(keys, false);
  });
  Routing RAPI(keys, false);
  Endpoint empty_endpoint;
  EXPECT_EQ(RAPI.GetStatus(), kNotJoined);
//  EXPECT_TRUE(boost::filesystem::remove(good_file));
}

TEST(APITest, BEH_API_ManualBootstrap) {
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
//  asymm::Keys keys3(MakeKeys());
  std::map<NodeId, asymm::Keys> key_map;
  key_map.insert(std::make_pair(NodeId(node1.node_id), GetKeys(node1)));
  key_map.insert(std::make_pair(NodeId(node2.node_id), GetKeys(node2)));
  key_map.insert(std::make_pair(NodeId(node3.node_id), GetKeys(node3)));
  node1.endpoint.port(5000);
  node2.endpoint.port(5001);

  Routing R1(GetKeys(node1), false);
  Routing R2(GetKeys(node2), false);
  Routing R3(GetKeys(node3), false);
  Functors functors1, functors2, functors3;

  functors1.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key )
  {
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

  auto a3 = std::async(std::launch::async,
                       [&]{return R3.Join(functors3, node2.endpoint);});  // NOLINT (Prakash)
  EXPECT_EQ(kSuccess, a3.get());  // wait for promise !

  EXPECT_GT(R3.GetStatus(), 0);
}

TEST(APITest, BEH_API_NodeNetwork) {
  uint8_t kNetworkSize(10);
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

  functors.request_public_key = [&](const NodeId& node_id, GivePublicKeyFunctor give_key ) {
      LOG(kWarning) << "node_validation called for " << HexSubstr(node_id.String());
      auto itr(key_map.find(NodeId(node_id)));
      if (key_map.end() != itr)
        give_key((*itr).second.public_key);
  };

  auto a1 = std::async(std::launch::async, [&]{
    return routing_node[0]->ZeroStateJoin(functors, node_infos[0].endpoint,
                                          node_infos[1]);});  // NOLINT (Prakash)
  auto a2 = std::async(std::launch::async, [&]{
    return routing_node[1]->ZeroStateJoin(functors, node_infos[1].endpoint,
                                          node_infos[0]);});  // NOLINT (Prakash)

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  for (auto i(2); i != kNetworkSize; ++i) {
    std::async(std::launch::async, [&]{
    ASSERT_EQ(kSuccess, routing_node[i]->Join(functors, node_infos[i%2].endpoint));
    LOG(kVerbose) << "Joined !!!!!!!!!!!!!!!!! " << i + 1 << " nodes";
  });
}
  for (auto i(0); i != kNetworkSize; ++i) {
    EXPECT_GT(routing_node[i]->GetStatus(), 0);
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
