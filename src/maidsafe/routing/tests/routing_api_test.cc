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


namespace maidsafe {
namespace routing {
namespace test {
  
class RoutingTableAPI {
  RoutingTableAPI();
};

// TODO really need transport or a fancy way around not having it :-(

// TEST(RoutingTableAPI, API_BadconfigFile) {
//   Routing RtAPI;
//   boost::filesystem::path bad_file("bad file/ not found/ I hope");
//   EXPECT_FALSE(RtAPI.setConfigFilePath(bad_file));
// }

}  // namespace test
}  // namespace routing
}  // namespace maidsafe
