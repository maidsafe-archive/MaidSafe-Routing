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

#include <chrono>

#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/node_id.h"

#include "maidsafe/passport/types.h"

#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/tests/mock_service.h"
#include "maidsafe/routing/tests/mock_response_handler.h"
#include "maidsafe/routing/tests/mock_network_utils.h"
#include "maidsafe/routing/tests/mock_routing_table.h"
#include "maidsafe/routing/tests/test_utils.h"
#include "maidsafe/routing/client_routing_table.h"
#include "maidsafe/routing/group_change_handler.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/timer.h"

namespace maidsafe {

namespace routing {

namespace test {

class MessageHandlerTest : public testing::Test {
 public:
  MessageHandlerTest()
      : asio_service_(5),
        timer_(asio_service_),
        message_and_caching_functor_(),
        message_(),
        mutex_(),
        cond_var_(),
        messages_received_(0),
        ntable_(),
        table_(),
        utils_(),
        remove_furthest_node_(),
        group_change_handler_(),
        service_(),
        response_handler_(),
        network_statistics_(),
        close_info_() {
    message_and_caching_functor_.message_received = [this](
        const std::string & message, const bool & /*cache_lookup*/, ReplyFunctor reply_functor) {
      MessageReceived(message);
      reply_functor("reply");
    };
    asio_service_.Start();
    NodeId node_id(NodeId::kRandomId);
    network_statistics_.reset(new NetworkStatistics(node_id));
    table_.reset(
        new MockRoutingTable(false, node_id, asymm::GenerateKeyPair(), *network_statistics_));
    ntable_.reset(new ClientRoutingTable(table_->kNodeId()));
    utils_.reset(new MockNetworkUtils(*table_, *ntable_));
    group_change_handler_.reset(new GroupChangeHandler(*table_, *ntable_, *utils_));
    service_.reset(new MockService(*table_, *ntable_, *utils_));
    response_handler_.reset(
        new MockResponseHandler(*table_, *ntable_, *utils_, *group_change_handler_));
    close_info_ = MakeNodeInfoAndKeys().node_info;
    close_info_.node_id = GenerateUniqueRandomId(table_->kNodeId(), 20);
    table_->AddNode(close_info_);
  }

  void MessageReceived(const std::string& /*message*/) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++messages_received_;
    cond_var_.notify_all();
  }

  void ClearMessage(protobuf::Message& message) { message.Clear(); }

 protected:
  AsioService asio_service_;
  Timer<std::string> timer_;
  MessageAndCachingFunctors message_and_caching_functor_;
  protobuf::Message message_;
  std::mutex mutex_;
  std::condition_variable cond_var_;
  int messages_received_;
  std::shared_ptr<ClientRoutingTable> ntable_;
  std::shared_ptr<MockRoutingTable> table_;
  std::shared_ptr<MockNetworkUtils> utils_;
  std::shared_ptr<RemoveFurthestNode> remove_furthest_node_;
  std::shared_ptr<GroupChangeHandler> group_change_handler_;
  std::shared_ptr<MockService> service_;
  std::shared_ptr<MockResponseHandler> response_handler_;
  std::shared_ptr<NetworkStatistics> network_statistics_;
  NodeInfo close_info_;
};

TEST_F(MessageHandlerTest, BEH_HandleInvalidMessage) {
  MessageHandler message_handler(*table_, *ntable_, *utils_, timer_, *remove_furthest_node_,
                                 *group_change_handler_, *network_statistics_);
  // Reset the service and response handler inside the message handler to be mocks
  message_handler.service_ = service_;
  message_handler.response_handler_ = response_handler_;
  protobuf::Message message;
  message.set_hops_to_live(1);

  // MessageHandler should not try to use any network operations during this test.
  EXPECT_CALL(*utils_, SendToClosestNode(testing::_)).Times(0);
  EXPECT_CALL(*utils_, SendToDirect(testing::_, testing::_, testing::_)).Times(0);
  EXPECT_CALL(*service_, FindNodes(testing::_)).Times(0);
  EXPECT_CALL(*service_, Ping(testing::_)).Times(0);
  EXPECT_CALL(*service_, Connect(testing::_)).Times(0);
  EXPECT_CALL(*response_handler_, FindNodes(testing::_)).Times(0);
  EXPECT_CALL(*response_handler_, Ping(testing::_)).Times(0);
  EXPECT_CALL(*response_handler_, Connect(testing::_)).Times(0);
  EXPECT_CALL(*response_handler_, ConnectSuccess(testing::_)).Times(0);
  message_handler.HandleMessage(message);  // Handle uninitialised message
  message.set_routing_message(true);
  message.set_direct(true);
  message.set_request(true);
  message.set_client_node(false);
  message_handler.HandleMessage(message);  // Handle minimally initialised message
  message.set_source_id(NodeId().string());
  message_handler.HandleMessage(message);  // Handle message with invalid source ID
}

TEST_F(MessageHandlerTest, BEH_HandleRelay) {
  MessageHandler message_handler(*table_, *ntable_, *utils_, timer_, *remove_furthest_node_,
                                 *group_change_handler_, *network_statistics_);
  message_handler.service_ = service_;
  message_handler.response_handler_ = response_handler_;

  {  // Handle direct relay request to self
    EXPECT_CALL(*utils_, SendToClosestNode(testing::_)).Times(0);
    EXPECT_CALL(*utils_, SendToDirect(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_CALL(*service_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*service_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*service_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, ConnectSuccess(testing::_)).Times(0);
    protobuf::Message message;
    message.set_hops_to_live(1);
    message.set_routing_message(true);
    message.set_direct(true);
    message.set_request(true);
    message.set_client_node(true);
    message.set_destination_id(table_->kNodeId().string());
    std::string relay_id(RandomString(64)), relay_connection_id(RandomString(64));
    message.set_relay_connection_id(relay_connection_id);
    message.set_relay_id(relay_id);
    message_handler.HandleMessage(message);
  }
  {  // Handle direct relay request to other not in routing table
    NodeId destination_id(GenerateUniqueRandomId(close_info_.node_id, 4));
    EXPECT_CALL(*utils_,
                SendToClosestNode(testing::AllOf(
                    testing::Property(&protobuf::Message::destination_id, destination_id.string()),
                    testing::Property(&protobuf::Message::source_id, table_->kNodeId().string()))))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*utils_, SendToDirect(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_CALL(*service_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*service_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*service_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, ConnectSuccess(testing::_)).Times(0);
    protobuf::Message message;
    message.set_routing_message(true);
    message.set_direct(true);
    message.set_request(true);
    message.set_client_node(true);
    std::string relay_id(RandomString(64)), relay_connection_id(RandomString(64));
    message.set_relay_connection_id(relay_connection_id);
    message.set_relay_id(relay_id);
    message.set_hops_to_live(1);
    message.set_destination_id(destination_id.string());
    message_handler.HandleMessage(message);
  }
  {  // Handle direct relay request to other in routing table
    EXPECT_CALL(
        *utils_,
        SendToClosestNode(testing::AllOf(
            testing::Property(&protobuf::Message::destination_id, close_info_.node_id.string()),
            testing::Property(&protobuf::Message::source_id, table_->kNodeId().string()))))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*utils_, SendToDirect(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_CALL(*service_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*service_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*service_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, ConnectSuccess(testing::_)).Times(0);
    protobuf::Message message;
    message.set_routing_message(true);
    message.set_direct(true);
    message.set_request(true);
    message.set_client_node(true);
    std::string relay_id(RandomString(64)), relay_connection_id(RandomString(64));
    message.set_relay_connection_id(relay_connection_id);
    message.set_relay_id(relay_id);
    message.set_hops_to_live(1);
    message.set_destination_id(close_info_.node_id.string());
    message_handler.HandleMessage(message);
  }
}

TEST_F(MessageHandlerTest, BEH_HandleGroupMessage) {
  MessageHandler message_handler(*table_, *ntable_, *utils_, timer_, *remove_furthest_node_,
                                 *group_change_handler_, *network_statistics_);
  bool result(true);
  message_handler.service_ = service_;
  message_handler.response_handler_ = response_handler_;
  /*{  // Handle group message to self
    protobuf::Message message;
    message.set_hops_to_live(1);
    message.set_routing_message(true);
    message.set_direct(false);
    message.set_request(true);
    message.set_client_node(true);
    EXPECT_CALL(*utils_, SendToClosestNode(testing::AllOf(
                                           testing::Property(&protobuf::Message::destination_id,
                                                             table_->kNodeId()),
                                           testing::Property(&protobuf::Message::source_id,
                                                             table_->kNodeId()))))
                .Times(1).RetiresOnSaturation();
    EXPECT_CALL(*utils_, SendToDirect(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_CALL(*service_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*service_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*service_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, ConnectSuccess(testing::_)).Times(0);
    message.set_source_id(table_->kNodeId());
    message.set_destination_id(table_->kNodeId());
    message_handler.HandleMessage(message);
  }*/
  for (int i(0); i < 3; ++i) {
    NodeInfo node_info = MakeNodeInfoAndKeys().node_info;
    table_->AddNode(node_info);
  }
  if (!table_->Contains(close_info_.node_id)) {
    LOG(kError) << "Re-adding close_info_";
    table_->AddNode(close_info_);
  }
  {  // Handle group message to node in routing table's closest
    protobuf::Message message;
    message.set_hops_to_live(1);
    message.set_routing_message(true);
    message.set_direct(false);
    message.set_request(true);
    message.set_client_node(true);
    message.set_visited(true);
    std::vector<NodeId> closest_nodes(table_->GetClosestNodes(close_info_.node_id, 4));
    closest_nodes.erase(
        std::remove_if(closest_nodes.begin(), closest_nodes.end(),
                       [&](const NodeId & node_id) { return node_id == close_info_.node_id; }),
        closest_nodes.end());
    EXPECT_CALL(*utils_, SendToClosestNode(testing::_)).Times(0);
    EXPECT_CALL(*utils_,
                SendToDirect(testing::AllOf(testing::Property(&protobuf::Message::destination_id,
                                                              closest_nodes.at(0).string()),
                                            testing::Property(&protobuf::Message::direct, true),
                                            testing::Property(&protobuf::Message::request, true)),
                             testing::_, testing::_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*utils_,
                SendToDirect(testing::AllOf(testing::Property(&protobuf::Message::destination_id,
                                                              closest_nodes.at(1).string()),
                                            testing::Property(&protobuf::Message::direct, true),
                                            testing::Property(&protobuf::Message::request, true)),
                             testing::_, testing::_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*utils_,
                SendToDirect(testing::AllOf(testing::Property(&protobuf::Message::destination_id,
                                                              closest_nodes.at(2).string()),
                                            testing::Property(&protobuf::Message::direct, true),
                                            testing::Property(&protobuf::Message::request, true)),
                             testing::_, testing::_))
        .Times(1)
        .RetiresOnSaturation();
    //    EXPECT_CALL(*table_, IsNodeIdInGroupRange(testing::_, testing::_)).Times(1);
    EXPECT_CALL(*service_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*service_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*service_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, ConnectSuccess(testing::_)).Times(0);
    message.set_destination_id(table_->kNodeId().string());
    message.set_replication(4);
    message.set_source_id(RandomString(64));
    message.set_destination_id(close_info_.node_id.string());
    message_handler.HandleMessage(message);
  }
  {  // Handle group message to node not in routing table's closest
    protobuf::Message message;
    message.set_hops_to_live(1);
    message.set_routing_message(true);
    message.set_direct(false);
    message.set_request(true);
    message.set_client_node(true);
    NodeId source_id(NodeId::kRandomId);
    NodeId destination_id(GenerateUniqueRandomId(close_info_.node_id, 4));
    EXPECT_CALL(*utils_,
                SendToClosestNode(testing::AllOf(
                    testing::Property(&protobuf::Message::destination_id, destination_id.string()),
                    testing::Property(&protobuf::Message::source_id, source_id.string()))))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*table_, IsNodeIdInGroupRange(testing::_, testing::_)).Times(0);
    EXPECT_CALL(*utils_, SendToDirect(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_CALL(*service_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*service_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*service_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, ConnectSuccess(testing::_)).Times(0);
    message.set_hops_to_live(1);
    message.set_source_id(source_id.string());
    message.set_destination_id(destination_id.string());
    message_handler.HandleMessage(message);
  }
  {  // Handle FindNodes non-relay group message to destination closest to us
    protobuf::Message message;
    message.set_hops_to_live(1);
    message.set_routing_message(true);
    message.set_direct(false);
    message.set_request(true);
    message.set_client_node(true);
    NodeId source_id(NodeId::kRandomId);
    NodeId destination_id(GenerateUniqueRandomId(table_->kNodeId(), 4));
    std::vector<NodeId> closest_nodes(table_->GetClosestNodes(table_->kNodeId(), 4));
    EXPECT_CALL(*utils_, SendToClosestNode(testing::_)).Times(0);
    EXPECT_CALL(*utils_,
                SendToDirect(testing::AllOf(testing::Property(&protobuf::Message::destination_id,
                                                              closest_nodes.at(0).string()),
                                            testing::Property(&protobuf::Message::direct, true),
                                            testing::Property(&protobuf::Message::request, true)),
                             testing::_, testing::_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*utils_,
                SendToDirect(testing::AllOf(testing::Property(&protobuf::Message::destination_id,
                                                              closest_nodes.at(1).string()),
                                            testing::Property(&protobuf::Message::direct, true),
                                            testing::Property(&protobuf::Message::request, true)),
                             testing::_, testing::_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*utils_,
                SendToDirect(testing::AllOf(testing::Property(&protobuf::Message::destination_id,
                                                              closest_nodes.at(2).string()),
                                            testing::Property(&protobuf::Message::direct, true),
                                            testing::Property(&protobuf::Message::request, true)),
                             testing::_, testing::_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*service_, FindNodes(testing::_))
        .WillOnce(testing::WithArgs<0>(
             testing::Invoke(boost::bind(&MessageHandlerTest::ClearMessage, this, _1))))
        .RetiresOnSaturation();
    //    EXPECT_CALL(*table_, IsNodeIdInGroupRange(testing::_, testing::_)).Times(1);
    EXPECT_CALL(*service_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*service_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, ConnectSuccess(testing::_)).Times(0);
    message.set_hops_to_live(1);
    message.set_source_id(source_id.string());
    message.set_replication(4);
    message.set_type(static_cast<uint32_t>(MessageType::kFindNodes));
    message.set_destination_id(destination_id.string());
    message_handler.HandleMessage(message);
  }
  {  // Handle Node level non-relay group message to destination closest to us
    protobuf::Message message;
    message.set_hops_to_live(1);
    message.set_routing_message(false);
    message.set_direct(false);
    message.set_request(true);
    message.set_client_node(false);
    NodeId source_id(NodeId::kRandomId);
    std::vector<NodeId> closest_nodes(table_->GetClosestNodes(table_->kNodeId(), 4));
    EXPECT_CALL(*utils_,
                SendToClosestNode(testing::AllOf(
                    testing::Property(&protobuf::Message::request, false),
                    testing::Property(&protobuf::Message::source_id, table_->kNodeId().string()),
                    testing::Property(&protobuf::Message::destination_id, source_id.string()))))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*utils_,
                SendToDirect(testing::AllOf(testing::Property(&protobuf::Message::destination_id,
                                                              closest_nodes.at(0).string()),
                                            testing::Property(&protobuf::Message::direct, true),
                                            testing::Property(&protobuf::Message::request, true)),
                             testing::_, testing::_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*utils_,
                SendToDirect(testing::AllOf(testing::Property(&protobuf::Message::destination_id,
                                                              closest_nodes.at(1).string()),
                                            testing::Property(&protobuf::Message::direct, true),
                                            testing::Property(&protobuf::Message::request, true)),
                             testing::_, testing::_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*utils_,
                SendToDirect(testing::AllOf(testing::Property(&protobuf::Message::destination_id,
                                                              closest_nodes.at(2).string()),
                                            testing::Property(&protobuf::Message::direct, true),
                                            testing::Property(&protobuf::Message::request, true)),
                             testing::_, testing::_))
        .Times(1)
        .RetiresOnSaturation();
    //    EXPECT_CALL(*table_, IsNodeIdInGroupRange(testing::_, testing::_)).Times(1);
    EXPECT_CALL(*service_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*service_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*service_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, ConnectSuccess(testing::_)).Times(0);
    message.set_source_id(source_id.string());
    message.set_replication(4);
    message.add_data("DATA");
    NodeId destination_id(GenerateUniqueRandomId(table_->kNodeId(), 4));
    message.set_destination_id(destination_id.string());
    message_handler.set_message_and_caching_functor(message_and_caching_functor_);
    message_handler.HandleMessage(message);
    std::unique_lock<std::mutex> lock(mutex_);
    EXPECT_TRUE(cond_var_.wait_for(lock, std::chrono::seconds(1), [this]()->bool {
      return messages_received_ != 0;
    }));  // NOLINT
    EXPECT_EQ(messages_received_, 1);
    messages_received_ = 0;
  }
  {  // Handle FindNodes relay group message to destination closest to us
    protobuf::Message message;
    message.set_hops_to_live(1);
    message.set_routing_message(true);
    message.set_direct(false);
    message.set_request(true);
    message.set_client_node(true);
    std::vector<NodeId> closest_nodes(table_->GetClosestNodes(table_->kNodeId(), 4));
    EXPECT_CALL(*utils_, SendToClosestNode(testing::_)).Times(0);
    EXPECT_CALL(*utils_,
                SendToDirect(testing::AllOf(testing::Property(&protobuf::Message::destination_id,
                                                              closest_nodes.at(0).string()),
                                            testing::Property(&protobuf::Message::direct, true),
                                            testing::Property(&protobuf::Message::request, true)),
                             testing::_, testing::_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*utils_,
                SendToDirect(testing::AllOf(testing::Property(&protobuf::Message::destination_id,
                                                              closest_nodes.at(1).string()),
                                            testing::Property(&protobuf::Message::direct, true),
                                            testing::Property(&protobuf::Message::request, true)),
                             testing::_, testing::_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*utils_,
                SendToDirect(testing::AllOf(testing::Property(&protobuf::Message::destination_id,
                                                              closest_nodes.at(2).string()),
                                            testing::Property(&protobuf::Message::direct, true),
                                            testing::Property(&protobuf::Message::request, true)),
                             testing::_, testing::_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*service_, FindNodes(testing::_))
        .WillOnce(testing::WithArgs<0>(
             testing::Invoke(boost::bind(&MessageHandlerTest::ClearMessage, this, _1))))
        .RetiresOnSaturation();
    EXPECT_CALL(*service_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*service_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, ConnectSuccess(testing::_)).Times(0);
    std::string relay_id(RandomString(64)), relay_connection_id(RandomString(64));
    message.set_relay_connection_id(relay_connection_id);
    message.set_relay_id(relay_id);
    message.set_replication(4);
    message.set_type(static_cast<uint32_t>(MessageType::kFindNodes));
    NodeId destination_id(GenerateUniqueRandomId(table_->kNodeId(), 4));
    message.set_destination_id(destination_id.string());
    message_handler.HandleMessage(message);
  }
  {  // Handle Node level relay group message to destination closest to us
    protobuf::Message message;
    message.set_hops_to_live(1);
    message.set_routing_message(false);
    message.set_direct(false);
    message.set_request(true);
    message.set_client_node(false);
    NodeId destination_id(GenerateUniqueRandomId(table_->kNodeId(), 4));
    std::vector<NodeId> closest_nodes(table_->GetClosestNodes(table_->kNodeId(), 4));
    EXPECT_CALL(*utils_,
                SendToClosestNode(testing::AllOf(
                    testing::Property(&protobuf::Message::request, false),
                    testing::Property(&protobuf::Message::source_id, table_->kNodeId().string()),
                    testing::Property(&protobuf::Message::destination_id, ""))))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*utils_,
                SendToDirect(testing::AllOf(testing::Property(&protobuf::Message::destination_id,
                                                              closest_nodes.at(0).string()),
                                            testing::Property(&protobuf::Message::direct, true),
                                            testing::Property(&protobuf::Message::request, true)),
                             testing::_, testing::_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*utils_,
                SendToDirect(testing::AllOf(testing::Property(&protobuf::Message::destination_id,
                                                              closest_nodes.at(1).string()),
                                            testing::Property(&protobuf::Message::direct, true),
                                            testing::Property(&protobuf::Message::request, true)),
                             testing::_, testing::_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*utils_,
                SendToDirect(testing::AllOf(testing::Property(&protobuf::Message::destination_id,
                                                              closest_nodes.at(2).string()),
                                            testing::Property(&protobuf::Message::direct, true),
                                            testing::Property(&protobuf::Message::request, true)),
                             testing::_, testing::_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*table_, IsNodeIdInGroupRange(testing::_, result))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*service_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*service_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*service_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, ConnectSuccess(testing::_)).Times(0);
    std::string relay_id(RandomString(64)), relay_connection_id(RandomString(64));
    message.set_relay_connection_id(relay_connection_id);
    message.set_relay_id(relay_id);
    message.set_replication(4);
    message.add_data("DATA");
    message.set_destination_id(destination_id.string());
    message_handler.set_message_and_caching_functor(message_and_caching_functor_);
    message_handler.HandleMessage(message);
    std::unique_lock<std::mutex> lock(mutex_);
    EXPECT_TRUE(cond_var_.wait_for(lock, std::chrono::seconds(1), [this]()->bool {
      return messages_received_ != 0;
    }));  // NOLINT
    EXPECT_EQ(messages_received_, 1);
    messages_received_ = 0;
  }
}

TEST_F(MessageHandlerTest, BEH_HandleNodeLevelMessage) {
  MessageHandler message_handler(*table_, *ntable_, *utils_, timer_, *remove_furthest_node_,
                                 *group_change_handler_, *network_statistics_);
  message_handler.service_ = service_;
  message_handler.response_handler_ = response_handler_;
  protobuf::Message message;
  message.set_hops_to_live(1);
  message.set_routing_message(false);
  message.set_direct(true);
  message.set_request(true);
  message.set_client_node(false);
  NodeId source_id(NodeId::kRandomId);
  message.set_source_id(source_id.string());
  message.set_id(5483);

  {  // Handle node level request to this node
    EXPECT_CALL(*utils_, SendToClosestNode(testing::_)).Times(1).RetiresOnSaturation();
    EXPECT_CALL(*utils_, SendToDirect(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_CALL(*service_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*service_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*service_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, ConnectSuccess(testing::_)).Times(0);
    message.set_destination_id(table_->kNodeId().string());
    message.add_data("DATA");
    message_handler.set_message_and_caching_functor(message_and_caching_functor_);
    message_handler.HandleMessage(message);
    std::unique_lock<std::mutex> lock(mutex_);
    EXPECT_TRUE(cond_var_.wait_for(lock, std::chrono::seconds(1), [this]()->bool {
      return messages_received_ != 0;
    }));  // NOLINT
    EXPECT_EQ(messages_received_, 1);
    messages_received_ = 0;
  }
  {  // Handle node level response to this node
    EXPECT_CALL(*utils_, SendToClosestNode(testing::_)).Times(0);
    EXPECT_CALL(*utils_, SendToDirect(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_CALL(*service_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*service_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*service_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, ConnectSuccess(testing::_)).Times(0);
    message.set_hops_to_live(1);
    message.set_request(false);
    message_handler.HandleMessage(message);
  }
}

TEST_F(MessageHandlerTest, BEH_ClientRoutingTable) {
  auto maid(MakeMaid());
  asymm::Keys keys;
  keys.private_key = maid.private_key();
  keys.public_key = maid.public_key();
  table_.reset(
      new MockRoutingTable(true, NodeId(maid.name()->string()), keys, *network_statistics_));
  table_->AddNode(close_info_);
  MessageHandler message_handler(*table_, *ntable_, *utils_, timer_, *remove_furthest_node_,
                                 *group_change_handler_, *network_statistics_);
  message_handler.service_ = service_;
  message_handler.response_handler_ = response_handler_;
  protobuf::Message message;
  message.set_hops_to_live(2);
  message.set_routing_message(false);
  message.set_direct(true);
  message.set_client_node(true);
  message.set_source_id(RandomString(64));
  message.set_destination_id(maid.name()->string());
  message.add_data("DATA");
  message_handler.set_message_and_caching_functor(message_and_caching_functor_);
  {  // Handle node level request to this node
    EXPECT_CALL(*utils_, SendToClosestNode(testing::_)).Times(0).RetiresOnSaturation();
    EXPECT_CALL(*utils_, SendToDirect(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_CALL(*service_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*service_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*service_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, ConnectSuccess(testing::_)).Times(0);
    message.set_request(true);
    message_handler.HandleMessage(message);
    std::unique_lock<std::mutex> lock(mutex_);
    EXPECT_FALSE(cond_var_.wait_for(lock, std::chrono::seconds(1), [this]()->bool {
      return messages_received_ != 0;
    }));  // NOLINT
    EXPECT_EQ(messages_received_, 0);
    messages_received_ = 0;
  }
  {  // Handle routing FindNodes request to this node
    EXPECT_CALL(*utils_, SendToClosestNode(testing::_)).Times(0);
    EXPECT_CALL(*utils_, SendToDirect(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_CALL(*service_, FindNodes(testing::_))
        .WillOnce(testing::WithArgs<0>(
             testing::Invoke(boost::bind(&MessageHandlerTest::ClearMessage, this, _1))))
        .RetiresOnSaturation();
    EXPECT_CALL(*service_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*service_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, ConnectSuccess(testing::_)).Times(0);
    message.set_request(true);
    message.set_routing_message(true);
    message.set_direct(true);
    message.set_client_node(true);
    message.add_data("DATA");
    message.set_source_id(RandomString(64));
    message.set_destination_id(maid.name()->string());
    message.set_hops_to_live(1);
    message.set_type(static_cast<uint32_t>(MessageType::kFindNodes));
    message_handler.HandleMessage(message);
  }
  {  // Handle routing Connect request to this node
    NodeId peer_id(NodeId::kRandomId), connection_id(NodeId::kRandomId);
    EXPECT_CALL(*utils_, SendToClosestNode(testing::_)).Times(0);
    EXPECT_CALL(*utils_, SendToDirect(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_CALL(*service_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*service_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*service_, Connect(testing::_))
        .WillOnce(testing::WithArgs<0>(
             testing::Invoke(boost::bind(&MessageHandlerTest::ClearMessage, this, _1))))
        .RetiresOnSaturation();
    EXPECT_CALL(*response_handler_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, ConnectSuccess(testing::_)).Times(0);
    protobuf::Message connect_message;
    connect_message.set_request(true);
    connect_message.set_routing_message(true);
    connect_message.set_direct(true);
    connect_message.set_client_node(true);
    connect_message.add_data("DATA");
    connect_message.set_source_id(RandomString(64));
    connect_message.set_destination_id(maid.name()->string());
    connect_message.set_hops_to_live(1);
    connect_message.set_type(static_cast<uint32_t>(MessageType::kConnect));
    message_handler.HandleMessage(connect_message);
  }
  {  // Handle routing Ping request to this node
    EXPECT_CALL(*utils_, SendToClosestNode(testing::_)).Times(0);
    EXPECT_CALL(*utils_, SendToDirect(testing::_, testing::_, testing::_)).Times(0);
    EXPECT_CALL(*service_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*service_, Ping(testing::_))
        .WillOnce(testing::WithArgs<0>(
             testing::Invoke(boost::bind(&MessageHandlerTest::ClearMessage, this, _1))))
        .RetiresOnSaturation();
    EXPECT_CALL(*service_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, FindNodes(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Ping(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, Connect(testing::_)).Times(0);
    EXPECT_CALL(*response_handler_, ConnectSuccess(testing::_)).Times(0);
    message.set_request(true);
    message.set_routing_message(true);
    message.set_hops_to_live(1);
    message.set_direct(true);
    message.set_client_node(true);
    message.add_data("DATA");
    message.set_source_id(RandomString(64));
    message.set_destination_id(maid.name()->string());
    message.set_type(static_cast<uint32_t>(MessageType::kPing));
    message_handler.HandleMessage(message);
  }
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
