/* Copyright 2012 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

#include "maidsafe/routing/tests/routing_network.h"

#include <future>
#include <set>
#include <string>
#include <vector>

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing_impl.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/tests/test_utils.h"
#include "maidsafe/routing/routing.pb.h"
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

GenericNode::GenericNode(bool client_mode, bool has_symmetric_nat, bool non_mutating_client)
    : functors_(),
      id_(0),
      node_info_plus_(),
      mutex_(),
      client_mode_(client_mode),
      joined_(false),
      expected_(0),
      nat_type_(rudp::NatType::kUnknown),
      has_symmetric_nat_(has_symmetric_nat),
      endpoint_(),
      messages_(),
      routing_(),
      health_mutex_(),
      health_(0) {
  assert(!(!client_mode && non_mutating_client) && "Only clients may be non mutating!");
  if (non_mutating_client) {
    node_info_plus_.reset(new NodeInfoAndPrivateKey(MakeNodeInfoAndKeys()));
    routing_.reset(new Routing(node_info_plus_->node_info.node_id));
  } else if (client_mode) {
    auto maid(MakeMaid());
    node_info_plus_.reset(new NodeInfoAndPrivateKey(MakeNodeInfoAndKeysWithMaid(maid)));
    routing_.reset(new Routing(maid));
  } else {
    auto pmid(MakePmid());
    node_info_plus_.reset(new NodeInfoAndPrivateKey(MakeNodeInfoAndKeysWithPmid(pmid)));
    routing_.reset(new Routing(pmid));
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
      joined_(false),
      expected_(0),
      nat_type_(nat_type),
      has_symmetric_nat_(nat_type == rudp::NatType::kSymmetric),
      endpoint_(),
      messages_(),
      routing_(),
      health_mutex_(),
      health_(0) {
  if (client_mode) {
    auto maid(MakeMaid());
    node_info_plus_.reset(new NodeInfoAndPrivateKey(MakeNodeInfoAndKeysWithMaid(maid)));
    routing_.reset(new Routing(maid));
  } else {
    auto pmid(MakePmid());
    node_info_plus_.reset(new NodeInfoAndPrivateKey(MakeNodeInfoAndKeysWithPmid(pmid)));
    routing_.reset(new Routing(pmid));
  }
  endpoint_.address(GetLocalIp());
  endpoint_.port(maidsafe::test::GetRandomPort());
  InitialiseFunctors();
  routing_->pimpl_->network_.nat_type_ = nat_type_;
  LOG(kVerbose) << "Node constructor";
  std::lock_guard<std::mutex> lock(mutex_);
  id_ = next_node_id_++;
}

GenericNode::GenericNode(bool client_mode,
                         const NodeInfoAndPrivateKey& node_info,
                         bool has_symmetric_nat,
                         bool non_mutating_client)
    : functors_(),
      id_(0),
      node_info_plus_(std::make_shared<NodeInfoAndPrivateKey>(node_info)),
      mutex_(),
      client_mode_(client_mode),
      joined_(false),
      expected_(0),
      nat_type_(rudp::NatType::kUnknown),
      has_symmetric_nat_(has_symmetric_nat),
      endpoint_(),
      messages_(),
      routing_(),
      health_mutex_(),
      health_(0) {
  assert(!(!client_mode && non_mutating_client) && "Only clients may be non mutating!");
  endpoint_.address(GetLocalIp());
  endpoint_.port(maidsafe::test::GetRandomPort());
  InitialiseFunctors();
  if (non_mutating_client) {
    routing_.reset(new Routing(node_info_plus_->node_info.node_id));
    InjectNodeInfoAndPrivateKey();
  } else if (client_mode) {
    auto maid(MakeMaid());
    routing_.reset(new Routing(maid));
    InjectNodeInfoAndPrivateKey();
  } else {
    auto pmid(MakePmid());
    routing_.reset(new Routing(pmid));
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
  const_cast<NodeId&>(routing_->pimpl_->client_routing_table_.kNodeId_) =
      node_info_plus_->node_info.node_id;
}

GenericNode::~GenericNode() {}

void GenericNode::InitialiseFunctors() {
  functors_.close_node_replaced = [](const std::vector<NodeInfo>&) {};  // NOLINT (Fraser)
  functors_.message_and_caching.message_received =
      [this] (const std::string& message, const bool& cache_lookup, ReplyFunctor reply_functor) {
          assert(!cache_lookup && "CacheLookup should be disabled for test");
          static_cast<void>(cache_lookup);
          LOG(kInfo) << id_ << " -- Received: message : " << message.substr(0, 10);
          std::lock_guard<std::mutex> guard(mutex_);
          messages_.push_back(message);
          reply_functor(node_id().string() + ">::< response to >:<" + message);
      };
  functors_.network_status = [&](const int& health) { SetHealth(health); };
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

bool GenericNode::HasSymmetricNat() const {
  return has_symmetric_nat_;
}

/* void GenericNode::set_client_mode(const bool& client_mode) {
  client_mode_ = client_mode;
} */

std::vector<NodeInfo> GenericNode::RoutingTable() const {
  return routing_->pimpl_->routing_table_.nodes_;
}

std::vector<NodeInfo> GenericNode::ClosestNodes() {
  return routing_->ClosestNodes();
}

bool GenericNode::IsConnectedVault(const NodeId& node_id) {
  return routing_->IsConnectedVault(node_id);
}

bool GenericNode::IsConnectedClient(const NodeId& node_id) {
  return routing_->IsConnectedClient(node_id);
}

void GenericNode::AddNodeToRandomNodeHelper(const NodeId& node_id) {
  routing_->pimpl_->random_node_helper_.Add(node_id);
}

void GenericNode::RemoveNodeFromRandomNodeHelper(const NodeId& node_id) {
  routing_->pimpl_->random_node_helper_.Remove(node_id);
}

bool GenericNode::NodeSubscribedForGroupUpdate(const NodeId& node_id) {
  auto subscribers(routing_->pimpl_->routing_table_.group_matrix_.GetConnectedPeers());
  std::string log;
  log += DebugId(this->node_id()) + " has "
      + std::to_string(subscribers.size()) + " nodes subscribed for update";
  for (auto& subscriber : subscribers) {
    log += DebugId(subscriber.node_id) + ", ";
  }

  LOG(kVerbose) << log;

  return (std::find_if(subscribers.begin(),
                       subscribers.end(),
                       [&](const NodeInfo& node) {
                         return node.node_id == node_id;
                       }) != subscribers.end());
}

void GenericNode::SetMatrixChangeFunctor(MatrixChangedFunctor group_matrix_functor) {
  functors_.matrix_changed = group_matrix_functor;
}

std::vector<NodeInfo> GenericNode::GetGroupMatrixConnectedPeers() {
  return routing_->pimpl_->routing_table_.group_matrix_.GetConnectedPeers();
}

void GenericNode::SendDirect(const NodeId& destination_id,
                             const std::string& data,
                             const bool& cacheable,
                             ResponseFunctor response_functor) {
  routing_->SendDirect(destination_id, data, cacheable, response_functor);
}

void GenericNode::SendGroup(const NodeId& destination_id,
                            const std::string& data,
                            const bool& cacheable,
                            ResponseFunctor response_functor) {
  routing_->SendGroup(destination_id, data, cacheable, response_functor);
}

std::future<std::vector<NodeId>> GenericNode::GetGroup(const NodeId& info_id) {
  return routing_->GetGroup(info_id);
}

GroupRangeStatus GenericNode::IsNodeIdInGroupRange(const NodeId& node_id) {
  return routing_->IsNodeIdInGroupRange(node_id);
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
  for (auto info : routing_->pimpl_->routing_table_.nodes_)
    LOG(kVerbose) << "RoutingTableHasNode " << DebugId(info.node_id);
  auto node(std::find_if(routing_->pimpl_->routing_table_.nodes_.begin(),
                         routing_->pimpl_->routing_table_.nodes_.end(),
                         [node_id](const NodeInfo& node_info) {
                           return node_id == node_info.node_id;
                         }));
  bool result(node != routing_->pimpl_->routing_table_.nodes_.end());
  LOG(kVerbose) << DebugId(node_id) << ", result: " << result;
  return result;
}

bool GenericNode::ClientRoutingTableHasNode(const NodeId& node_id) {
  return std::find_if(routing_->pimpl_->client_routing_table_.nodes_.begin(),
                      routing_->pimpl_->client_routing_table_.nodes_.end(),
                      [&node_id](const NodeInfo& node_info) {
                        return (node_id == node_info.node_id);
                      }) !=
         routing_->pimpl_->client_routing_table_.nodes_.end();
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
    for (const auto& node_info : routing_->pimpl_->routing_table_.nodes_) {
      LOG(kInfo) << "\tNodeId : " << HexSubstr(node_info.node_id.string());
    }
  }
  LOG(kInfo) << "[" << HexSubstr(node_info_plus_->node_info.node_id.string())
            << "]'s Non-RoutingTable : ";
  std::lock_guard<std::mutex> lock(routing_->pimpl_->client_routing_table_.mutex_);
  for (const auto& node_info : routing_->pimpl_->client_routing_table_.nodes_) {
    LOG(kInfo) << "\tNodeId : " << HexSubstr(node_info.node_id.string());
  }
}

std::vector<NodeId> GenericNode::ReturnRoutingTable() {
  std::vector<NodeId> routing_nodes;
  std::lock_guard<std::mutex> lock(routing_->pimpl_->routing_table_.mutex_);
  for (const auto& node_info : routing_->pimpl_->routing_table_.nodes_)
    routing_nodes.push_back(node_info.node_id);
  return routing_nodes;
}

void GenericNode::PrintGroupMatrix() {
  routing_->pimpl_->routing_table_.PrintGroupMatrix();
}

std::string GenericNode::SerializeRoutingTable() {
  std::vector<NodeId> node_list;
  for (const auto& node_info : routing_->pimpl_->routing_table_.nodes_)
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
  std::lock_guard<std::mutex> lock(routing_->pimpl_->running_mutex_);
  if (routing_->pimpl_->running_)
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
      nat_info_available_(true),
      nodes_() { LOG(kVerbose) << "RoutingNetwork Constructor"; }

GenericNetwork::~GenericNetwork() {
  nat_info_available_ = false;
  for (auto& node : nodes_) {
    node->functors_.request_public_key = [] (const NodeId&,
                                             GivePublicKeyFunctor) {};  // NOLINT (Alison)
  }

  while (!nodes_.empty())
    RemoveNode(nodes_.at(0)->node_id());
}

void GenericNetwork::SetUp() {
  NodePtr node1(new GenericNode(false, false)), node2(new GenericNode(false, false));
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
  std::vector<NodeId> nodes_id;
  for (auto index(this->ClientIndex()); index < this->nodes_.size(); ++index)
    nodes_id.push_back(this->nodes_.at(index)->node_id());
  for (const auto& node_id : nodes_id)
    this->RemoveNode(node_id);
  nodes_id.clear();
  LOG(kVerbose) << "Clients are removed";

  for (const auto& node : this->nodes_)
    if (node->HasSymmetricNat())
      nodes_id.push_back(node->node_id());
  for (const auto& node_id : nodes_id)
    this->RemoveNode(node_id);
  nodes_id.clear();
  LOG(kVerbose) << "Vaults behind symmetric NAT are removed";

  for (const auto& node : this->nodes_)
    nodes_id.insert(nodes_id.begin(), node->node_id());  // reverse order - do bootstraps last
  for (const auto& node_id : nodes_id)
    this->RemoveNode(node_id);

  std::lock_guard<std::mutex> lock(mutex_);
  GenericNode::next_node_id_ = 1;
}

void GenericNetwork::SetUpNetwork(const size_t& total_number_vaults,
                                  const size_t& total_number_clients) {
  SetUpNetwork(total_number_vaults, total_number_clients, 0, 0);
}

void GenericNetwork::SetUpNetwork(const size_t& total_number_vaults,
                                  const size_t& total_number_clients,
                                  const size_t& num_symmetric_nat_vaults,
                                  const size_t& num_symmetric_nat_clients) {
  assert(total_number_vaults >= num_symmetric_nat_vaults + 2);
  assert(total_number_clients >= num_symmetric_nat_clients);

  size_t num_nonsym_nat_vaults(total_number_vaults - num_symmetric_nat_vaults);
  size_t num_nonsym_nat_clients(total_number_clients - num_symmetric_nat_clients);

  for (size_t index(2); index < num_nonsym_nat_vaults; ++index) {
    NodePtr node(new GenericNode(false, false));
    AddNodeDetails(node);
    LOG(kVerbose) << "Node # " << nodes_.size() << " added to network";
//      node->PrintRoutingTable();
  }

  for (size_t index(0); index < num_symmetric_nat_vaults; ++index) {
    NodePtr node(new GenericNode(false, true));
    AddNodeDetails(node);
    LOG(kVerbose) << "Node # " << nodes_.size() << " added to network";
//      node->PrintRoutingTable();
  }

  for (size_t index(0); index < num_nonsym_nat_clients; ++index) {
    NodePtr node(new GenericNode(true, false, RandomUint32() % 2 == 0));
    AddNodeDetails(node);
    LOG(kVerbose) << "Node # " << nodes_.size() << " added to network";
  }

  for (size_t index(0); index < num_symmetric_nat_clients; ++index) {
    NodePtr node(new GenericNode(true, true, RandomUint32() % 2 == 0));
    AddNodeDetails(node);
    LOG(kVerbose) << "Node # " << nodes_.size() << " added to network";
  }

  Sleep(std::chrono::seconds(1));
  PrintRoutingTables();
//    EXPECT_TRUE(ValidateRoutingTables());
}

void GenericNetwork::AddNode(const bool& client_mode,
                             const NodeId& node_id,
                             MatrixChangedFunctor matrix_change_functor) {
  NodeInfoAndPrivateKey node_info;
  node_info = MakeNodeInfoAndKeys();
  node_info.node_info.node_id = node_id;
  NodePtr node(new GenericNode(client_mode, node_info, false, false));
  node->SetMatrixChangeFunctor(matrix_change_functor);
  AddNodeDetails(node);
  LOG(kVerbose) << "Node # " << nodes_.size() << " added to network";
}

void GenericNetwork::AddNode(const bool& client_mode,
                             const NodeId& node_id,
                             const bool& has_symmetric_nat,
                             const bool& non_mutating_client) {
  assert(!(!client_mode && non_mutating_client) && "Only clients may be non mutating!");
  NodeInfoAndPrivateKey node_info;
  node_info = MakeNodeInfoAndKeys();
  node_info.node_info.node_id = node_id;
  NodePtr node(new GenericNode(client_mode, node_info, has_symmetric_nat, non_mutating_client));
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

void GenericNetwork::AddNode(const bool& client_mode, const bool& has_symmetric_nat) {
  NodePtr node(new GenericNode(client_mode, has_symmetric_nat));
  AddNodeDetails(node);
  LOG(kVerbose) << "Node # " << nodes_.size() << " added to network";
//    node->PrintRoutingTable();
}

bool GenericNetwork::RemoveNode(const NodeId& node_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto iter = std::find_if(nodes_.begin(),
                           nodes_.end(),
                           [&node_id] (const NodePtr node) { return node_id == node->node_id(); });
  if (iter == nodes_.end())
    return false;

  if (!(*iter)->IsClient())
    --client_index_;
  nodes_.erase(iter);

  return true;
}

bool GenericNetwork::WaitForNodesToJoin() {
  // TODO(Alison) - tailor max. duration to match number of nodes joining?
  bool all_joined = true;
  uint16_t max(10);
  uint16_t i(0);
  while (i < max) {
    all_joined = true;
    for (uint16_t j(2); j < nodes_.size(); ++j) {
      if (!nodes_.at(j)->joined()) {
        all_joined = false;
        break;
      }
    }
    if (all_joined)
      return true;
    ++i;
    if (i == max)
      return false;
    Sleep(std::chrono::seconds(5));
  }
  return false;
}

void GenericNetwork::Validate(const NodeId& node_id, GivePublicKeyFunctor give_public_key) const {
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
  NodeId own_node_id(node->node_id());
  if (node->HasSymmetricNat()) {
    node->functors_.request_public_key = [this, own_node_id] (const NodeId& node_id,
                                             GivePublicKeyFunctor give_public_key) {
      assert(node_id != own_node_id && "(1) Should not get public key request from own node id!");
      if (!NodeHasSymmetricNat(node_id))
        this->Validate(node_id, give_public_key);
    };
  } else {
    node->functors_.request_public_key = [this, own_node_id] (const NodeId& node_id,
                                             GivePublicKeyFunctor give_public_key) {
      assert(node_id != own_node_id && "(2) Should not get public key request from own node id!");
      this->Validate(node_id, give_public_key);
    };
  }
}

std::vector<NodeId> GenericNetwork::GroupIds(const NodeId& node_id) const {
  std::vector<NodeId> all_ids;
  for (const auto& node : this->nodes_)
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

void GenericNetwork::PrintRoutingTables() const {
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& node : nodes_)
    node->PrintRoutingTable();
}

bool GenericNetwork::ValidateRoutingTables() const {
  std::vector<NodeId> node_ids;
  for (const auto& node : nodes_) {
    if (!node->IsClient())
      node_ids.push_back(node->node_id());
  }
  for (const auto& node : nodes_) {
    LOG(kVerbose) << "Reference node: " << HexSubstr(node->node_id().string());
    std::sort(node_ids.begin(),
              node_ids.end(),
              [=] (const NodeId& lhs, const NodeId& rhs)->bool {
                return NodeId::CloserToTarget(lhs, rhs, node->node_id());
              });
    for (const auto& node_id : node_ids)
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
    for (const auto& node_info : routing_table)
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

uint16_t GenericNetwork::RandomNodeIndex() const {
  assert(!nodes_.empty());
  return static_cast<uint16_t>(RandomUint32() % nodes_.size());
}

uint16_t GenericNetwork::RandomClientIndex() const {
  assert(nodes_.size() > client_index_);
  uint16_t client_count(static_cast<uint16_t>(nodes_.size()) - client_index_);
  return static_cast<uint16_t>(RandomUint32() % client_count) + client_index_;
}

uint16_t GenericNetwork::RandomVaultIndex() const {
  assert(nodes_.size() > 2);
  assert(client_index_ > 2);
  return static_cast<uint16_t>(RandomUint32() % client_index_);
}

GenericNetwork::NodePtr GenericNetwork::RandomClientNode() const {
  std::lock_guard<std::mutex> lock(mutex_);
  NodePtr random(nodes_.at(RandomClientIndex()));
  return random;
}

GenericNetwork::NodePtr GenericNetwork::RandomVaultNode() const {
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
  for (auto& node : this->nodes_)
    node->ClearMessages();
}

int GenericNetwork::NodeIndex(const NodeId& node_id) const {
  for (int index(0); index < static_cast<int>(nodes_.size()); ++index) {
    if (nodes_[index]->node_id() == node_id)
      return index;
  }
  return -1;
}

std::vector<NodeId> GenericNetwork::GetGroupForId(const NodeId& node_id) const {
  std::vector<NodeId> group_ids;
  for (const auto& node : nodes_) {
    if (!node->IsClient() && (node->node_id() != node_id))
      group_ids.push_back(node->node_id());
  }
  std::partial_sort(group_ids.begin(),
                    group_ids.begin() + Parameters::node_group_size,
                    group_ids.end(),
                    [&](const NodeId& lhs, const NodeId& rhs) {
                      return NodeId::CloserToTarget(lhs, rhs, node_id);
                    });
  return std::vector<NodeId>(group_ids.begin(),
                             group_ids.begin() + Parameters::node_group_size);
}

std::vector<NodeInfo> GenericNetwork::GetClosestNodes(const NodeId& target_id,
                                                      const uint32_t& quantity,
                                                      const bool vault_only) const {
  std::vector<NodeInfo> closet_nodes;
  for (const auto& node : nodes_) {
    if (vault_only && node->IsClient())
      continue;
    closet_nodes.push_back(node->node_info_plus_->node_info);
  }
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
                                                       const uint32_t& quantity) const {
  std::vector<NodeInfo> closest_nodes;
  for (const auto& node : nodes_) {
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

void GenericNetwork::ValidateExpectedNodeType(const NodeId& node_id,
                                              const ExpectedNodeType& expected_node_type) const {
  auto itr = std::find_if(this->nodes_.begin(),
                          this->nodes_.end(),
                          [&] (const NodePtr node) {
                            return (node->node_id() == node_id);
                          });
  if (expected_node_type == kExpectVault) {
    if (itr == this->nodes_.end())
      assert(false && "Expected vault, but node_id is not found");
    else if ((*itr)->IsClient())
      assert(false && "Expected vault, but got client");
  } else if (expected_node_type == kExpectClient) {
    if (itr == this->nodes_.end())
      assert(false && "Expected client, but node_id is not found");
    else if (!(*itr)->IsClient())
      assert(false && "Expected client, but got vault");
  } else if (expected_node_type == kExpectDoesNotExist) {
    if (itr != this->nodes_.end())
      assert(false && "Found node, but it shouldn't exist");
  } else {
    assert(false && "Expected node type not recognised.");
  }
}

bool GenericNetwork::RestoreComposition() {
  // Intended for use in test SetUp/TearDown
  while (ClientIndex() > kServerSize) {
    RemoveNode(nodes_.at(ClientIndex() - 1)->node_id());
    Sleep(std::chrono::seconds(1));  // TODO(Alison) - remove once hanging is fixed
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
    Sleep(std::chrono::seconds(1));  // TODO(Alison) - remove once hanging is fixed
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

bool GenericNetwork::WaitForHealthToStabilise() const {
  int i(0);
  bool healthy(false);
  int vault_health(100);
  int client_health(100);
  int vault_symmetric_health(100);
  int client_symmetric_health(100);
  int server_size(static_cast<int>(client_index_));
  if (server_size <= Parameters::max_routing_table_size)
    vault_health = (server_size - 1) * 100 / Parameters::max_routing_table_size;
  if (server_size <= Parameters::max_routing_table_size_for_client)
    client_health = (server_size - 1) * 100 /Parameters::max_routing_table_size_for_client;
  int number_nonsymmetric_vaults(NonClientNonSymmetricNatNodesSize());
  if (number_nonsymmetric_vaults <= Parameters::max_routing_table_size)
    vault_symmetric_health = number_nonsymmetric_vaults * 100 / Parameters::max_routing_table_size;
  if (number_nonsymmetric_vaults <= Parameters::max_routing_table_size_for_client)
    client_symmetric_health =
        number_nonsymmetric_vaults * 100 /Parameters::max_routing_table_size_for_client;

  while (i != 10 && !healthy) {
    LOG(kVerbose) << "Wait iteration number: " << i;
    ++i;
    healthy = true;
    int expected_health;
    std::string error_message;
    for (const auto& node: nodes_) {
      error_message = "[" + DebugId(node->node_id()) + "]";
      int node_health = node->Health();
      if (node->IsClient()) {
        if (node->has_symmetric_nat_) {
          expected_health = client_symmetric_health;
          error_message.append(" Client health (symmetric).");
        } else {
          expected_health = client_health;
          error_message.append(" Client health (not symmetric).");
        }
      } else {
        if (node->has_symmetric_nat_) {
          expected_health = vault_symmetric_health;
          error_message.append(" Vault health (symmetric).");
        } else {
          expected_health = vault_health;
          error_message.append(" Vault health (not symmetric).");
        }
      }
      if (node_health != expected_health) {
        LOG(kError) << "Bad " << error_message << " Expected: "
                    << expected_health << " Got: " << node_health;
        healthy = false;
        break;
      }
    }
    if (!healthy)
      Sleep(std::chrono::seconds(1));
  }
  if (!healthy)
    LOG(kError) << "Health failed to stabilise in 10 seconds.";
  return healthy;
}

bool GenericNetwork::WaitForHealthToStabiliseInLargeNetwork() const {
  // TODO(Alison) - check values for clients
  int server_size(static_cast<int>(client_index_));
  assert(server_size > Parameters::max_routing_table_size);
  int number_nonsymmetric_vaults(NonClientNonSymmetricNatNodesSize());
  assert(number_nonsymmetric_vaults >= Parameters::greedy_fraction);

  int i(0);
  bool healthy(false);
  int vault_health(Parameters::greedy_fraction);
  int vault_symmetric_health(Parameters::greedy_fraction);
  int client_health(100);
  int client_symmetric_health(100);
  if (server_size <= Parameters::max_client_routing_table_size)
    client_health = (server_size - 1) * 100 /Parameters::max_client_routing_table_size;
  if (number_nonsymmetric_vaults <= Parameters::max_client_routing_table_size)
    client_symmetric_health =
        number_nonsymmetric_vaults * 100 /Parameters::max_client_routing_table_size;

  while (i != 10 && !healthy) {
    LOG(kVerbose) << "Wait iteration number: " << i;
    ++i;
    healthy = true;
    int expected_health;
    std::string error_message;
    for (const auto& node: nodes_) {
      error_message = "[" + DebugId(node->node_id()) + "]";
      int node_health = node->Health();
      if (node->IsClient()) {
        if (node->has_symmetric_nat_) {
          expected_health = client_symmetric_health;
          error_message.append(" Client health (symmetric).");
        } else {
          expected_health = client_health;
          error_message.append(" Client health (not symmetric).");
        }
      } else {
        if (node->has_symmetric_nat_) {
          expected_health = vault_symmetric_health;
          error_message.append(" Vault health (symmetric).");
        } else {
          expected_health = vault_health;
          error_message.append(" Vault health (not symmetric).");
        }
      }
      if (node_health < expected_health) {
        LOG(kError) << "Bad " << error_message << " Expected at least: "
                    << expected_health << " Got: " << node_health;
        healthy = false;
        break;
      }
    }
    if (!healthy)
      Sleep(std::chrono::seconds(1));
  }
  if (!healthy)
    LOG(kError) << "Health failed to stabilise in 10 seconds.";
  return healthy;
}

bool GenericNetwork::NodeHasSymmetricNat(const NodeId& node_id) const {
  if (!nat_info_available_)
    return false;

  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& node : nodes_) {
    if (node->node_id() == node_id) {
      return node->HasSymmetricNat();
    }
  }
  LOG(kError) << "Couldn't find node_id";
  return false;
}

testing::AssertionResult GenericNetwork::CheckGroupMatrixUniqueNodes(const uint16_t& check_length) {
  bool success(true);
  for (const auto& node : this->nodes_) {
    std::vector<NodeInfo> nodes_from_matrix(node->ClosestNodes());
    if (nodes_from_matrix.size() < check_length)
      return testing::AssertionFailure();
    nodes_from_matrix.resize(check_length);
    std::vector<NodeInfo> nodes_from_network(this->GetClosestVaults(node->node_id(),
                                                                    check_length));
    if (nodes_from_network.size() != check_length)
      return testing::AssertionFailure();
    for (uint16_t i(0); i < check_length; ++i) {
      EXPECT_EQ(nodes_from_matrix.at(i).node_id, nodes_from_network.at(i).node_id)
          << "Index " << i << " from matrix: " << DebugId(nodes_from_matrix.at(i).node_id)
          << "\t\tIndex " << i << " from network: "  << DebugId(nodes_from_network.at(i).node_id);
      if (nodes_from_matrix.at(i).node_id != nodes_from_network.at(i).node_id)
        success = false;
    }
  }
  if (success)
    return testing::AssertionSuccess();

  return testing::AssertionFailure();
}

testing::AssertionResult GenericNetwork::SendDirect(const size_t& repeats, size_t message_size) {
  assert(repeats > 0);
  size_t total_num_nodes(this->nodes_.size());

  std::shared_ptr<std::mutex> response_mutex(std::make_shared<std::mutex>());
  std::shared_ptr<std::condition_variable> cond_var(std::make_shared<std::condition_variable>());
  std::shared_ptr<uint16_t> reply_count(std::make_shared<uint16_t>(0)),
      expected_count(std::make_shared<uint16_t>(static_cast<uint16_t>(
      repeats * total_num_nodes * total_num_nodes)));
  std::shared_ptr<bool> failed(std::make_shared<bool>(false));

  for (size_t repeat = 0; repeat < repeats; ++repeat) {
    for (const auto& dest : this->nodes_) {
      for (const auto& src : this->nodes_) {
        std::string data(RandomAlphaNumericString(message_size));
        assert(!data.empty() && "Send Data Empty !");
        ResponseFunctor response_functor;
        if (dest->IsClient() &&
                (dest->RoutingTableHasNode(src->node_id()) || dest->node_id() == src->node_id())) {
          response_functor = [response_mutex, cond_var, reply_count, expected_count,
               failed](std::string reply) {
            std::lock_guard<std::mutex> lock(*response_mutex);
            ++(*reply_count);
            EXPECT_FALSE(reply.empty());
            if (reply.empty()) {
              *failed = true;
              if (*reply_count == *expected_count)
                cond_var->notify_one();
              return;
            }
            if (*reply_count == *expected_count)
              cond_var->notify_one();
          };
        } else if (dest->IsClient() && !dest->RoutingTableHasNode(src->node_id())) {
          response_functor = [response_mutex, cond_var, reply_count, expected_count,
               failed](std::string reply) {
            std::lock_guard<std::mutex> lock(*response_mutex);
            ++(*reply_count);
            EXPECT_TRUE(reply.empty());
            if (!reply.empty()) {
              *failed = true;
              if (*reply_count == *expected_count)
                cond_var->notify_one();
              return;
            }
            if (*reply_count == *expected_count)
              cond_var->notify_one();
          };
        } else {
          std::shared_ptr<NodeId> expected_replier(std::make_shared<NodeId>(dest->node_id()));
          response_functor = [response_mutex, cond_var, reply_count, expected_count, failed,
              expected_replier](std::string reply) {
            std::lock_guard<std::mutex> lock(*response_mutex);
            ++(*reply_count);
//            EXPECT_FALSE(reply.empty());
            if (reply.empty()) {
              *failed = true;
              if (*reply_count == *expected_count)
                cond_var->notify_one();
              return;
            }
            try {
              NodeId replier(reply.substr(0, reply.find(">::<")));
              EXPECT_EQ(replier, *expected_replier);
              if (replier != *expected_replier) {
                *failed = false;
                if (*reply_count == *expected_count)
                  cond_var->notify_one();
                return;
              }
              // TODO(Alison) - check data
            } catch(const std::exception& ex) {
              EXPECT_TRUE(false) << "Got message with invalid replier ID. Exception: " << ex.what();
              *failed = true;
              if (*reply_count == *expected_count)
                cond_var->notify_one();
              return;
            }
            if (*reply_count == *expected_count)
              cond_var->notify_one();
          };
        }
        src->SendDirect(dest->node_id(), data, false, response_functor);
      }
    }
  }

  std::unique_lock<std::mutex> lock(*response_mutex);
  if (!cond_var->wait_for(lock, std::chrono::seconds(15 * (nodes_.size()) * (nodes_.size() - 1)),
                          [reply_count, expected_count]() {
                            return *reply_count == *expected_count;
                          })) {
    EXPECT_TRUE(false) << "Didn't get reply within allowed time!";
    return testing::AssertionFailure();
  }

  if (*failed)
    return testing::AssertionFailure();
  return testing::AssertionSuccess();
}

struct SendGroupMonitor {
  explicit SendGroupMonitor(const std::vector<NodeId>& expected_ids)
    : response_count(0), expected_ids(expected_ids) {}

  uint16_t response_count;
  std::vector<NodeId> expected_ids;
};

testing::AssertionResult GenericNetwork::SendGroup(const NodeId& target_id,
                                                   const size_t& repeats,
                                                   uint16_t source_index,
                                                   size_t message_size) {
  LOG(kVerbose) << "Doing SendGroup from " << DebugId(nodes_.at(source_index)->node_id())
                << " to " << DebugId(target_id);
  assert(repeats > 0);
  std::shared_ptr<std::mutex> response_mutex(std::make_shared<std::mutex>());
  std::shared_ptr<std::condition_variable> cond_var(std::make_shared<std::condition_variable>());
  std::string data(RandomAlphaNumericString(message_size));
  std::shared_ptr<uint16_t> reply_count(std::make_shared<uint16_t>(0)),
      expected_count(std::make_shared<uint16_t>(static_cast<uint16_t>(Parameters::node_group_size *
                                                                      repeats)));
  std::shared_ptr<bool> failed(std::make_shared<bool>(false));

  std::vector<NodeId> target_group(this->GetGroupForId(target_id));
  for (uint16_t repeat(0); repeat < repeats; ++repeat) {
    std::shared_ptr<SendGroupMonitor> monitor(std::make_shared<SendGroupMonitor>(target_group));
    monitor->response_count = 0;
    ResponseFunctor response_functor = [response_mutex, cond_var, failed, reply_count,
        expected_count, target_id, monitor, repeat] (std::string reply) {
      std::lock_guard<std::mutex> lock(*response_mutex);
      ++(*reply_count);
      monitor->response_count += 1;
      if (monitor->response_count > Parameters::node_group_size) {
        EXPECT_TRUE(false) << "Received too many replies: " << monitor->response_count;
        *failed = true;
        if (*reply_count == *expected_count)
          cond_var->notify_one();
        return;
      }
      EXPECT_FALSE(reply.empty());
      if (reply.empty()) {
        LOG(kError) << "Got empty reply for SendGroup to target: " << DebugId(target_id);
        *failed = true;
        if (*reply_count == *expected_count)
          cond_var->notify_one();
        return;
      }
      // TODO(Alison) - check data in reply
      try {
        NodeId replier(reply.substr(0, reply.find(">::<")));
        LOG(kInfo) << "Got reply from: " << DebugId(replier)
                   << " for SendGroup to target: " << DebugId(target_id);
        bool valid_replier(std::find(monitor->expected_ids.begin(),
                                     monitor->expected_ids.end(), replier) !=
            monitor->expected_ids.end());
        monitor->expected_ids.erase(std::remove_if(monitor->expected_ids.begin(),
                                                   monitor->expected_ids.end(),
                                                   [&](const NodeId& node_id) {
                                                     return node_id == replier;
                                                   }), monitor->expected_ids.end());
      std::string output("SendGroup to target " + DebugId(target_id) + " awaiting replies from: ");
      for (const auto& node_id : monitor->expected_ids) {
        output.append("\t");
        output.append(DebugId(node_id));
      }
      LOG(kVerbose) << output;
        if (!valid_replier) {
          EXPECT_TRUE(false) << "Got unexpected reply from " << DebugId(replier)
                             << "\t (for target: " << DebugId(target_id) << ")";
          *failed = true;
        }
      } catch(const std::exception& /*ex*/) {
        EXPECT_TRUE(false) << "Reply contained invalid node ID.";
        *failed = true;
      }
      if (*reply_count == *expected_count)
        cond_var->notify_one();
    };

    this->nodes_.at(source_index)->SendGroup(target_id, data, false, response_functor);
  }

  std::unique_lock<std::mutex> lock(*response_mutex);
  if (!cond_var->wait_for(lock, std::chrono::seconds(15 * repeats),
                          [reply_count, expected_count]() {
                            return *reply_count == *expected_count;
                          })) {
    EXPECT_TRUE(false) << "Didn't get replies within allowed time!";
    return testing::AssertionFailure();
  }

  if (*failed)
    return testing::AssertionFailure();
  return testing::AssertionSuccess();
}

testing::AssertionResult GenericNetwork::SendDirect(const NodeId& destination_node_id,
                                                    const ExpectedNodeType& destination_node_type) {
  ValidateExpectedNodeType(destination_node_id, destination_node_type);

  std::shared_ptr<std::mutex> response_mutex(std::make_shared<std::mutex>());
  std::shared_ptr<std::condition_variable> cond_var(std::make_shared<std::condition_variable>());
  std::shared_ptr<uint16_t> reply_count(std::make_shared<uint16_t>(0)),
      expected_count(std::make_shared<uint16_t>(static_cast<uint16_t>(this->nodes_.size())));
  std::shared_ptr<bool> failed(std::make_shared<bool>(false));

  size_t message_index(0);
  for (const auto& src : this->nodes_) {
    ResponseFunctor response_functor;
    std::string data(std::to_string(message_index) + "<:>" +
                     RandomAlphaNumericString((RandomUint32() % 255 + 1) * 2^10));
    if (destination_node_type == ExpectedNodeType::kExpectVault) {
      response_functor = [response_mutex, cond_var, reply_count, expected_count, failed,
          message_index, destination_node_id](std::string reply) {
        std::lock_guard<std::mutex> lock(*response_mutex);
        ++(*reply_count);
        EXPECT_FALSE(reply.empty());
        if (reply.empty()) {
          *failed = true;
          if (*reply_count == *expected_count)
            cond_var->notify_one();
          return;
        }
        try {
          std::string data_index(reply.substr(reply.find(">:<") + 3,
                                              reply.find("<:>") - 3 - reply.find(">:<")));
          NodeId replier(reply.substr(0, reply.find(">::<")));
          EXPECT_EQ(replier, destination_node_id);
          EXPECT_EQ(atoi(data_index.c_str()), message_index);
          if (replier != destination_node_id) {
            *failed = false;
            if (*reply_count == *expected_count)
              cond_var->notify_one();
            return;
          }
          // TODO(Alison) - check data
        } catch(const std::exception& ex) {
          EXPECT_TRUE(false) << "Got message with invalid replier ID. Exception: " << ex.what();
          *failed = true;
          if (*reply_count == *expected_count)
            cond_var->notify_one();
          return;
        }
        if (*reply_count == *expected_count)
          cond_var->notify_one();
      };
    } else {
      NodePtr dest;
      for (size_t index(0); index < nodes_.size(); ++index) {
        if (nodes_[index]->node_id() == destination_node_id) {
          dest = nodes_[index];
          break;
        }
      }
      if ((dest != nullptr) && (dest->RoutingTableHasNode(src->node_id()) ||
              dest->node_id() == src->node_id())) {
        response_functor = [response_mutex, cond_var, reply_count, expected_count,
            failed](std::string reply) {
          std::lock_guard<std::mutex> lock(*response_mutex);
          ++(*reply_count);
          EXPECT_FALSE(reply.empty());
          if (reply.empty()) {
            *failed = true;
            if (*reply_count == *expected_count)
              cond_var->notify_one();
            return;
          }
          if (*reply_count == *expected_count)
            cond_var->notify_one();
        };
      } else if (dest != nullptr) {
        response_functor = [response_mutex, cond_var, reply_count, expected_count,
            failed](std::string reply) {
          std::lock_guard<std::mutex> lock(*response_mutex);
          ++(*reply_count);
          EXPECT_TRUE(reply.empty());
          if (!reply.empty()) {
            *failed = true;
            if (*reply_count == *expected_count)
              cond_var->notify_one();
            return;
          }
          if (*reply_count == *expected_count)
            cond_var->notify_one();
        };
      }
    }
    src->SendDirect(destination_node_id, data, false, response_functor);
    ++message_index;
  }

  std::unique_lock<std::mutex> lock(*response_mutex);
  if (!cond_var->wait_for(lock, std::chrono::seconds(15),
                          [reply_count, expected_count]() {
                            return *reply_count == *expected_count;
                          })) {
//    EXPECT_TRUE(false) << "Didn't get reply within allowed time!";
    return testing::AssertionFailure();
  }

  if (*failed)
    return testing::AssertionFailure();
  return testing::AssertionSuccess();
}

testing::AssertionResult GenericNetwork::SendDirect(std::shared_ptr<GenericNode> source_node,
                                                    const NodeId& destination_node_id,
                                                    const ExpectedNodeType& destination_node_type) {
  ValidateExpectedNodeType(destination_node_id, destination_node_type);

  std::shared_ptr<std::mutex> response_mutex(std::make_shared<std::mutex>());
  std::shared_ptr<std::condition_variable> cond_var(std::make_shared<std::condition_variable>());
  std::shared_ptr<bool> failed(std::make_shared<bool>(false));

  std::string data(RandomAlphaNumericString(512 * 2^10));
  assert(!data.empty() && "Send Data Empty !");
  ResponseFunctor response_functor;
  if (destination_node_type == kExpectVault) {
    response_functor = [response_mutex, cond_var, failed, destination_node_id](std::string reply) {
      std::lock_guard<std::mutex> lock(*response_mutex);
      EXPECT_FALSE(reply.empty());
      // TODO(Alison) - compare reply to sent data
      if (reply.empty())
        *failed = true;
      try {
        NodeId replier_id(reply.substr(0, reply.find(">::<")));
        EXPECT_EQ(replier_id, destination_node_id);
        if (replier_id != destination_node_id)
          *failed = true;
      } catch(const std::exception* /*ex*/) {
        EXPECT_TRUE(false) << "Reply contained invalid node ID!";
        *failed = true;
      }
      cond_var->notify_one();
    };
  } else {
    response_functor = [response_mutex, cond_var, failed](std::string reply) {
      std::lock_guard<std::mutex> lock(*response_mutex);
//      EXPECT_TRUE(reply.empty());
      if (reply.empty())
        *failed = true;
      cond_var->notify_one();
    };
  }

  source_node->SendDirect(destination_node_id, data, false, response_functor);

  std::unique_lock<std::mutex> lock(*response_mutex);
  if (cond_var->wait_for(lock, std::chrono::seconds(15)) != std::cv_status::no_timeout) {
    EXPECT_TRUE(false) << "Didn't get reply within allowed time!";
    return testing::AssertionFailure();
  }

  if (*failed)
    return testing::AssertionFailure();
  return testing::AssertionSuccess();
}

uint16_t GenericNetwork::NonClientNodesSize() const {
  uint16_t non_client_size(0);
  for (const auto& node : nodes_) {
    if (!node->IsClient())
      non_client_size++;
  }
  return non_client_size;
}

uint16_t GenericNetwork::NonClientNonSymmetricNatNodesSize() const {
  uint16_t non_client_non_sym_size(0);
  for (const auto& node : nodes_) {
    if (!node->IsClient() && !node->HasSymmetricNat())
      non_client_non_sym_size++;
  }
  return non_client_non_sym_size;
}

void GenericNetwork::AddNodeDetails(NodePtr node) {
  std::string descriptor;
  if (node->has_symmetric_nat_)
    descriptor.append("Symmetric ");
  else
    descriptor.append("Normal ");
  if (node->IsClient())
    descriptor.append("client");
  else
    descriptor.append(("vault"));
  LOG(kVerbose) << "GenericNetwork::AddNodeDetails - starting to add " << descriptor
                << " " << DebugId(node->node_id());
  std::shared_ptr<std::condition_variable> cond_var(new std::condition_variable);
  std::weak_ptr<std::condition_variable> cond_var_weak(cond_var);
  {
    {
      std::lock_guard<std::mutex> fobs_lock(fobs_mutex_);
      public_keys_.insert(std::make_pair(node->node_id(), node->public_key()));
    }
    std::lock_guard<std::mutex> lock(mutex_);
    SetNodeValidationFunctor(node);

    if (node->has_symmetric_nat_) {
      node->set_expected(NetworkStatus(node->IsClient(),
                                       std::min(NonClientNonSymmetricNatNodesSize(),
                                                Parameters::closest_nodes_size)));
    } else {
      node->set_expected(NetworkStatus(node->IsClient(),
                                       std::min(NonClientNodesSize(),
                                                Parameters::closest_nodes_size)));
    }
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
        ASSERT_GE(result, kSuccess);
        if (result == node->expected() && !node->joined()) {
          node->set_joined(true);
          cond_var->notify_one();
        }
      };
  node->Join(bootstrap_endpoints_);

  std::mutex mutex;
  if (!node->joined()) {
    std::unique_lock<std::mutex> lock(mutex);
    uint16_t maximum_wait(20);
    if (node->has_symmetric_nat_)
      maximum_wait = 30;
    auto result = cond_var->wait_for(lock, std::chrono::seconds(maximum_wait));
    EXPECT_EQ(result, std::cv_status::no_timeout) << descriptor << " node failed to join: "
                                                  << DebugId(node->node_id());
    Sleep(std::chrono::milliseconds(1000));
  }
  PrintRoutingTables();
}

std::shared_ptr<GenericNetwork> NodesEnvironment::g_env_ =
  std::shared_ptr<GenericNetwork>(new GenericNetwork());

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
