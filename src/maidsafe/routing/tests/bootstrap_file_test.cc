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
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/bootstrap_file_handler.h"
#include "maidsafe/routing/log.h"


namespace maidsafe {
namespace routing {
namespace test {



TEST(BootStrapFileTest1, BEH_ReadValidFile) {
//   std::vector<boost::asio::ip::udp::endpoint>vec;
//   rudp::IP ip;
//   vec.push_back(boost::asio::ip::udp::endpoint(ip.from_string("192.168.1.1") , 5000));
// //   (test_handler.ReadBootstrapFile());
//   EXPECT_TRUE(test_handler.WriteBootstrapFile(vec));
//   EXPECT_FALSE(test_handler.ReadBootstrapFile().empty());
//   EXPECT_EQ(test_handler.ReadBootstrapFile().size(), vec.size());
//   EXPECT_EQ(test_handler.ReadBootstrapFile()[0].port, vec[0].port);
//   EXPECT_EQ(test_handler.ReadBootstrapFile()[0].ip, vec[0].ip);
}



}  // namespace test
}  // namespace routing
}  // namespace maidsafe
