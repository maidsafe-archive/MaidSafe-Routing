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

#include <future>
#include <memory>
#include <vector>
#include <future>

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

using Endpoint = asio::ip::udp::endpoint;

struct FutureHandler {
  using Value = boost::optional<CloseGroupDifference>;

  struct State {
    std::promise<Value> promise;
    std::future<Value> future;

    State() : future(promise.get_future()) {}
  };

  std::string what;
  std::shared_ptr<State> state;

  FutureHandler(std::string what) : what(std::move(what)), state(std::make_shared<State>()) {}

  void operator()(asio::error_code error, Value v) const {
    if (error) {
      throw std::runtime_error("operation failed");
    }
    state->promise.set_value(std::move(v));
  }

  Value Get() { return state->future.get(); }
};

NodeInfo GenerateNodeInfo() {
  passport::PublicPmid fob{passport::Pmid(passport::Anpmid())};
  NodeInfo node_info(MakeIdentity(), fob, false);
  return std::move(node_info);
}

EndpointPair LocalEndpointPair(unsigned short port) {
  return EndpointPair(Endpoint(GetLocalIp(), port));
}

TEST(ConnectionManagerTest, FUNC_AddNodes) {
  NodeInfo c1_info = GenerateNodeInfo();
  NodeInfo c2_info = GenerateNodeInfo();
  AsioService asio_service(10);

  ConnectionManager cm1(asio_service.service(), c1_info.id, ConnectionManager::OnReceive(),
                        ConnectionManager::OnConnectionLost());
  ConnectionManager cm2(asio_service.service(), c2_info.id, ConnectionManager::OnReceive(),
                        ConnectionManager::OnConnectionLost());

  FutureHandler cm1_result("cm1");
  FutureHandler cm2_result("cm2");

  cm1.AddNode(c2_info, LocalEndpointPair(cm2.AcceptingPort()), cm1_result);
  cm2.AddNodeAccept(c1_info, LocalEndpointPair(cm1.AcceptingPort()), cm2_result);

  cm1_result.Get();
  cm2_result.Get();

  cm1.Shutdown();
  cm2.Shutdown();
}

TEST(ConnectionManagerTest, FUNC_AddNodesCheckCloseGroup) {
//  boost::asio::io_service io_service;
//  passport::PublicPmid our_public_pmid(passport::CreatePmidAndSigner().first);
//  auto our_id(our_public_pmid.Name());
//  ConnectionManager connection_manager(io_service, our_public_pmid);
//  asymm::Keys key(asymm::GenerateKeyPair());
//  std::vector<Address> addresses(60, MakeIdentity());
//  // iterate and fill routing table
//  auto fob(PublicFob());
//  for (auto& node : addresses) {
//    NodeInfo nodeinfo_to_add(node, fob, true);
//    EXPECT_TRUE(connection_manager.IsManaged(nodeinfo_to_add.id));
//    EndpointPair endpoint_pair;
//    endpoint_pair.local = (GetRandomEndpoint());
//    endpoint_pair.external = (GetRandomEndpoint());
//    connection_manager.AddNode(nodeinfo_to_add, endpoint_pair);
//  }
//  std::sort(std::begin(addresses), std::end(addresses),
//            [our_id](const Address& lhs,
//                     const Address& rhs) { return CloserToTarget(lhs, rhs, our_id); });
//  auto close_group(connection_manager.OurCloseGroup());
//  // no node added as rudp will refuse these connections;
//  EXPECT_EQ(0U, close_group.size());
//  // EXPECT_EQ(GroupSize, close_group.size());
//  // for (size_t i(0); i < GroupSize; ++i)
//  //   EXPECT_EQ(addresses.at(i), close_group.at(i).id);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
