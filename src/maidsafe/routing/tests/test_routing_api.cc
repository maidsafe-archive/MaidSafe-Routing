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
  node.endpoint.address(boost::asio::ip::address::from_string("192.168.1.1"));
  node.endpoint.port(++test_routing_api_node_port);
  return node;
}

asymm::Keys MakeKeys() {
  NodeInfo node(MakeNodeInfo());
  asymm::Keys keys;
  keys.identity = node.node_id.String();
  keys.public_key = node.public_key;
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
//  EXPECT_TRUE(boost::filesystem::remove(node1_config));
//  EXPECT_TRUE(boost::filesystem::remove(node2_config));
}

TEST(APITest, BEH_API_ZeroState) {
  asymm::Keys keys1(MakeKeys());
  asymm::Keys keys2(MakeKeys());
  asymm::Keys keys3(MakeKeys());
  Routing R1(keys1, false);
  Routing R2(keys2, false);
  Routing R3(keys3, false);
  bool zero_state1(true), zero_state2(true);
  Functors functors1, functors2, functors3;
  functors1.node_validation = [&](const NodeId& /*node_id*/,
                                  const rudp::EndpointPair& their_endpoint,
                                  const rudp::EndpointPair& our_endpoint,
                                  const bool& client) {
     if (zero_state1) {
       LOG(kVerbose) << "node_validation called for " << HexSubstr(keys2.identity);
       R1.ValidateThisNode(NodeId(keys2.identity), keys2.public_key, their_endpoint, our_endpoint,
                           client);
       zero_state1 = false;
     }
  };

  functors2.node_validation = [&](const NodeId& node_id,
                                 const rudp::EndpointPair& their_endpoint,
                                 const rudp::EndpointPair& our_endpoint,
                                 const bool& client) {
    if (zero_state2) {
      LOG(kVerbose) << "node_validation called for " << HexSubstr(keys1.identity);
      R2.ValidateThisNode(NodeId(keys1.identity), keys1.public_key, their_endpoint, our_endpoint,
                          client);
      zero_state2 = false;
    } else {
      LOG(kVerbose) << "node_validation called for " << HexSubstr(node_id.String());
      if (node_id == NodeId(keys3.identity))
        R2.ValidateThisNode(node_id, keys1.public_key, their_endpoint, our_endpoint,
                            client);
    }
  };
  Endpoint endpoint1(GetLocalIp(), 5000);
  Endpoint endpoint2(GetLocalIp(), 5001);
  Endpoint endpoint3(GetLocalIp(), 5002);

  auto a1 = std::async(std::launch::async,
                       [&]{return R1.Join(functors1, endpoint2, endpoint1);});  // NOLINT (Prakash)
  auto a2 = std::async(std::launch::async,
                       [&]{return R2.Join(functors2, endpoint1, endpoint2);});  // NOLINT (Prakash)

  EXPECT_EQ(kSuccess, a2.get());  // wait for promise !
  EXPECT_EQ(kSuccess, a1.get());  // wait for promise !

  auto a3 = std::async(std::launch::async,
                       [&]{return R3.Join(functors3, endpoint2);});
  EXPECT_EQ(kSuccess, a3.get());  // wait for promise !

  EXPECT_GT(R3.GetStatus(), 0);
}

//TEST(APITest, BEH_API_NodeNetwork) {
//  const uint16_t network_size(30);
//  std::vector<asymm::Keys> network(network_size, MakeKeys());
////  int count(0);
////  for (auto &i : network) {
////    Routing AnodeToBEFixed (i, fs::unique_path(fs::temp_directory_path() / i.identity), nullptr, false);
////  }
//  // TODO(dirvine) do this properly !!!
//
//}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
