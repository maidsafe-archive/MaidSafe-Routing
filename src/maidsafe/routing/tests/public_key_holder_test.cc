/*  Copyright 2013 MaidSafe.net limited

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

#include "maidsafe/common/test.h"

#include "maidsafe/passport/passport.h"

#include "maidsafe/routing/utils.h"
#include "maidsafe/routing/acknowledgement.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/network.h"

#include "maidsafe/routing/tests/test_utils.h"

namespace maidsafe {

namespace routing {

namespace test {

TEST(PublicKeyHolderTest, BEH_AddFindRemoveTimeout) {
  AsioService asio_service(1);
  auto node_details(MakeNodeInfoAndKeysWithPmid(passport::CreatePmidAndSigner().first));
  RoutingTable routing_table(false, node_details.node_info.id, asymm::Keys());
  ClientRoutingTable client_routing_table(node_details.node_info.id);
  Acknowledgement acknowledgment(node_details.node_info.id, asio_service);
  Network network(routing_table, client_routing_table, acknowledgment);
  PublicKeyHolder public_key_holder(asio_service, network);

  EXPECT_FALSE(public_key_holder.Find(NodeId(NodeId::IdType::kRandomId)));

  public_key_holder.Add(node_details.node_info.id, node_details.node_info.public_key);
  EXPECT_TRUE(static_cast<bool>(public_key_holder.Find(node_details.node_info.id)));
  EXPECT_FALSE(public_key_holder.Find(NodeId(NodeId::IdType::kRandomId)));

  Sleep(std::chrono::seconds(Parameters::public_key_holding_time + 1));
  EXPECT_FALSE(public_key_holder.Find(node_details.node_info.id));

  public_key_holder.Add(node_details.node_info.id, node_details.node_info.public_key);
  EXPECT_TRUE(static_cast<bool>(public_key_holder.Find(node_details.node_info.id)));
  public_key_holder.Remove(node_details.node_info.id);
  Sleep(std::chrono::seconds(1));
  EXPECT_FALSE(public_key_holder.Find(node_details.node_info.id));
}

TEST(PublicKeyHolderTest, BEH_MultipleAddFindRemove) {
  AsioService asio_service(2);
  auto node_details(MakeNodeInfoAndKeysWithPmid(passport::CreatePmidAndSigner().first));
  RoutingTable routing_table(false, node_details.node_info.id, asymm::Keys());
  ClientRoutingTable client_routing_table(node_details.node_info.id);
  Acknowledgement acknowledgment(node_details.node_info.id, asio_service);
  Network network(routing_table, client_routing_table, acknowledgment);
  PublicKeyHolder public_key_holder(asio_service, network);
  std::vector<NodeInfoAndPrivateKey> nodes_details;
  const size_t kIterations(100);
  std::vector<std::future<bool>> futures;

  // prepare keys
  for (size_t index(0); index < kIterations; ++index)
    nodes_details.push_back(MakeNodeInfoAndKeysWithPmid(passport::CreatePmidAndSigner().first));

  // store keys
  for (size_t index(0); index < kIterations; ++index)
    futures.emplace_back(
        std::async(std::launch::async,
                   [index, &public_key_holder, &nodes_details]() {
                     return public_key_holder.Add(nodes_details.at(index).node_info.id,
                                                  nodes_details.at(index).node_info.public_key);
                   }));

  for (auto& future : futures)
    EXPECT_TRUE(future.get());

  futures.clear();

  // validate store
  for (size_t index(0); index < kIterations; ++index)
    futures.emplace_back(
        std::async(std::launch::async,
                   [index, &public_key_holder, &nodes_details]()->bool {
                     if (public_key_holder.Find(nodes_details.at(index).node_info.id))
                       return true;
                     return false;
                   }));

  for (auto& future : futures)
    EXPECT_TRUE(future.get());

  futures.clear();

  // remove all enytries
  for (size_t index(0); index < kIterations; ++index)
    futures.emplace_back(
        std::async(std::launch::async,
                   [index, &public_key_holder, &nodes_details]()->bool {
                     public_key_holder.Remove(nodes_details.at(index).node_info.id);
                     return true;
                   }));

  for (auto& future : futures)
    EXPECT_TRUE(future.get());

  Sleep(std::chrono::seconds(1));

  futures.clear();

  // validate remove
  for (size_t index(0); index < kIterations; ++index)
    futures.emplace_back(
        std::async(std::launch::async,
                   [index, &public_key_holder, &nodes_details]()->bool {
                     if (public_key_holder.Find(nodes_details.at(index).node_info.id))
                       return true;
                     return false;
                   }));

  for (auto& future : futures)
    EXPECT_FALSE(future.get());
}

TEST(PublicKeyHolderTest, BEH_MultipleAddFindTimeout) {
  AsioService asio_service(2);
  auto node_details(MakeNodeInfoAndKeysWithPmid(passport::CreatePmidAndSigner().first));
  RoutingTable routing_table(false, node_details.node_info.id, asymm::Keys());
  ClientRoutingTable client_routing_table(node_details.node_info.id);
  Acknowledgement acknowledgment(node_details.node_info.id, asio_service);
  Network network(routing_table, client_routing_table, acknowledgment);
  PublicKeyHolder public_key_holder(asio_service, network);
  std::vector<NodeInfoAndPrivateKey> nodes_details;
  const size_t kIterations(100);
  std::vector<std::future<bool>> futures;

  // prepare keys
  for (size_t index(0); index < kIterations; ++index) {
    nodes_details.push_back(MakeNodeInfoAndKeysWithPmid(passport::CreatePmidAndSigner().first));
    LOG(kVerbose) << index;
  }

  // store keys
  for (size_t index(0); index < kIterations; ++index)
    futures.emplace_back(
        std::async(std::launch::async,
                   [index, &public_key_holder, &nodes_details]() {
                     return public_key_holder.Add(nodes_details.at(index).node_info.id,
                                                  nodes_details.at(index).node_info.public_key);
                   }));

  for (auto& future : futures)
    EXPECT_TRUE(future.get());

  futures.clear();

  // validate store
  for (size_t index(0); index < kIterations; ++index)
    futures.emplace_back(
        std::async(std::launch::async,
                   [index, &public_key_holder, &nodes_details]()->bool {
                     if (public_key_holder.Find(nodes_details.at(index).node_info.id))
                       return true;
                     return false;
                   }));

  for (auto& future : futures)
    EXPECT_TRUE(future.get());

  futures.clear();

  // wait for timeout
  Sleep(std::chrono::seconds(Parameters::public_key_holding_time + 2));

  futures.clear();

  // validate removal on timeout
  for (size_t index(0); index < kIterations; ++index)
    futures.emplace_back(
        std::async(std::launch::async,
                   [index, &public_key_holder, &nodes_details]()->bool {
                     if (public_key_holder.Find(nodes_details.at(index).node_info.id))
                       return true;
                     return false;
                   }));

  for (auto& future : futures)
    EXPECT_FALSE(future.get());
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
