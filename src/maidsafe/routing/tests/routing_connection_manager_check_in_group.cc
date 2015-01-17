/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#include <memory>
#include <vector>
#include "boost/asio/io_service.hpp"

#include "maidsafe/common/rsa.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/connection_manager.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/tests/utils/test_utils.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/contact.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST(ConnectionManagerTest, FUNC_AddNodesCheckCloseGroup) {
  rudp::ManagedConnections rudp;
  asio::io_service io_service(1);
  auto our_id(Address(RandomString(Address::kSize)));

  ConnectionManager connection_manager(io_service, rudp, our_id);
  asymm::Keys key(asymm::GenerateKeyPair());
  std::vector<Address> addresses;
  addresses.reserve(RoutingTable::OptimalSize());
  // iterate and filll oruting table
  auto fob(PublicFob());
  for (auto& node : addresses) {
    NodeInfo nodeinfo_to_add(node, fob);
    EXPECT_TRUE(connection_manager.SuggestNodeToAdd(nodeinfo_to_add.id));
    rudp::EndpointPair endpoint_pair;
    endpoint_pair.local = (GetRandomEndpoint());
    endpoint_pair.external = (GetRandomEndpoint());
    connection_manager.AddNode(nodeinfo_to_add, endpoint_pair);
  }
  std::sort(std::begin(addresses), std::end(addresses),
            [our_id](const Address& lhs,
                     const Address& rhs) { return Address::CloserToTarget(lhs, rhs, our_id); });
  auto close_group(connection_manager.OurCloseGroup());
  // no node added as rudp will refuse these connections;
  EXPECT_EQ(0U, close_group.size());
  // EXPECT_EQ(GroupSize, close_group.size());
  // for (size_t i(0); i < GroupSize; ++i)
  //   EXPECT_EQ(addresses.at(i), close_group.at(i).id);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
