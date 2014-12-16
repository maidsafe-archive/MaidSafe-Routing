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

#include <utility>

#include "maidsafe/common/crypto.h"

#include "maidsafe/routing/routing_impl.h"
#include "maidsafe/routing/tests/routing_network.h"
#include "maidsafe/routing/tests/test_utils.h"


namespace maidsafe {

namespace routing {

namespace test {

namespace {

// FIXME (Mahmoud) New API don't use response type message even for a node level response
// from Vaults. This test need to be updated to use request only type of messages with
// new API Send() with typed messages. Temporarily added a utility function.
protobuf::Message CreateNodeLevelSingleToSingleResponseMessageProto(
    const SingleToSingleMessage& message) {
  protobuf::Message proto_message;
  proto_message.set_destination_id(message.receiver->string());
  proto_message.set_routing_message(false);
  proto_message.add_data(message.contents);
  proto_message.set_type(static_cast<int32_t>(MessageType::kNodeLevel));

  proto_message.set_cacheable(static_cast<int32_t>(message.cacheable));
  proto_message.set_client_node(false);

  proto_message.set_hops_to_live(Parameters::hops_to_live);

  proto_message.set_direct(true);
  proto_message.set_replication(1);
  proto_message.set_id(RandomUint32());
  proto_message.set_ack_id(RandomUint32());
  proto_message.set_request(false);
  return proto_message;
}

}  // anonymous namespace

class NetworkCache : public GenericNetwork, public testing::Test {
 public:
  virtual void SetUp() override { GenericNetwork::SetUp(); }
  virtual void TearDown() override { Sleep(std::chrono::microseconds(100)); }

 protected:
  void SetGetFromCacheFunctor(NodePtr node, HaveCacheDataFunctor get_from_cache) {
    node->SetGetFromCacheFunctor(get_from_cache);
  }

  void SetStoreToCacheFunctor(NodePtr node, StoreCacheDataFunctor store_cache_data) {
    node->SetStoreInCacheFunctor(store_cache_data);
  }

  std::map<size_t, std::map<std::string, std::string>> network_cache;
};

TEST_F(NetworkCache, FUNC_StoreGet) {
  this->SetUpNetwork(kServerSize);
  for (size_t index(0); index < ClientIndex(); ++index) {
    StoreCacheDataFunctor store_functor([index, this](const std::string& string) {
      auto node_cache_iter(network_cache.find(index));
      std::string identity(crypto::Hash<crypto::SHA512>(string).string());
      auto pair(std::make_pair(identity, string));
      if (node_cache_iter == std::end(network_cache)) {
        std::map<std::string, std::string> value_pair;
        value_pair.insert(pair);
        network_cache.insert(std::make_pair(index, value_pair));
      } else {
        node_cache_iter->second.insert(pair);
      }
    });
    SetStoreToCacheFunctor(nodes_[index], store_functor);
  }

  for (size_t index(0); index < ClientIndex(); ++index) {
    HaveCacheDataFunctor node_get_cache([index, this](const std::string& string,
                                                      ReplyFunctor reply) {
      LOG(kVerbose) << "In Get looking for ";
      auto node_cache_iter(network_cache.find(index));
      if (node_cache_iter == std::end(network_cache)) {
        reply(std::string());
      } else {
        auto inner_iter(node_cache_iter->second.find(string));
        if (inner_iter == std::end(node_cache_iter->second)) {
          reply(std::string());
        } else {
          reply(inner_iter->second);
        }
      }
    });
    SetGetFromCacheFunctor(nodes_[index], node_get_cache);
  }

  std::string content("Dummy content for test purpose");
  SingleToSingleMessage single_to_single_message;
  single_to_single_message.receiver = SingleId(NodeId(RandomString(NodeId::kSize)));
  single_to_single_message.sender = SingleSource(SingleId(nodes_[0]->node_id()));
  single_to_single_message.contents = content;
  single_to_single_message.cacheable = Cacheable::kPut;

  auto message(CreateNodeLevelSingleToSingleResponseMessageProto(single_to_single_message));
//  message.set_cacheable(static_cast<int32_t>(Cacheable::kPut));
//  message.set_request(false);
  LOG(kVerbose) << "Before sending " << message.id();
  nodes_[0]->SendMessage(single_to_single_message.receiver, message);
  Sleep(std::chrono::seconds(2));

  size_t cache_holder_index(0), no_cache_holder_index(0);
  for (size_t index(0); index < ClientIndex(); ++index) {
    auto iter(network_cache.find(index));
    if (iter != std::end(network_cache)) {
      cache_holder_index = index;
      break;
    }
  }

  for (size_t index(0); index < ClientIndex(); ++index) {
    auto iter(network_cache.find(index));
    if (iter == std::end(network_cache)) {
      no_cache_holder_index = index;
      break;
    }
  }

  LOG(kVerbose) << "cache holder index is: " << cache_holder_index
                << "no cache holder index is: " << no_cache_holder_index;

  message.clear_data();
  message.set_id(RandomUint32());
  auto response_functor([&](std::string string) { EXPECT_EQ(string, content); });
  nodes_[no_cache_holder_index]->AddTask(response_functor, 1, message.id());

  message.add_data(crypto::Hash<crypto::SHA512>(single_to_single_message.contents).string());
  message.set_destination_id(nodes_[cache_holder_index]->node_id().string());
  message.set_source_id(nodes_[no_cache_holder_index]->node_id().string());
  message.set_cacheable(static_cast<int32_t>(Cacheable::kGet));
  message.set_request(true);
  nodes_[no_cache_holder_index]->SendMessage(nodes_[cache_holder_index]->node_id(), message);
  Sleep(std::chrono::seconds(5));
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
