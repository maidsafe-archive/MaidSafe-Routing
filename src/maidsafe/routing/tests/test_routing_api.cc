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

TEST(APITest, BEH_BadConfigFile) {
  // See bootstrap file tests for further interrogation of these files
  asymm::Keys keys(MakeKeys());
  boost::filesystem::path bad_file("/bad file/ not found/ I hope/");
  boost::filesystem::path good_file
              (fs::unique_path(fs::temp_directory_path() / "test"));
  Functors functors;
  EXPECT_THROW({Routing RtAPI(keys, bad_file, functors, false);},
              boost::filesystem::filesystem_error)  << "should not accept invalid files";
  EXPECT_NO_THROW({
    Routing RtAPI(keys, good_file, functors, false);
  });
  EXPECT_TRUE(WriteFile(good_file, "not a vector of endpoints"));
  EXPECT_NO_THROW({
    Routing RtAPI(keys, good_file, functors, false);
  }) << "cannot handle corrupt files";
  EXPECT_TRUE(boost::filesystem::remove(good_file));
}

TEST(APITest, BEH_API_StandAloneNodeNotConnected) {
  asymm::Keys keys(MakeKeys());
  boost::filesystem::path good_file(fs::unique_path(fs::temp_directory_path() / "test"));
  Functors functors;
  EXPECT_NO_THROW({
    Routing RtAPI(keys, good_file, functors, false);
  });
  Routing RAPI(keys, good_file, functors, false);
  Endpoint empty_endpoint;
  EXPECT_EQ(RAPI.GetStatus(), kNotJoined);
  EXPECT_TRUE(boost::filesystem::remove(good_file));
}

TEST(APITest, BEH_API_ManualBootstrap) {
  asymm::Keys keys1(MakeKeys());
  asymm::Keys keys2(MakeKeys());
  boost::filesystem::path node1_config(fs::unique_path(fs::temp_directory_path() / "test1"));
  boost::filesystem::path node2_config(fs::unique_path(fs::temp_directory_path() / "test2"));
  Functors functors;
  EXPECT_NO_THROW({
    Routing RtAPI(keys1, node1_config, functors, false);
  });
  EXPECT_NO_THROW({
    Routing RtAPI(keys2, node2_config, functors, false);
  });
  Routing R1(keys1, node1_config, functors, false);
  Routing R2(keys2, node2_config, functors, false);
  boost::asio::ip::udp::endpoint empty_endpoint;
  EXPECT_EQ(R1.GetStatus(), kNotJoined);
  EXPECT_EQ(R2.GetStatus(), kNotJoined);
  Endpoint endpoint1g(boost::asio::ip::address_v4::loopback(), 5000);
  Endpoint endpoint2g(boost::asio::ip::address_v4::loopback(), 5001);
  R1.BootStrapFromThisEndpoint(endpoint2g);
  R2.BootStrapFromThisEndpoint(endpoint1g);
  EXPECT_EQ(R1.GetStatus(), kSuccess);
  EXPECT_EQ(R2.GetStatus(), kSuccess);
  EXPECT_TRUE(boost::filesystem::remove(node1_config));
  EXPECT_TRUE(boost::filesystem::remove(node2_config));
}

TEST(APITest, BEH_API_ZeroState) {
  asymm::Keys keys1(MakeKeys());
  asymm::Keys keys2(MakeKeys());
  asymm::Keys keys3(MakeKeys());
  boost::filesystem::path node1_config
                       (fs::unique_path(fs::temp_directory_path() / "test1"));
  boost::filesystem::path node2_config
                       (fs::unique_path(fs::temp_directory_path() / "test2"));
  boost::filesystem::path node3_config
                       (fs::unique_path(fs::temp_directory_path() / "test3"));
  Functors functors;
  Routing R1(keys1, node1_config, functors, false);
  Routing R2(keys2, node2_config, functors, false);
  Routing R3(keys3, node3_config, functors, false);
  Endpoint endpoint1(boost::asio::ip::address_v4::loopback(), 5000);
  Endpoint endpoint2(boost::asio::ip::address_v4::loopback(), 5001);
  Endpoint endpoint3(boost::asio::ip::address_v4::loopback(), 5002);

  auto a1 = std::async(std::launch::async,
                       [&]{return R1.BootStrapFromThisEndpoint(endpoint2, endpoint1);});  // NOLINT (Prakash)
  auto a2 = std::async(std::launch::async,
                       [&]{return R2.BootStrapFromThisEndpoint(endpoint1, endpoint2);});  // NOLINT (Prakash)

  std::this_thread::sleep_for(std::chrono::milliseconds(3000));
  EXPECT_TRUE(a2.get());  // wait for promise !
  EXPECT_TRUE(a1.get());  // wait for promise !

  auto a3 = std::async(std::launch::async,
                       [&]{return R3.BootStrapFromThisEndpoint(endpoint1);});
  std::this_thread::sleep_for(std::chrono::milliseconds(3000));
  EXPECT_TRUE(a3.get());  // wait for future !

  EXPECT_GT(R3.GetStatus(), 0);

  try {
    EXPECT_TRUE(boost::filesystem::remove(node1_config));
    EXPECT_TRUE(boost::filesystem::remove(node2_config));
    EXPECT_TRUE(boost::filesystem::remove(node3_config));
  } catch(const std::exception &e) {
    LOG(kError) << e.what();
  }
}

TEST(APITest, BEH_API_NodeNetwork) {
  const uint16_t network_size(30);
  std::vector<asymm::Keys> network(network_size, MakeKeys());
//  int count(0);
//  for (auto &i : network) {
//    Routing AnodeToBEFixed (i, fs::unique_path(fs::temp_directory_path() / i.identity), nullptr, false);
//  }
  // TODO(dirvine) do this properly !!!

}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
