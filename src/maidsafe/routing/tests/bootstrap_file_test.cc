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
#include "maidsafe/routing/bootstrap_file_handler.h"
#include "maidsafe/routing/log.h"


namespace maidsafe {
namespace routing {
namespace test {


TEST(BootStrapFileTest, ReadInvalidFile) {
  routing::BootStrapFile test_handler;
  routing::Parameters::bootstrap_file_path = "/impossible/path/for/us";
  std::vector<transport::Endpoint>empty_vec(test_handler.ReadBootstrapFile());
  EXPECT_FALSE(test_handler.WriteBootstrapFile(empty_vec));
  EXPECT_TRUE(test_handler.ReadBootstrapFile().empty());

}

}  // namespace test
}  // namespace routing
}  // namespace maidsafe
