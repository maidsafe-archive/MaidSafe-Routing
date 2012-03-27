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
#include "maidsafe/common/test.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/transport/managed_connections.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/log.h"


namespace maidsafe {
namespace routing {
namespace test {

class RoutingTableAPI {
  RoutingTableAPI();
};

NodeInfo MakeNodeInfo() {
  NodeInfo node;
  node.node_id = NodeId(RandomString(64));
  asymm::Keys keys;
  asymm::GenerateKeyPair(&keys);
  node.public_key = keys.public_key;
  transport::Port port = 1500;
  transport::IP ip;
  node.endpoint = transport::Endpoint(ip.from_string("192.168.1.1") , port);
  return node;
}

asymm::Keys MakeKeys() {
  NodeInfo node(MakeNodeInfo());
  asymm::Keys keys;
  keys.identity = node.node_id.String();
  keys.public_key = node.public_key;
  return keys;
}

 TEST(RoutingTableAPI, API_BadconfigFile) {
  asymm::Keys keys(MakeKeys());

//   Routing RtAPI(false, keys, false);
//    boost::filesystem::path bad_file("bad file/ not found/ I hope");
//    EXPECT_FALSE(RtAPI.setConfigFilePath(bad_file));

}

}  // namespace test
}  // namespace routing
}  // namespace maidsafe
