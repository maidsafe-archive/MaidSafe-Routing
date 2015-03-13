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

#include "maidsafe/common/rsa.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/connection_manager.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/tests/utils/test_utils.h"
#include "maidsafe/routing/contact.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST(ConnectionManagerTest, FUNC_AddNodesCheckCloseGroup) {
  boost::asio::io_service io_service;
  passport::PublicPmid our_public_pmid(passport::CreatePmidAndSigner().first);
  auto our_id(our_public_pmid.Name());
  ConnectionManager connection_manager(io_service, our_public_pmid);
  asymm::Keys key(asymm::GenerateKeyPair());
  std::vector<Address> addresses(60, MakeIdentity());
  // iterate and fill routing table
  auto fob(PublicFob());
  for (auto& node : addresses) {
    NodeInfo nodeinfo_to_add(node, fob, true);
    EXPECT_TRUE(connection_manager.IsManaged(nodeinfo_to_add.id));
    EndpointPair endpoint_pair;
    endpoint_pair.local = (GetRandomEndpoint());
    endpoint_pair.external = (GetRandomEndpoint());
    connection_manager.AddNode(nodeinfo_to_add, endpoint_pair);
  }
  std::sort(std::begin(addresses), std::end(addresses),
            [our_id](const Address& lhs,
                     const Address& rhs) { return CloserToTarget(lhs, rhs, our_id); });
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
