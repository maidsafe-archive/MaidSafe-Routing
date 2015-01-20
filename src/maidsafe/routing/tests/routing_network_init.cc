/*  Copyright 2014 MaidSafe.net limited

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


#include <chrono>
#include <tuple>
#include <vector>

#include "asio/ip/udp.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/path.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/sqlite3_wrapper.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing_node.h"
#include "maidsafe/routing/bootstrap_handler.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/tests/utils/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

namespace fs = boost::filesystem;

TEST(RoutingNetworkInit, BEH_ConstructNode) {
  asio::io_service ios;

  passport::Pmid pmid = passport::CreatePmidAndSigner().first;

  LruCache<Identity, SerialisedMessage> cache(0, std::chrono::seconds(0));

  maidsafe::test::TestPath test_dir(maidsafe::test::CreateTestPath("RoutingNetworkInit_BEH_ConstructNode"));

  RoutingNode n(ios, *test_dir / "node.sqlite3", pmid, std::make_shared<RoutingNode::Listener>(cache));

  ios.run();
}

TEST(RoutingNetworkInit, BEH_InitTwo) {
  asio::io_service ios;

  passport::Pmid pmid1 = passport::CreatePmidAndSigner().first;
  passport::Pmid pmid2 = passport::CreatePmidAndSigner().first;

  LruCache<Identity, SerialisedMessage> cache1(0, std::chrono::seconds(0));
  LruCache<Identity, SerialisedMessage> cache2(0, std::chrono::seconds(0));

  maidsafe::test::TestPath test_dir(maidsafe::test::CreateTestPath("RoutingNetworkInit_BEH_InitTwo"));

  RoutingNode n1(ios, *test_dir / "node.sqlite3", pmid1, std::make_shared<RoutingNode::Listener>(cache1));
  RoutingNode n2(ios, *test_dir / "node.sqlite3", pmid2, std::make_shared<RoutingNode::Listener>(cache2));

  ios.run();
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
