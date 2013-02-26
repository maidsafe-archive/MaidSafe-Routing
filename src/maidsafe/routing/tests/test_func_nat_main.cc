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

#include "maidsafe/common/test.h"
#include "maidsafe/routing/tests/routing_network.h"

int main(int argc, char **argv) {
//  testing::InitGoogleTest(&argc, argv);
  testing::AddGlobalTestEnvironment(new maidsafe::routing::test::NodesEnvironment(
      maidsafe::routing::test::kServerSize, maidsafe::routing::test::kClientSize, 
      static_cast<size_t>(maidsafe::routing::test::kServerSize * 0.25),
      static_cast<size_t>(maidsafe::routing::test::kClientSize / 2)));
//  return RUN_ALL_TESTS();
  return maidsafe::test::ExecuteMain(argc, argv);
}
