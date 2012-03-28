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
#include "maidsafe/common/utils.h"
#include "maidsafe/transport/managed_connections.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/log.h"


namespace maidsafe {
namespace routing {
namespace test {


TEST(RPC, BEH_PingMessageInitialised) {
  // check with assert in debug mode, should NEVER fail
  ASSERT_TRUE(rpcs::Ping(NodeId("david"), "me").IsInitialized());
}

TEST(RPC, BEH_ConnectMessageInitialised) {
  transport::IP ip;
  transport::Endpoint our_endpoint(ip.from_string("192.168.1.1") , 5000);
  ASSERT_TRUE(rpcs::Connect(NodeId("dav"), our_endpoint, "id").IsInitialized());
}

TEST(RPC, BEH_FindNodesMessageInitialised) {
  ASSERT_TRUE(rpcs::FindNodes(NodeId("david")).IsInitialized());
}





}  // namespace test
}  // namespace routing
}  // namespace maidsafe
