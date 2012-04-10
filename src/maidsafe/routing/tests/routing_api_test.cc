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

#include <memory>
#include <vector>
#include "boost/filesystem/exception.hpp"
#include "boost/asio.hpp"
#include "maidsafe/common/test.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/log.h"
#include "maidsafe/routing/return_codes.h"


namespace maidsafe {
namespace routing {
namespace test {

static int test_routing_api_node_port(5000);

NodeInfo MakeNodeInfo() {
  NodeInfo node;
  node.node_id = NodeId(RandomString(64));
  asymm::Keys keys;
  asymm::GenerateKeyPair(&keys);
  node.public_key = keys.public_key;
  node.endpoint.address().from_string("192.168.1.1");
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

 TEST(APITest, BadConfigFile) {
  // See bootstrap file tests for further interrogation of these files
  asymm::Keys keys(MakeKeys());
  boost::filesystem::path bad_file("/bad file/ not found/ I hope/");
  boost::filesystem::path good_file
              (fs::unique_path(fs::temp_directory_path() / "test"));
  EXPECT_THROW({Routing RtAPI(keys, bad_file, false);},
              boost::filesystem::filesystem_error)
                                         << "should not accept invalid files";
  EXPECT_NO_THROW({Routing RtAPI(keys, good_file, false);});
  EXPECT_TRUE(WriteFile(good_file, "not a vector of endpoints"));
  EXPECT_NO_THROW({Routing RtAPI(keys, good_file, false);})
                                            << "cannot handle corrupt files";
  EXPECT_TRUE(boost::filesystem::remove(good_file));
}

TEST(APITest, StandAloneNodeNotConnected) {
  asymm::Keys keys(MakeKeys());
  boost::filesystem::path good_file
                       (fs::unique_path(fs::temp_directory_path() / "test"));
  EXPECT_NO_THROW({Routing RtAPI(keys, good_file, false);});
  Routing RAPI(keys, good_file, false);
  boost::asio::ip::udp::endpoint endpoint(RAPI.GetEndPoint());
  boost::asio::ip::udp::endpoint empty_endpoint;
  EXPECT_EQ(endpoint , empty_endpoint);
  EXPECT_EQ(RAPI.GetStatus(), kNotJoined);
  EXPECT_TRUE(boost::filesystem::remove(good_file));
}


TEST(APITest, ManualBootstrap) {
  asymm::Keys keys1(MakeKeys());
  asymm::Keys keys2(MakeKeys());
  boost::filesystem::path node1_config
                       (fs::unique_path(fs::temp_directory_path() / "test1"));
  boost::filesystem::path node2_config
                       (fs::unique_path(fs::temp_directory_path() / "test2"));
  EXPECT_NO_THROW({Routing RtAPI(keys1, node1_config, false);});
  EXPECT_NO_THROW({Routing RtAPI(keys2, node2_config, false);});
  Routing R1(keys1, node1_config, false);
  Routing R2(keys2, node2_config, false);
  boost::asio::ip::udp::endpoint endpoint1d(R1.GetEndPoint());
  boost::asio::ip::udp::endpoint endpoint2d(R2.GetEndPoint());
  boost::asio::ip::udp::endpoint empty_endpoint;
  EXPECT_EQ(endpoint1d , empty_endpoint);
  EXPECT_EQ(R1.GetStatus(), kNotJoined);
  EXPECT_EQ(endpoint2d , empty_endpoint);
  EXPECT_EQ(R2.GetStatus(), kNotJoined);
  boost::asio::ip::udp::endpoint endpoint1g(boost::asio::ip::address_v4::loopback(), 5000);
  boost::asio::ip::udp::endpoint endpoint2g(boost::asio::ip::address_v4::loopback(), 5001);
  R1.BootStrapFromThisEndpoint(endpoint1g);
  R2.BootStrapFromThisEndpoint(endpoint2g);
  EXPECT_EQ(R1.GetStatus(), kNotJoined);
  EXPECT_TRUE(boost::filesystem::remove(node1_config));
  EXPECT_TRUE(boost::filesystem::remove(node2_config));
}



}  // namespace test
}  // namespace routing
}  // namespace maidsafe
