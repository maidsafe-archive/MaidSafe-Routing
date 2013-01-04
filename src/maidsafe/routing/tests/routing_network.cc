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

#include "maidsafe/routing/tests/routing_network.h"

#include <future>
#include <set>
#include <string>

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing_impl.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/tests/test_utils.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/utils.h"

namespace asio = boost::asio;
namespace ip = asio::ip;
namespace bs = boost::system;

namespace maidsafe {

namespace routing {

namespace test {

bool IsPortAvailable(ip::udp::endpoint endpoint) {
  asio::io_service asio_service;
  asio::ip::udp::socket socket(asio_service);
  bs::error_code ec;
  socket.open(endpoint.protocol(), ec);
  if (ec) {
    LOG(kError) << "Open error: " << ec.message();
  }
  socket.bind(endpoint, ec);
  if (ec) {
    LOG(kError) << "Bind error: " << ec.message();
    return false;
  }
  return true;
}

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // unnamed namespace

size_t GenericNode::next_node_id_(1);

GenericNode::GenericNode(bool client_mode)
    : functors_(),
      id_(0),
      node_info_plus_(),
      mutex_(),
      client_mode_(client_mode),
      anonymous_(false),
      joined_(false),
      expected_(0),
      nat_type_(rudp::NatType::kUnknown),
      endpoint_(),
      messages_(),
      routing_(),
      health_mutex_(),
      health_(0) {
  if (client_mode) {
    auto maid(MakeMaid());
    node_info_plus_.reset(new NodeInfoAndPrivateKey(MakeNodeInfoAndKeysWithMaid(maid)));
    routing_.reset(new Routing(&maid));
  } else {
    auto pmid(MakePmid());
    node_info_plus_.reset(new NodeInfoAndPrivateKey(MakeNodeInfoAndKeysWithPmid(pmid)));
    routing_.reset(new Routing(&pmid));
  }
  endpoint_.address(GetLocalIp());
  endpoint_.port(maidsafe::test::GetRandomPort());
  InitialiseFunctors();
  LOG(kVerbose) << "Node constructor";
  std::lock_guard<std::mutex> lock(mutex_);
  id_ = next_node_id_++;
}

GenericNode::GenericNode(bool client_mode, const rudp::NatType& nat_type)
    : functors_(),
      id_(0),
      node_info_plus_(),
      mutex_(),
      client_mode_(client_mode),
      anonymous_(false),
      joined_(false),
      expected_(0),
      nat_type_(nat_type),
      endpoint_(),
      messages_(),
      routing_(),
      health_mutex_(),
      health_(0) {
  if (client_mode) {
    auto maid(MakeMaid());
    node_info_plus_.reset(new NodeInfoAndPrivateKey(MakeNodeInfoAndKeysWithMaid(maid)));
    routing_.reset(new Routing(&maid));
  } else {
    auto pmid(MakePmid());
    node_info_plus_.reset(new NodeInfoAndPrivateKey(MakeNodeInfoAndKeysWithPmid(pmid)));
    routing_.reset(new Routing(&pmid));
  }
  endpoint_.address(GetLocalIp());
  endpoint_.port(maidsafe::test::GetRandomPort());
  InitialiseFunctors();
  routing_->pimpl_->network_.nat_type_ = nat_type_;
  LOG(kVerbose) << "Node constructor";
  std::lock_guard<std::mutex> lock(mutex_);
  id_ = next_node_id_++;
}

GenericNode::GenericNode(bool client_mode, const NodeInfoAndPrivateKey& node_info)
    : functors_(),
      id_(0),
      node_info_plus_(std::make_shared<NodeInfoAndPrivateKey>(node_info)),
      mutex_(),
      client_mode_(client_mode),
      anonymous_(false),
      joined_(false),
      expected_(0),
      nat_type_(rudp::NatType::kUnknown),
      endpoint_(),
      messages_(),
      routing_(),
      health_mutex_(),
      health_(0) {
  endpoint_.address(GetLocalIp());
  endpoint_.port(maidsafe::test::GetRandomPort());
  InitialiseFunctors();
  if (node_info_plus_->node_info.node_id.IsZero()) {
    anonymous_ = true;
    routing_.reset(new Routing(nullptr));
  } else if (client_mode) {
    auto maid(MakeMaid());
    routing_.reset(new Routing(&maid));
    InjectNodeInfoAndPrivateKey();
  } else {
    auto pmid(MakePmid());
    routing_.reset(new Routing(&pmid));
    InjectNodeInfoAndPrivateKey();
  }
  LOG(kVerbose) << "Node constructor";
  std::lock_guard<std::mutex> lock(mutex_);
  id_ = next_node_id_++;
}

void GenericNode::InjectNodeInfoAndPrivateKey() {
  const_cast<NodeId&>(routing_->pimpl_->kNodeId_) = node_info_plus_->node_info.node_id;
  const_cast<NodeId&>(routing_->pimpl_->routing_table_.kNodeId_) =
      node_info_plus_->node_info.node_id;
  if (!client_mode_) {
    const_cast<NodeId&>(routing_->pimpl_->routing_table_.kConnectionId_) =
        node_info_plus_->node_info.node_id;
  }
  const_cast<asymm::Keys&>(routing_->pimpl_->routing_table_.kKeys_).private_key =
      node_info_plus_->private_key;
  const_cast<asymm::Keys&>(routing_->pimpl_->routing_table_.kKeys_).public_key =
      node_info_plus_->node_info.public_key;
  const_cast<NodeId&>(routing_->pimpl_->non_routing_table_.kNodeId_) =
      node_info_plus_->node_info.node_id;
}

GenericNode::~GenericNode() {}

void GenericNode::InitialiseFunctors() {
  functors_.close_node_replaced = [](const std::vector<NodeInfo>&) {};  // NOLINT (Fraser)
  functors_.message_received = [this] (const std::string& message,
                                       const NodeId&,
                                       const bool& cache_lookup,
                                       ReplyFunctor reply_functor) {
                                 assert(!cache_lookup && "CacheLookup should be disabled for test");
                                 static_cast<void>(cache_lookup);
                                 LOG(kInfo) << id_ << " -- Received: message : "
                                            << message.substr(0, 10);
                                 std::lock_guard<std::mutex> guard(mutex_);
                                 messages_.push_back(message);
                                 if (!IsClient())
                                   reply_functor("Response to >:<" + message);
                               };
  functors_.network_status = [&](const int& health) { SetHealth(health); };  // NOLINT (Alison)
}

int GenericNode::GetStatus() const {
  return /*routing_->GetStatus()*/0;
}

Endpoint GenericNode::endpoint() const {
  return endpoint_;
}

NodeId GenericNode::connection_id() const {
  return node_info_plus_->node_info.connection_id;
}

NodeId GenericNode::node_id() const {
  return node_info_plus_->node_info.node_id;
}

size_t GenericNode::id() const {
  return id_;
}

bool GenericNode::IsClient() const {
  return client_mode_;
}

/* void GenericNode::set_client_mode(const bool& client_mode) {
  client_mode_ = client_mode;
} */

std::vector<NodeInfo> GenericNode::RoutingTable() const {
  return routing_->pimpl_->routing_table_.nodes_;
}

NodeId GenericNode::GetRandomExistingNode() const {
  return routing_->GetRandomExistingNode();
}

std::vector<NodeInfo> GenericNode::ClosestNodes() {
  return routing_->ClosestNodes();
}

// bool GenericNode::IsConnectedToVault(const NodeId& node_id) {
//  return routing_->IsConnectedToVault(node_id);
// }

// bool GenericNode::IsConnectedToClient(const NodeId& node_id) {
//  return routing_->IsConnectedToClient(node_id);
// }

void GenericNode::AddNodeToRandomNodeHelper(const NodeId& node_id) {
  routing_->pimpl_->random_node_helper_.Add(node_id);
}

void GenericNode::RemoveNodeFromRandomNodeHelper(const NodeId& node_id) {
  routing_->pimpl_->random_node_helper_.Remove(node_id);
}

bool GenericNode::NodeSubscriedForGroupUpdate(const NodeId& node_id) {
  LOG(kVerbose) << DebugId(this->node_id()) << " has "
               << routing_->pimpl_->group_change_handler_.update_subscribers_.size()
               << " nodes subscribed for update";
  return (std::find_if(routing_->pimpl_->group_change_handler_.update_subscribers_.begin(),
                      routing_->pimpl_->group_change_handler_.update_subscribers_.end(),
                      [&](const NodeInfo& node) {
                        return node.node_id == node_id;
                      }) != routing_->pimpl_->group_change_handler_.update_subscribers_.end());
}

void GenericNode::Send(const NodeId& destination_id,
                       const std::string& data,
                       const ResponseFunctor& response_functor,
                       const DestinationType& destination_type,
                       const bool& cache) {
    routing_->Send(destination_id, data, response_functor, destination_type, cache);
}

void GenericNode::RudpSend(const NodeId& peer_node_id,
                           const protobuf::Message& message,
                           rudp::MessageSentFunctor message_sent_functor) {
  routing_->pimpl_->network_.RudpSend(peer_node_id, message, message_sent_functor);
}

void GenericNode::SendToClosestNode(const protobuf::Message& message) {
  routing_->pimpl_->network_.SendToClosestNode(message);
}

bool GenericNode::RoutingTableHasNode(const NodeId& node_id) {
  return std::find_if(routing_->pimpl_->routing_table_.nodes_.begin(),
                      routing_->pimpl_->routing_table_.nodes_.end(),
                      [node_id](const NodeInfo& node_info) {
                        return node_id == node_info.node_id;
                      }) !=
         routing_->pimpl_->routing_table_.nodes_.end();
}

bool GenericNode::NonRoutingTableHasNode(const NodeId& node_id) {
  return std::find_if(routing_->pimpl_->non_routing_table_.nodes_.begin(),
                      routing_->pimpl_->non_routing_table_.nodes_.end(),
                      [&node_id](const NodeInfo& node_info) {
                        return (node_id == node_info.node_id);
                      }) !=
         routing_->pimpl_->non_routing_table_.nodes_.end();
}

NodeInfo GenericNode::GetRemovableNode() {
  return routing_->pimpl_->routing_table_.GetRemovableNode();
}

NodeInfo GenericNode::GetNthClosestNode(const NodeId& target_id, uint16_t node_number) {
  return routing_->pimpl_->routing_table_.GetNthClosestNode(target_id, node_number);
}


testing::AssertionResult GenericNode::DropNode(const NodeId& node_id) {
  LOG(kInfo) << " DropNode " << HexSubstr(routing_->pimpl_->routing_table_.kNodeId_.string())
             << " Removes " << HexSubstr(node_id.string());
  auto iter = std::find_if(routing_->pimpl_->routing_table_.nodes_.begin(),
                           routing_->pimpl_->routing_table_.nodes_.end(),
                           [&node_id] (const NodeInfo& node_info) {
                             return (node_id == node_info.node_id);
                           });
  if (iter != routing_->pimpl_->routing_table_.nodes_.end()) {
    LOG(kVerbose) << HexSubstr(routing_->pimpl_->routing_table_.kNodeId_.string())
                  << " Removes " << HexSubstr(node_id.string());
//    routing_->pimpl_->network_.Remove(iter->connection_id);
    routing_->pimpl_->routing_table_.DropNode(iter->connection_id, false);
  } else {
    testing::AssertionFailure() << DebugId(routing_->pimpl_->routing_table_.kNodeId_)
                                << " does not have " << DebugId(node_id) << " in routing table of ";
  }
  return testing::AssertionSuccess();
}


NodeInfo GenericNode::node_info() const {
  return node_info_plus_->node_info;
}

int GenericNode::ZeroStateJoin(const Endpoint& peer_endpoint, const NodeInfo& peer_node_info) {
  return routing_->ZeroStateJoin(functors_, endpoint(), peer_endpoint, peer_node_info);
}

void GenericNode::Join(const std::vector<Endpoint>& peer_endpoints) {
  routing_->Join(functors_, peer_endpoints);
}

void GenericNode::set_joined(const bool node_joined) {
  joined_ = node_joined;
}

bool GenericNode::joined() const {
  return joined_;
}

int GenericNode::expected() {
  return expected_;
}

void GenericNode::set_expected(const int& expected) {
  expected_ = expected;
}

void GenericNode::PrintRoutingTable() {
  LOG(kInfo) << "[" << HexSubstr(node_info_plus_->node_info.node_id.string())
            << "]'s RoutingTable "

            << (IsClient() ? " (Client)" : " (Vault) :")
            << "Routing table size: " << routing_->pimpl_->routing_table_.nodes_.size();
  {
    std::lock_guard<std::mutex> lock(routing_->pimpl_->routing_table_.mutex_);
    for (auto node_info : routing_->pimpl_->routing_table_.nodes_) {
      LOG(kInfo) << "\tNodeId : " << HexSubstr(node_info.node_id.string());
    }
  }
  LOG(kInfo) << "[" << HexSubstr(node_info_plus_->node_info.node_id.string())
            << "]'s Non-RoutingTable : ";
  std::lock_guard<std::mutex> lock(routing_->pimpl_->non_routing_table_.mutex_);
  for (auto node_info : routing_->pimpl_->non_routing_table_.nodes_) {
    LOG(kInfo) << "\tNodeId : " << HexSubstr(node_info.node_id.string());
  }
}

void GenericNode::PrintGroupMatrix() {
  routing_->pimpl_->routing_table_.PrintGroupMatrix();
}

std::string GenericNode::SerializeRoutingTable() {
  std::vector<NodeId> node_list;
  for (auto node_info : routing_->pimpl_->routing_table_.nodes_)
    node_list.push_back(node_info.node_id);
  return SerializeNodeIdList(node_list);
}

size_t GenericNode::MessagesSize() const { return messages_.size(); }

void GenericNode::ClearMessages() {
  std::lock_guard<std::mutex> lock(mutex_);
  messages_.clear();
}

asymm::PublicKey GenericNode::public_key() {
  std::lock_guard<std::mutex> lock(mutex_);
  return node_info_plus_->node_info.public_key;
}

int GenericNode::Health() {
  std::lock_guard<std::mutex> health_lock(health_mutex_);
  return health_;
}

void GenericNode::SetHealth(const int &health) {
  std::lock_guard<std::mutex> health_lock(health_mutex_);
  health_ = health;
}

void GenericNode::PostTaskToAsioService(std::function<void()> functor) {
  routing_->pimpl_->asio_service_.service().post(functor);
}

rudp::NatType GenericNode::nat_type() {
  return routing_->pimpl_->network_.nat_type();
}

GenericNetwork::GenericNetwork()
    : mutex_(),
      fobs_mutex_(),
      bootstrap_endpoints_(),
      bootstrap_path_("bootstrap"),
      public_keys_(),
      client_index_(0),
      nodes_() { LOG(kVerbose) << "RoutingNetwork Constructor"; }

GenericNetwork::~GenericNetwork() {}

void GenericNetwork::SetUp() {
  NodePtr node1(new GenericNode(false)), node2(new GenericNode(false));
  nodes_.push_back(node1);
  nodes_.push_back(node2);
  client_index_ = 2;
  public_keys_.insert(std::make_pair(node1->node_id(), node1->public_key()));
  public_keys_.insert(std::make_pair(node2->node_id(), node2->public_key()));
  // fobs_.push_back(node2->fob());
  SetNodeValidationFunctor(node1);
  SetNodeValidationFunctor(node2);
  LOG(kVerbose) << "Setup started";
  auto f1 = std::async(std::launch::async, [=, &node2] ()->int {
    return node1->ZeroStateJoin(node2->endpoint(), node2->node_info());
  });
  auto f2 = std::async(std::launch::async, [=, &node1] ()->int {
    return node2->ZeroStateJoin(node1->endpoint(), node1->node_info());
  });
  EXPECT_EQ(kSuccess, f2.get());
  EXPECT_EQ(kSuccess, f1.get());
  LOG(kVerbose) << "Setup succeeded";
  bootstrap_endpoints_.clear();
  bootstrap_endpoints_.push_back(node1->endpoint());
  bootstrap_endpoints_.push_back(node2->endpoint());
}

void GenericNetwork::TearDown() {
  std::lock_guard<std::mutex> lock(mutex_);
  GenericNode::next_node_id_ = 1;
}

void GenericNetwork::SetUpNetwork(const size_t& non_client_size, const size_t& client_size) {
  for (size_t index = 2; index < non_client_size; ++index) {
    NodePtr node(new GenericNode(false));
    AddNodeDetails(node);
    LOG(kVerbose) << "Node # " << nodes_.size() << " added to network";
//      node->PrintRoutingTable();
  }
  for (size_t index = 0; index < client_size; ++index) {
    NodePtr node(new GenericNode(true));
    AddNodeDetails(node);
    LOG(kVerbose) << "Node # " << nodes_.size() << " added to network";
  }
  Sleep(boost::posix_time::seconds(1));
  PrintRoutingTables();
//    EXPECT_TRUE(ValidateRoutingTables());
}

void GenericNetwork::AddNode(const bool& client_mode, const NodeId& node_id, bool anonymous) {
  NodeInfoAndPrivateKey node_info;
  if (!anonymous) {
    node_info = MakeNodeInfoAndKeys();
    if (node_id != NodeId())
      node_info.node_info.node_id = node_id;
  }
  NodePtr node(new GenericNode(client_mode, node_info));
  AddNodeDetails(node);
  LOG(kVerbose) << "Node # " << nodes_.size() << " added to network";
//    node->PrintRoutingTable();
}

void GenericNetwork::AddNode(const bool& client_mode, const rudp::NatType& nat_type) {
  NodeInfoAndPrivateKey node_info(MakeNodeInfoAndKeys());
  NodePtr node(new GenericNode(client_mode, nat_type));
  AddNodeDetails(node);
  LOG(kVerbose) << "Node # " << nodes_.size() << " added to network";
//    node->PrintRoutingTable();
}

bool GenericNetwork::RemoveNode(const NodeId& node_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto iter = std::find_if(nodes_.begin(),
                           nodes_.end(),
                           [&node_id] (const NodePtr node) { return node_id == node->node_id(); });  // NOLINT (Dan)
  if (iter == nodes_.end())
    return false;

  if (!(*iter)->IsClient())
    --client_index_;
  nodes_.erase(iter);

  return true;
}

void GenericNetwork::Validate(const NodeId& node_id, GivePublicKeyFunctor give_public_key) {
  if (node_id == NodeId())
    return;
  std::lock_guard<std::mutex> lock(fobs_mutex_);

  auto iter(public_keys_.find(node_id));
  if (!public_keys_.empty())
    EXPECT_NE(iter, public_keys_.end());
  if (iter != public_keys_.end())
    give_public_key((*iter).second);
}

void GenericNetwork::SetNodeValidationFunctor(NodePtr node) {
  node->functors_.request_public_key = [this] (const NodeId& node_id,
                                               GivePublicKeyFunctor give_public_key) {
                                         this->Validate(node_id, give_public_key);
                                       };
}

std::vector<NodeId> GenericNetwork::GroupIds(const NodeId& node_id) {
  std::vector<NodeId> all_ids;
  for (auto node : this->nodes_)
    all_ids.push_back(node->node_id());
  std::partial_sort(all_ids.begin(),
                    all_ids.begin() + Parameters::node_group_size + 1,
                    all_ids.end(),
                    [&](const NodeId& lhs, const NodeId& rhs) {
                      return NodeId::CloserToTarget(lhs, rhs, node_id);
                    });
  return std::vector<NodeId>(all_ids.begin() + static_cast<uint16_t>(all_ids[0] == node_id),
                             all_ids.begin() + Parameters::node_group_size +
                                 static_cast<uint16_t>(all_ids[0] == node_id));
}

void GenericNetwork::PrintRoutingTables() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto node : nodes_)
    node->PrintRoutingTable();
}

bool GenericNetwork::ValidateRoutingTables() {
  std::vector<NodeId> node_ids;
  for (auto node : nodes_) {
    if (!node->IsClient())
      node_ids.push_back(node->node_id());
  }
  for (auto node : nodes_) {
    LOG(kVerbose) << "Reference node: " << HexSubstr(node->node_id().string());
    std::sort(node_ids.begin(),
              node_ids.end(),
              [=] (const NodeId& lhs, const NodeId& rhs)->bool {
                return NodeId::CloserToTarget(lhs, rhs, node->node_id());
              });
    for (auto node_id : node_ids)
      LOG(kVerbose) << HexSubstr(node_id.string());
//      node->PrintRoutingTable();
    auto routing_table(node->RoutingTable());
//      EXPECT_FALSE(routing_table.size() < Parameters::closest_nodes_size);
    std::sort(routing_table.begin(),
              routing_table.end(),
              [&, this] (const NodeInfo& lhs, const NodeInfo& rhs)->bool {
                return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, node->node_id());
              });
    LOG(kVerbose) << "Print ordered RT";
    uint16_t size(std::min(static_cast<uint16_t>(routing_table.size()),
                           Parameters::closest_nodes_size));
    for (auto node_info : routing_table)
      LOG(kVerbose) << HexSubstr(node_info.node_id.string());
    for (auto iter(routing_table.begin());
         iter < routing_table.begin() + size -1;
         ++iter) {
      size_t distance(std::distance(node_ids.begin(), std::find(node_ids.begin(),
                                                                node_ids.end(),
                                                                (*iter).node_id)));
       LOG(kVerbose) << "distance: " << distance << " from "
                     << HexSubstr((*iter).node_id.string());
       if (distance > size)
        return false;
    }
  }
  return true;
}

size_t GenericNetwork::RandomNodeIndex() {
  assert(nodes_.size() > 0);
  return RandomUint32() % nodes_.size();
}

size_t GenericNetwork::RandomClientIndex() {
  assert(nodes_.size() > client_index_);
  size_t client_count(nodes_.size() - client_index_);
  return (RandomUint32() % client_count) + client_index_;
}

size_t GenericNetwork::RandomVaultIndex() {
  assert(nodes_.size() > 2);
  assert(client_index_ > 2);
  return RandomUint32() % client_index_;
}

GenericNetwork::NodePtr GenericNetwork::RandomClientNode() {
  std::lock_guard<std::mutex> lock(mutex_);
  NodePtr random(nodes_.at(RandomClientIndex()));
  return random;
}

GenericNetwork::NodePtr GenericNetwork::RandomVaultNode() {
  std::lock_guard<std::mutex> lock(mutex_);
  NodePtr random(nodes_.at(RandomVaultIndex()));
  return random;
}

void GenericNetwork::RemoveRandomClient() {
  std::lock_guard<std::mutex> lock(mutex_);
  nodes_.erase(nodes_.begin() + RandomClientIndex());
}

void GenericNetwork::RemoveRandomVault() {
  std::lock_guard<std::mutex> lock(mutex_);
  assert(nodes_.size() > 2);
  assert(client_index_ > 2);
  nodes_.erase(nodes_.begin() + 2 + (RandomUint32() % (client_index_ - 2)));  // keep zero state
  --client_index_;
}

void GenericNetwork::ClearMessages() {
  for (auto node : this->nodes_)
    node->ClearMessages();
}

int GenericNetwork::NodeIndex(const NodeId& node_id) {
  for (int index(0); index < static_cast<int>(nodes_.size()); ++index) {
    if (nodes_[index]->node_id() == node_id)
      return index;
  }
  return -1;
}

std::vector<NodeId> GenericNetwork::GetGroupForId(const NodeId& node_id) {
  std::vector<NodeId> group_ids;
  for (auto& node : nodes_) {
    if (!node->IsClient() && (node->node_id() != node_id))
      group_ids.push_back(node->node_id());
  }
  std::partial_sort(group_ids.begin(),
                    group_ids.begin() + Parameters::node_group_size - 1,
                    group_ids.end(),
                    [&](const NodeId& lhs, const NodeId& rhs) {
                      return NodeId::CloserToTarget(lhs, rhs, node_id);
                    });
  return std::vector<NodeId>(group_ids.begin(),
                             group_ids.begin() + Parameters::node_group_size - 1);
}

std::vector<NodeInfo> GenericNetwork::GetClosestNodes(const NodeId& target_id,
                                                      const uint32_t& quantity) {
  std::vector<NodeInfo> closet_nodes;
  for (auto node : nodes_)
    closet_nodes.push_back(node->node_info_plus_->node_info);
  uint32_t size = std::min(quantity + 1, static_cast<uint32_t>(nodes_.size()));
  std::lock_guard<std::mutex> lock(mutex_);
  std::partial_sort(closet_nodes.begin(),
                    closet_nodes.begin() + size,
                    closet_nodes.end(),
                    [&](const NodeInfo& lhs, const NodeInfo& rhs) {
                      return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target_id);
                    });
  return std::vector<NodeInfo>(closet_nodes.begin() + 1, closet_nodes.begin() + size);
}

std::vector<NodeInfo> GenericNetwork::GetClosestVaults(const NodeId& target_id,
                                                       const uint32_t& quantity) {
  std::vector<NodeInfo> closest_nodes;
  for (auto node : nodes_) {
    if (!node->IsClient())
      closest_nodes.push_back(node->node_info());
  }

  uint32_t sort_size(std::min(quantity, static_cast<uint32_t>(closest_nodes.size())));

    std::lock_guard<std::mutex> lock(mutex_);
    std::partial_sort(closest_nodes.begin(),
                      closest_nodes.begin() + sort_size,
                      closest_nodes.end(),
                      [&](const NodeInfo& lhs, const NodeInfo& rhs) {
                        return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, target_id);
                      });

  return std::vector<NodeInfo>(closest_nodes.begin(), closest_nodes.begin() + sort_size);
}

bool GenericNetwork::RestoreComposition() {
  // Intended for use in test SetUp/TearDown
  while (ClientIndex() > kServerSize) {
    RemoveNode(nodes_.at(ClientIndex() - 1)->node_id());
  }
  while (ClientIndex() < kServerSize) {
    AddNode(false, NodeId());
  }
  if (ClientIndex() != kServerSize) {
    LOG(kError) << "Failed to restore number of servers. Actual: " << ClientIndex() <<
                   " Sought: " << kServerSize;
    return false;
  }

  while (nodes_.size() > kNetworkSize) {
    RemoveNode(nodes_.at(nodes_.size() - 1)->node_id());
  }
  while (nodes_.size() < kNetworkSize) {
    AddNode(true, NodeId());
  }
  if (ClientIndex() != kServerSize) {
    LOG(kError) << "Failed to maintain number of servers. Actual: " << ClientIndex() <<
                   " Sought: " << kServerSize;
    return false;
  }
  if (nodes_.size() != kNetworkSize) {
    LOG(kError) << "Failed to restore number of clients. Actual: "
                << nodes_.size() - ClientIndex()
                << " Sought: " << kClientSize;
    return false;
  }
  return true;
}

bool GenericNetwork::WaitForHealthToStabilise() {
  // Intended for use in test SetUp/TearDown
  int i(0);
  bool healthy(false);
  int vault_health(100);
  int client_health(100);
  if (kServerSize <= Parameters::max_routing_table_size)
    vault_health = (kServerSize - 1) * 100 / Parameters::max_routing_table_size;
  if (kServerSize <= Parameters::max_client_routing_table_size)
    client_health = (kServerSize - 1) * 100 /Parameters::max_client_routing_table_size;

  while (i < 10 && !healthy) {
    ++i;
    healthy = true;
    for (auto node: nodes_) {
      int node_health = node->Health();
      if (node->IsClient()) {
        if (node_health != client_health) {
          LOG(kError) << "Bad client health. Expected: "
                      << client_health << " Got: " << node_health;
          healthy = false;
          break;
        }
      } else {
        if (node_health != vault_health) {
          LOG(kError) << "Bad vault health. Expected: "
                      << vault_health << " Got: " << node_health;
          healthy = false;
          break;
        }
      }
    }
    if (!healthy)
      Sleep(boost::posix_time::seconds(1));
  }
  if (!healthy)
    LOG(kError) << "Health failed to stabilise in 10 seconds.";
  return healthy;
}

testing::AssertionResult GenericNetwork::Send(const size_t& messages) {
  NodeId group_id;
  size_t message_id(0), client_size(0), non_client_size(0);
  std::set<size_t> received_ids;
  for (auto node : this->nodes_)
    (node->IsClient()) ? client_size++ : non_client_size++;

  LOG(kVerbose) << "Network node size: " << client_size << " : " << non_client_size;

  size_t messages_count(0),
      expected_messages(non_client_size * (non_client_size - 1 + client_size) * messages);
  std::mutex mutex;
  std::condition_variable cond_var;
  for (size_t index = 0; index < messages; ++index) {
    for (auto source_node : this->nodes_) {
      for (auto dest_node : this->nodes_) {
        auto callable = [&](const std::vector<std::string> &message) {
            if (message.empty())
              return;
            std::lock_guard<std::mutex> lock(mutex);
            messages_count++;
            std::string data_id(message.at(0).substr(message.at(0).find(">:<") + 3,
                message.at(0).find("<:>") - 3 - message.at(0).find(">:<")));
            received_ids.insert(boost::lexical_cast<size_t>(data_id));
            LOG(kVerbose) << "ResponseHandler .... " << messages_count << " msg_id: "
                          << data_id;
            if (messages_count == expected_messages) {
              cond_var.notify_one();
              LOG(kVerbose) << "ResponseHandler .... DONE " << messages_count;
            }
          };
        if (source_node->node_id() != dest_node->node_id()) {
          std::string data(RandomAlphaNumericString(512 * 2^10));
          {
            std::lock_guard<std::mutex> lock(mutex);
            data = boost::lexical_cast<std::string>(++message_id) + "<:>" + data;
          }
          assert(!data.empty() && "Send Data Empty !");
          source_node->Send(NodeId(dest_node->node_id()),
                            data,
                            callable,
                            DestinationType::kDirect,
                            false);
          Sleep(boost::posix_time::microseconds(21));
        }
      }
    }
  }

  std::unique_lock<std::mutex> lock(mutex);
  bool result = cond_var.wait_for(lock, std::chrono::seconds(20),
      [&]()->bool {
        LOG(kInfo) << " message count " << messages_count << " expected "
                   << expected_messages << "\n";
        return messages_count == expected_messages;
      });
  EXPECT_TRUE(result);
  if (!result) {
    for (size_t id(1); id <= expected_messages; ++id) {
      auto iter = received_ids.find(id);
      if (iter == received_ids.end())
        LOG(kVerbose) << "missing id: " << id;
    }
    return testing::AssertionFailure() << "Send operarion timed out: "
                                       << expected_messages - messages_count
                                       << " failed to reply.";
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult GenericNetwork::GroupSend(const NodeId& node_id,
                                                   const size_t& messages,
                                                   uint16_t source_index) {
  assert(static_cast<long>(10 * messages) > 0); // NOLINT (Fraser)
  size_t messages_count(0), expected_messages(messages);
  std::string data(RandomAlphaNumericString((2 ^ 10) * 256));

  std::mutex mutex;
  std::condition_variable cond_var;
  for (size_t index = 0; index < messages; ++index) {
    auto callable = [&] (const std::vector<std::string> message) {
                      if (message.empty())
                        return;
                      std::lock_guard<std::mutex> lock(mutex);
                      messages_count++;
                      LOG(kVerbose) << "ResponseHandler .... " << messages_count;
                      if (messages_count == expected_messages) {
                        cond_var.notify_one();
                        LOG(kVerbose) << "ResponseHandler .... DONE " << messages_count;
                      }
                    };
    this->nodes_[source_index]->Send(node_id, data, callable, DestinationType::kGroup, false);
  }

  std::unique_lock<std::mutex> lock(mutex);
  bool result = cond_var.wait_for(lock,
                                  std::chrono::seconds(10 * messages + 5),
                                  [&] ()->bool {
                                    LOG(kInfo) << " message count " << messages_count
                                               << " expected " << expected_messages << "\n";
                                    return messages_count == expected_messages;
                                  });
  EXPECT_TRUE(result);
  if (!result) {
    return testing::AssertionFailure() << "Send operarion timed out: "
                                       << expected_messages - messages_count
                                       << " failed to reply.";
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult GenericNetwork::Send(const NodeId& node_id) {
  std::set<size_t> received_ids;
  std::mutex mutex;
  std::condition_variable cond_var;
  size_t messages_count(0), message_id(0), expected_messages(0);
  auto node(std::find_if(this->nodes_.begin(), this->nodes_.end(),
                         [&](const std::shared_ptr<GenericNode> node) {
                           return node->node_id() == node_id;
                         }));
  if ((node != this->nodes_.end()) && !((*node)->IsClient()))
    expected_messages = this->nodes_.size() - 1;
  for (auto source_node : this->nodes_) {
    auto callable = [&](const std::vector<std::string> &message) {
      if (message.empty())
        return;
      std::lock_guard<std::mutex> lock(mutex);
      messages_count++;
      std::string data_id(message.at(0).substr(message.at(0).find(">:<") + 3,
          message.at(0).find("<:>") - 3 - message.at(0).find(">:<")));
      received_ids.insert(boost::lexical_cast<size_t>(data_id));
      LOG(kVerbose) << "ResponseHandler .... " << messages_count << " msg_id: "
                    << data_id;
      if (messages_count == expected_messages) {
        cond_var.notify_one();
        LOG(kVerbose) << "ResponseHandler .... DONE " << messages_count;
      }
    };
    if (source_node->node_id() != node_id) {
        std::string data(RandomAlphaNumericString((RandomUint32() % 255 + 1) * 2^10));
        {
          std::lock_guard<std::mutex> lock(mutex);
          data = boost::lexical_cast<std::string>(++message_id) + "<:>" + data;
        }
        source_node->Send(node_id, data, callable, DestinationType::kDirect, false);
    }
  }

  std::unique_lock<std::mutex> lock(mutex);
  bool result = cond_var.wait_for(lock, std::chrono::seconds(20),
      [&]()->bool {
        LOG(kInfo) << " message count " << messages_count << " expected "
                   << expected_messages << "\n";
        return messages_count == expected_messages;
      });
  EXPECT_TRUE(result);
  if (!result) {
    for (size_t id(1); id <= expected_messages; ++id) {
      auto iter = received_ids.find(id);
      if (iter == received_ids.end())
        LOG(kVerbose) << "missing id: " << id;
    }
    return testing::AssertionFailure() << "Send operarion timed out: "
                                       << expected_messages - messages_count
                                       << " failed to reply.";
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult GenericNetwork::Send(std::shared_ptr<GenericNode> source_node,
                                              const NodeId& node_id,
                                              bool no_response_expected) {
  std::mutex mutex;
  std::condition_variable cond_var;
  size_t messages_count(0), expected_messages(0);
  ResponseFunctor callable;
  if (!no_response_expected) {
    expected_messages = std::count_if(this->nodes_.begin(), this->nodes_.end(),
        [=](const std::shared_ptr<GenericNode> node)->bool {
          return node_id == node->node_id();
        });

    callable = [&](const std::vector<std::string> &message) {
      if (message.empty())
        return;
      std::lock_guard<std::mutex> lock(mutex);
      messages_count++;
      LOG(kVerbose) << "ResponseHandler .... " << messages_count;
      if (messages_count == expected_messages) {
        cond_var.notify_one();
        LOG(kVerbose) << "ResponseHandler .... DONE " << messages_count;
      }
    };
  }
  std::string data(RandomAlphaNumericString(512 * 2^10));
  assert(!data.empty() && "Send Data Empty !");
  source_node->Send(node_id, data, callable, DestinationType::kDirect, false);

  if (!no_response_expected) {
    std::unique_lock<std::mutex> lock(mutex);
    bool result = cond_var.wait_for(lock, std::chrono::seconds(20),
        [&]()->bool {
          LOG(kInfo) << " message count " << messages_count << " expected "
                    << expected_messages << "\n";
          return messages_count == expected_messages;
        });
    EXPECT_TRUE(result);
    if (!result) {
      return testing::AssertionFailure() << "Send operarion timed out: "
                                         << expected_messages - messages_count
                                         << " failed to reply.";
    }
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionSuccess();
  }
}

uint16_t GenericNetwork::NonClientNodesSize() const {
  uint16_t non_client_size(0);
  for (auto node : nodes_) {
    if (!node->IsClient())
      non_client_size++;
  }
  return non_client_size;
}

void GenericNetwork::AddNodeDetails(NodePtr node) {
  std::shared_ptr<std::condition_variable> cond_var(new std::condition_variable);
  std::weak_ptr<std::condition_variable> cond_var_weak(cond_var);
  {
    {
      std::lock_guard<std::mutex> fobs_lock(fobs_mutex_);
      public_keys_.insert(std::make_pair(node->node_id(), node->public_key()));
    }
    std::lock_guard<std::mutex> lock(mutex_);
    SetNodeValidationFunctor(node);
    uint16_t node_size(NonClientNodesSize());
    node->set_expected(NetworkStatus(node->IsClient(),
                                     std::min(node_size, Parameters::closest_nodes_size)));
    if (node->IsClient()) {
      nodes_.push_back(node);
    } else {
      nodes_.insert(nodes_.begin() + client_index_, node);
      ++client_index_;
    }
  }
  std::weak_ptr<GenericNode> weak_node(node);
  node->functors_.network_status =
      [cond_var_weak, weak_node] (const int& result) {
        std::shared_ptr<std::condition_variable> cond_var(cond_var_weak.lock());
        NodePtr node(weak_node.lock());
        if (node) {
          node->SetHealth(result);
        }
        if (!cond_var || !node)
          return;

        if (!node->anonymous_) {
          ASSERT_GE(result, kSuccess);
        } else  {
          if (!node->joined()) {
            ASSERT_EQ(result, kSuccess);
          } else if (node->joined()) {
            ASSERT_EQ(result, kAnonymousSessionEnded);
          }
        }
        if ((result == node->expected() && !node->joined()) || node->anonymous_) {
          node->set_joined(true);
          cond_var->notify_one();
        }
      };
  node->Join(bootstrap_endpoints_);

  std::mutex mutex;
  if (!node->joined()) {
    std::unique_lock<std::mutex> lock(mutex);
    auto result = cond_var->wait_for(lock, std::chrono::seconds(20));
    EXPECT_EQ(result, std::cv_status::no_timeout);
    Sleep(boost::posix_time::millisec(1000));
  }
  PrintRoutingTables();
}

std::shared_ptr<GenericNetwork> NodesEnvironment::g_env_ =
  std::shared_ptr<GenericNetwork>(new GenericNetwork());

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
