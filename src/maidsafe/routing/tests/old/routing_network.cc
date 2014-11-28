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

#include "maidsafe/routing/tests/routing_network.h"

#include <future>
#include <set>
#include <string>
#include <vector>
#include <memory>

#include "maidsafe/common/log.h"
#include "maidsafe/common/make_unique.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/passport/passport.h"

#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_impl.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/utils.h"
#include "maidsafe/routing/tests/test_utils.h"

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

GenericNode::GenericNode(bool has_symmetric_nat)
    : functors_(),
      id_(0),
      node_info_plus_(),
      mutex_(),
      client_mode_(true),
      joined_(false),
      expected_(0),
      nat_type_(rudp::NatType::kUnknown),
      has_symmetric_nat_(has_symmetric_nat),
      endpoint_(),
      messages_(),
      routing_(),
      health_(0) {
  node_info_plus_.reset(new NodeInfoAndPrivateKey(MakeNodeInfoAndKeys()));
  routing_.reset(new Routing());  // FIXME Prakash
  node_info_plus_->node_info.id = routing_->kNodeId();
  endpoint_.address(GetLocalIp());
  endpoint_.port(maidsafe::test::GetRandomPort());
  InitialiseFunctors();
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
      health_(0) {
  if (client_mode) {
    auto maid(passport::CreateMaidAndSigner().first);
    node_info_plus_.reset(new NodeInfoAndPrivateKey(MakeNodeInfoAndKeysWithMaid(maid)));
    routing_.reset(new Routing(maid));  // FIXME prakash
  } else {
    auto pmid(passport::CreatePmidAndSigner().first);
    node_info_plus_.reset(new NodeInfoAndPrivateKey(MakeNodeInfoAndKeysWithPmid(pmid)));
    routing_.reset(new Routing(pmid));  // FIXME prakash
  }
  endpoint_.address(GetLocalIp());
  endpoint_.port(maidsafe::test::GetRandomPort());
  InitialiseFunctors();
  routing_->pimpl_->network_->nat_type_ = nat_type_;
  std::lock_guard<std::mutex> lock(mutex_);
  id_ = next_node_id_++;
}

GenericNode::GenericNode(const passport::Pmid& pmid, bool has_symmetric_nat)
    : functors_(),
      id_(0),
      node_info_plus_(std::make_shared<NodeInfoAndPrivateKey>(MakeNodeInfoAndKeysWithFob(pmid))),
      maid_(),
      mutex_(),
      client_mode_(false),
      joined_(false),
      expected_(0),
      nat_type_(rudp::NatType::kUnknown),
      has_symmetric_nat_(has_symmetric_nat),
      endpoint_(),
      messages_(),
      routing_(),
      health_(0) {
  endpoint_.address(GetLocalIp());
  endpoint_.port(maidsafe::test::GetRandomPort());
  InitialiseFunctors();
  routing_.reset(new Routing(pmid));
  std::lock_guard<std::mutex> lock(mutex_);
  id_ = next_node_id_++;
}

GenericNode::GenericNode(const passport::Maid& maid, bool has_symmetric_nat)
    : functors_(),
      id_(0),
      node_info_plus_(std::make_shared<NodeInfoAndPrivateKey>(MakeNodeInfoAndKeysWithFob(maid))),
      maid_(std::make_shared<passport::Maid>(maid)),
      mutex_(),
      client_mode_(true),
      joined_(false),
      expected_(0),
      nat_type_(rudp::NatType::kUnknown),
      has_symmetric_nat_(has_symmetric_nat),
      endpoint_(),
      messages_(),
      routing_(),
      health_(0) {
  endpoint_.address(GetLocalIp());
  endpoint_.port(maidsafe::test::GetRandomPort());
  InitialiseFunctors();
  routing_.reset(new Routing(maid));
  std::lock_guard<std::mutex> lock(mutex_);
  id_ = next_node_id_++;
}

GenericNode::~GenericNode() {}

void GenericNode::InitialiseFunctors() {
  functors_.message_and_caching.message_received = [this](const std::string& message,
                                                          ReplyFunctor reply_functor) {
    std::lock_guard<std::mutex> guard(mutex_);
    messages_.push_back(message);
    reply_functor(node_id().string() + ">::< response to >:<" + message);
  };
  functors_.network_status = [&](const int& health) { SetHealth(health); };  // NOLINT
  functors_.close_nodes_change = [&](std::shared_ptr<routing::CloseNodesChange> /*close_change*/) {
    //     close_nodes_change_functor(node_info_plus_->node_info.id, close_nodes_change);
  };
}

void GenericNode::SetStoreInCacheFunctor(StoreCacheDataFunctor cache_store_functor) {
  functors_.message_and_caching.store_cache_data = cache_store_functor;
  routing_->pimpl_->message_handler_->cache_manager_->InitialiseFunctors(
      functors_.message_and_caching);
}

void GenericNode::SetGetFromCacheFunctor(HaveCacheDataFunctor get_from_functor) {
  functors_.message_and_caching.have_cache_data = get_from_functor;
  routing_->pimpl_->message_handler_->cache_manager_->InitialiseFunctors(
      functors_.message_and_caching);
}

int GenericNode::GetStatus() const { return routing_->network_status(); }

Endpoint GenericNode::endpoint() const { return endpoint_; }

NodeId GenericNode::connection_id() const { return node_info_plus_->node_info.connection_id; }

NodeId GenericNode::node_id() const { return node_info_plus_->node_info.id; }

size_t GenericNode::id() const { return id_; }

bool GenericNode::IsClient() const { return client_mode_; }

bool GenericNode::HasSymmetricNat() const { return has_symmetric_nat_; }

std::vector<NodeInfo> GenericNode::RoutingTable() const {
  return routing_->pimpl_->routing_table_->nodes_;
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

void GenericNode::SetCloseNodesChangeFunctor(CloseNodesChangeFunctor close_nodes_change_functor) {
  functors_.close_nodes_change = close_nodes_change_functor;
}

void GenericNode::SendDirect(const NodeId& destination_id, const std::string& data, bool cacheable,
                             ResponseFunctor response_functor) {
  routing_->SendDirect(destination_id, data, cacheable, response_functor);
}

void GenericNode::SendGroup(const NodeId& destination_id, const std::string& data, bool cacheable,
                            ResponseFunctor response_functor) {
  routing_->SendGroup(destination_id, data, cacheable, response_functor);
}

void GenericNode::SendMessage(const NodeId& destination_id, protobuf::Message& proto_message) {
  routing_->pimpl_->SendMessage(destination_id, proto_message);
}

void GenericNode::AddTask(const ResponseFunctor& response_functor, int expected_response_count,
                          TaskId task_id) {
  routing_->pimpl_->timer_.AddTask(Parameters::default_response_timeout, response_functor,
                                   expected_response_count, task_id);
}

void GenericNode::RudpSend(const NodeId& peer_node_id, const protobuf::Message& message,
                           rudp::MessageSentFunctor message_sent_functor) {
  routing_->pimpl_->network_->RudpSend(peer_node_id, message, message_sent_functor);
}

void GenericNode::SendToClosestNode(const protobuf::Message& message) {
  routing_->pimpl_->network_->SendToClosestNode(message);
}

bool GenericNode::RoutingTableHasNode(const NodeId& node_id) {
  auto node(std::find_if(routing_->pimpl_->routing_table_->nodes_.begin(),
                         routing_->pimpl_->routing_table_->nodes_.end(),
                         [node_id](const NodeInfo& node_info) { return node_id == node_info.id; }));
  bool result(node != routing_->pimpl_->routing_table_->nodes_.end());
  return result;
}

bool GenericNode::ClientRoutingTableHasNode(const NodeId& node_id) {
  return std::find_if(routing_->pimpl_->client_routing_table_.nodes_.begin(),
                      routing_->pimpl_->client_routing_table_.nodes_.end(),
                      [&node_id](const NodeInfo& node_info) {
           return (node_id == node_info.id);
         }) != routing_->pimpl_->client_routing_table_.nodes_.end();
}

NodeInfo GenericNode::GetNthClosestNode(const NodeId& target_id, unsigned int node_number) {
  return routing_->pimpl_->routing_table_->GetNthClosestNode(target_id, node_number);
}

testing::AssertionResult GenericNode::DropNode(const NodeId& node_id) {
  auto iter =
      std::find_if(routing_->pimpl_->routing_table_->nodes_.begin(),
                   routing_->pimpl_->routing_table_->nodes_.end(),
                   [&node_id](const NodeInfo& node_info) { return (node_id == node_info.id); });
  if (iter != routing_->pimpl_->routing_table_->nodes_.end()) {
    routing_->pimpl_->routing_table_->DropNode(iter->connection_id, false);
  } else {
    testing::AssertionFailure() << DebugId(routing_->pimpl_->routing_table_->kNodeId_)
                                << " does not have " << DebugId(node_id) << " in routing table of ";
  }
  return testing::AssertionSuccess();
}

NodeInfo GenericNode::node_info() const { return node_info_plus_->node_info; }

int GenericNode::ZeroStateJoin(const Endpoint& peer_endpoint, const NodeInfo& peer_node_info) {
  return routing_->ZeroStateJoin(functors_, endpoint(), peer_endpoint, peer_node_info);
}

void GenericNode::Join() { routing_->Join(functors_); }

void GenericNode::set_joined(const bool node_joined) { joined_ = node_joined; }

bool GenericNode::joined() const { return joined_; }

unsigned int GenericNode::expected() { return expected_; }

void GenericNode::set_expected(int expected) { expected_ = expected; }

void GenericNode::PrintRoutingTable() {
  routing_->pimpl_->routing_table_->Print();
}

std::vector<NodeId> GenericNode::ReturnRoutingTable() {
  std::vector<NodeId> routing_nodes;
  std::lock_guard<std::mutex> lock(routing_->pimpl_->routing_table_->mutex_);
  for (const auto& node_info : routing_->pimpl_->routing_table_->nodes_)
    routing_nodes.push_back(node_info.id);
  return routing_nodes;
}

std::string GenericNode::SerializeRoutingTable() {
  std::vector<NodeId> node_list;
  for (const auto& node_info : routing_->pimpl_->routing_table_->nodes_)
    node_list.push_back(node_info.id);
  return SerializeNodeIdList(node_list);
}

size_t GenericNode::MessagesSize() const { return messages_.size(); }

void GenericNode::ClearMessages() {
  std::lock_guard<std::mutex> lock(mutex_);
  messages_.clear();
}

passport::Maid GenericNode::GetMaid() {
  assert(maid_);
  return *maid_;
}

asymm::PublicKey GenericNode::public_key() {
  std::lock_guard<std::mutex> lock(mutex_);
  return node_info_plus_->node_info.public_key;
}

int GenericNode::Health() {
  return health_;
}

void GenericNode::SetHealth(int health) {
  health_ = health;
}

void GenericNode::PostTaskToAsioService(std::function<void()> functor) {
  std::lock_guard<std::mutex> lock(routing_->pimpl_->running_mutex_);
  if (routing_->pimpl_->running_)
    routing_->pimpl_->asio_service_.service().post(functor);
}

rudp::NatType GenericNode::nat_type() { return routing_->pimpl_->network_->nat_type(); }

GenericNetwork::GenericNetwork()
    : mutex_(),
      fobs_mutex_(),
      public_keys_(),
      client_index_(0),
      nat_info_available_(true),
      bootstrap_file_(),
      nodes_() {}

GenericNetwork::~GenericNetwork() {
  nat_info_available_ = false;
  for (auto& node : nodes_) {
    node->functors_.request_public_key =
        RequestPublicKeyFunctor([](NodeId, GivePublicKeyFunctor) {});
  }

  while (!nodes_.empty())
    RemoveNode(nodes_.at(0)->node_id());
}

void GenericNetwork::SetUp() {
  NodePtr node1(new GenericNode(passport::CreatePmidAndSigner().first)),
      node2(new GenericNode(passport::CreatePmidAndSigner().first));
  nodes_.push_back(node1);
  nodes_.push_back(node2);
  bootstrap_file_.reset();
  bootstrap_file_ =
      maidsafe::make_unique<ScopedBootstrapFile>(BootstrapContacts{ node1->endpoint(),
                                                                    node2->endpoint() });

  client_index_ = 2;
  public_keys_.insert(std::make_pair(node1->node_id(), node1->public_key()));
  public_keys_.insert(std::make_pair(node2->node_id(), node2->public_key()));
  // fobs_.push_back(node2->fob());
  SetNodeValidationFunctor(node1);
  SetNodeValidationFunctor(node2);
  auto f1 = std::async(std::launch::async, [ =, &node2 ]()->int {
    return node1->ZeroStateJoin(node2->endpoint(), node2->node_info());
  });
  auto f2 = std::async(std::launch::async, [ =, &node1 ]()->int {
    return node2->ZeroStateJoin(node1->endpoint(), node1->node_info());
  });
  EXPECT_EQ(kSuccess, f2.get());
  EXPECT_EQ(kSuccess, f1.get());
}

void GenericNetwork::TearDown() {
  std::vector<NodeId> nodes_id;
  for (auto index(this->ClientIndex()); index < this->nodes_.size(); ++index)
    nodes_id.push_back(this->nodes_.at(index)->node_id());
  for (const auto& node_id : nodes_id)
    this->RemoveNode(node_id);
  nodes_id.clear();

  for (const auto& node : this->nodes_)
    if (node->HasSymmetricNat())
      nodes_id.push_back(node->node_id());
  for (const auto& node_id : nodes_id)
    this->RemoveNode(node_id);
  nodes_id.clear();

  for (const auto& node : this->nodes_)
    nodes_id.insert(nodes_id.begin(), node->node_id());  // reverse order - do bootstraps last
  for (const auto& node_id : nodes_id)
    this->RemoveNode(node_id);

  std::lock_guard<std::mutex> lock(mutex_);
  GenericNode::next_node_id_ = 1;
}

void GenericNetwork::SetUpNetwork(size_t total_number_vaults, size_t total_number_clients) {
  SetUpNetwork(total_number_vaults, total_number_clients, 0, 0);
}

void GenericNetwork::SetUpNetwork(size_t total_number_vaults, size_t total_number_clients,
                                  size_t num_symmetric_nat_vaults,
                                  size_t num_symmetric_nat_clients) {
  assert(total_number_vaults >= num_symmetric_nat_vaults + 2);
  assert(total_number_clients >= num_symmetric_nat_clients);

  size_t num_nonsym_nat_vaults(total_number_vaults - num_symmetric_nat_vaults);
  size_t num_nonsym_nat_clients(total_number_clients - num_symmetric_nat_clients);

  for (size_t index(2); index < num_nonsym_nat_vaults; ++index) {
    NodePtr node(new GenericNode(passport::CreatePmidAndSigner().first));
    AddNodeDetails(node);
  }

  for (size_t index(0); index < num_symmetric_nat_vaults; ++index) {
    NodePtr node(new GenericNode(passport::CreatePmidAndSigner().first, true));
    AddNodeDetails(node);
  }

  for (size_t index(0); index < num_nonsym_nat_clients; ++index) {
    NodePtr node(new GenericNode(passport::CreateMaidAndSigner().first));
    AddNodeDetails(node);
  }

  for (size_t index(0); index < num_symmetric_nat_clients; ++index) {
    NodePtr node(new GenericNode(passport::CreateMaidAndSigner().first, true));
    AddNodeDetails(node);
  }

  Sleep(std::chrono::seconds(1));
  PrintRoutingTables();
  //    EXPECT_TRUE(ValidateRoutingTables());
}

void GenericNetwork::AddNode(bool client_mode, CloseNodesChangeFunctor close_nodes_change_functor) {
  NodePtr node;
  if (client_mode) {
    auto maid(passport::CreateMaidAndSigner().first);
    node.reset(new GenericNode(maid));
  } else {
    auto pmid(passport::CreatePmidAndSigner().first);
    node.reset(new GenericNode(pmid, false));
  }
  node->SetCloseNodesChangeFunctor(close_nodes_change_functor);
  AddNodeDetails(node);
}

void GenericNetwork::AddNode(const passport::Maid& maid, bool has_symmetric_nat) {
  NodePtr node;
  node.reset(new GenericNode(maid, has_symmetric_nat));
  AddNodeDetails(node);
}
void GenericNetwork::AddNode(const passport::Pmid& pmid, bool has_symmetric_nat) {
  NodePtr node;
  node.reset(new GenericNode(pmid, has_symmetric_nat));
  AddNodeDetails(node);
}

void GenericNetwork::AddMutatingClient(bool has_symmetric_nat) {
  NodePtr node;
  node.reset(new GenericNode(has_symmetric_nat));
  AddNodeDetails(node);
}

void GenericNetwork::AddClient(bool has_symmetric_nat) {
  AddNode(passport::CreateMaidAndSigner().first, has_symmetric_nat);
}

void GenericNetwork::AddVault(bool has_symmetric_nat) {
  AddNode(passport::CreatePmidAndSigner().first, has_symmetric_nat);
}

void GenericNetwork::AddNode(const passport::Maid& maid,
                             CloseNodesChangeFunctor close_nodes_change_functor) {
  NodePtr node;
  node.reset(new GenericNode(maid));
  node->SetCloseNodesChangeFunctor(close_nodes_change_functor);
  AddNodeDetails(node);
}

void GenericNetwork::AddNode(const passport::Pmid& pmid,
                             CloseNodesChangeFunctor close_nodes_change_functor) {
  NodePtr node;
  node.reset(new GenericNode(pmid, false));
  node->SetCloseNodesChangeFunctor(close_nodes_change_functor);
  AddNodeDetails(node);
}

void GenericNetwork::AddNode(bool has_symmetric_nat) {
  NodePtr node(new GenericNode(has_symmetric_nat));
  AddNodeDetails(node);
}

void GenericNetwork::AddNode(bool client_mode, const rudp::NatType& nat_type) {
  NodeInfoAndPrivateKey node_info(MakeNodeInfoAndKeys());
  NodePtr node(new GenericNode(client_mode, nat_type));
  AddNodeDetails(node);
}

void GenericNetwork::AddNode(bool client_mode, bool has_symmetric_nat) {
  NodePtr node;
  if (client_mode)
    node.reset(new GenericNode(passport::CreateMaidAndSigner().first, has_symmetric_nat));
  else
    node.reset(new GenericNode(passport::CreatePmidAndSigner().first, has_symmetric_nat));
  AddNodeDetails(node);
}

bool GenericNetwork::RemoveNode(const NodeId& node_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto iter = std::find_if(nodes_.begin(), nodes_.end(),
                           [&node_id](const NodePtr node) { return node_id == node->node_id(); });
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
  unsigned int max(10);
  unsigned int i(0);
  while (i < max) {
    all_joined = true;
    for (unsigned int j(2); j < nodes_.size(); ++j) {
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


bool GenericNetwork::WaitForNodesToJoin(size_t num_total_nodes) {
  // TODO(Alison) - tailor max. duration to match number of nodes joining?
  bool all_joined = true;
  size_t expected_health(num_total_nodes < Parameters::max_client_routing_table_size
                             ? (num_total_nodes * 100) /
                                   static_cast<size_t>(Parameters::max_client_routing_table_size)
                             : 100);
  unsigned int max(10), i(0);
  while (i < max) {
    all_joined = true;
    for (unsigned int j(2); j < nodes_.size(); ++j) {
      if (!nodes_.at(j)->joined()) {
        all_joined = false;
        break;
      }
      if (static_cast<size_t>(nodes_.at(j)->Health()) < expected_health) {
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
  if (iter != public_keys_.end()) {
    give_public_key((*iter).second);
  }
}

void GenericNetwork::SetNodeValidationFunctor(NodePtr node) {
  NodeId own_node_id(node->node_id());
  if (node->HasSymmetricNat()) {
    node->functors_.request_public_key = [this, own_node_id](const NodeId& node_id,
                                                             GivePublicKeyFunctor give_public_key) {
      assert(node_id != own_node_id && "(1) Should not get public key request from own node id!");
      if (!NodeHasSymmetricNat(node_id))
        this->Validate(node_id, give_public_key);
    };
  } else {
    node->functors_.request_public_key = [this, own_node_id](const NodeId& node_id,
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
  std::partial_sort(all_ids.begin(), all_ids.begin() + Parameters::group_size + 1, all_ids.end(),
                    [&](const NodeId& lhs,
                        const NodeId& rhs) { return NodeId::CloserToTarget(lhs, rhs, node_id); });
  return std::vector<NodeId>(all_ids.begin() + static_cast<unsigned int>(all_ids[0] == node_id),
                             all_ids.begin() + Parameters::group_size +
                                 static_cast<unsigned int>(all_ids[0] == node_id));
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
    std::sort(node_ids.begin(), node_ids.end(), [=](const NodeId & lhs, const NodeId & rhs)->bool {
      return NodeId::CloserToTarget(lhs, rhs, node->node_id());
    });
    auto routing_table(node->RoutingTable());
    //      EXPECT_FALSE(routing_table.size() < Parameters::closest_nodes_size);
    std::sort(routing_table.begin(), routing_table.end(),
              [&, this ](const NodeInfo & lhs, const NodeInfo & rhs)->bool {
      return NodeId::CloserToTarget(lhs.id, rhs.id, node->node_id());
    });
    unsigned int size(
        std::min(static_cast<unsigned int>(routing_table.size()), Parameters::closest_nodes_size));
    for (auto iter(routing_table.begin()); iter < routing_table.begin() + size - 1; ++iter) {
      size_t distance(
          std::distance(node_ids.begin(), std::find(node_ids.begin(), node_ids.end(), (*iter).id)));
      if (distance > size)
        return false;
    }
  }
  return true;
}

unsigned int GenericNetwork::RandomNodeIndex() const {
  assert(!nodes_.empty());
  return static_cast<unsigned int>(RandomUint32() % nodes_.size());
}

unsigned int GenericNetwork::RandomClientIndex() const {
  assert(nodes_.size() > client_index_);
  unsigned int client_count(static_cast<unsigned int>(nodes_.size()) - client_index_);
  return static_cast<unsigned int>(RandomUint32() % client_count) + client_index_;
}

unsigned int GenericNetwork::RandomVaultIndex() const {
  assert(nodes_.size() > 2);
  assert(client_index_ > 2);
  return static_cast<unsigned int>(RandomUint32() % client_index_);
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

std::vector<NodeId> GenericNetwork::GetAllNodeIds() const {
  std::vector<NodeId> node_ids;
  for (const auto& node : nodes_)
    node_ids.push_back(node->node_id());
  return node_ids;
}

std::vector<NodeId> GenericNetwork::GetGroupForId(const NodeId& node_id) const {
  std::vector<NodeId> group_ids;
  for (const auto& node : nodes_) {
    if (!node->IsClient() && (node->node_id() != node_id))
      group_ids.push_back(node->node_id());
  }
  std::partial_sort(group_ids.begin(), group_ids.begin() + Parameters::group_size, group_ids.end(),
                    [&](const NodeId& lhs,
                        const NodeId& rhs) { return NodeId::CloserToTarget(lhs, rhs, node_id); });
  return std::vector<NodeId>(group_ids.begin(), group_ids.begin() + Parameters::group_size);
}

std::vector<NodeInfo> GenericNetwork::GetClosestNodes(const NodeId& target_id, uint32_t quantity,
                                                      bool vault_only) const {
  std::vector<NodeInfo> closet_nodes;
  for (const auto& node : nodes_) {
    if (vault_only && node->IsClient())
      continue;
    closet_nodes.push_back(node->node_info_plus_->node_info);
  }
  uint32_t size = std::min(quantity + 1, static_cast<uint32_t>(nodes_.size()));
  std::lock_guard<std::mutex> lock(mutex_);
  std::partial_sort(closet_nodes.begin(), closet_nodes.begin() + size, closet_nodes.end(),
                    [&](const NodeInfo& lhs, const NodeInfo& rhs) {
    return NodeId::CloserToTarget(lhs.id, rhs.id, target_id);
  });
  return std::vector<NodeInfo>(closet_nodes.begin() + 1, closet_nodes.begin() + size);
}

std::vector<NodeInfo> GenericNetwork::GetClosestVaults(const NodeId& target_id,
                                                       uint32_t quantity) const {
  std::vector<NodeInfo> closest_nodes;
  for (const auto& node : nodes_) {
    if (!node->IsClient())
      closest_nodes.push_back(node->node_info());
  }

  uint32_t sort_size(std::min(quantity, static_cast<uint32_t>(closest_nodes.size())));

  std::lock_guard<std::mutex> lock(mutex_);
  std::partial_sort(closest_nodes.begin(), closest_nodes.begin() + sort_size, closest_nodes.end(),
                    [&](const NodeInfo& lhs, const NodeInfo& rhs) {
    return NodeId::CloserToTarget(lhs.id, rhs.id, target_id);
  });

  return std::vector<NodeInfo>(closest_nodes.begin(), closest_nodes.begin() + sort_size);
}

void GenericNetwork::ValidateExpectedNodeType(const NodeId& node_id,
                                              const ExpectedNodeType& expected_node_type) const {
  auto itr = std::find_if(this->nodes_.begin(), this->nodes_.end(),
                          [&](const NodePtr node) { return (node->node_id() == node_id); });
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
  while (nodes_.size() > kNetworkSize) {
    RemoveNode(nodes_.back()->node_id());
    Sleep(std::chrono::seconds(1));
  }
  assert(nodes_.size() == kNetworkSize && "Fails to remove added nodes");
  return true;
}

bool GenericNetwork::WaitForHealthToStabilise() const {
  int i(0);
  bool healthy(false);
  int vault_health(100);
  int client_health(100);
  int vault_symmetric_health(100);
  int client_symmetric_health(100);
  auto server_size(client_index_);
  if (server_size <= Parameters::max_routing_table_size)
    vault_health = (server_size - 1) * 100 / Parameters::max_routing_table_size;
  if (server_size <= Parameters::max_routing_table_size_for_client)
    client_health = (server_size - 1) * 100 / Parameters::max_routing_table_size_for_client;
  auto number_nonsymmetric_vaults(NonClientNonSymmetricNatNodesSize());
  if (number_nonsymmetric_vaults <= Parameters::max_routing_table_size)
    vault_symmetric_health = number_nonsymmetric_vaults * 100 / Parameters::max_routing_table_size;
  if (number_nonsymmetric_vaults <= Parameters::max_routing_table_size_for_client)
    client_symmetric_health =
        number_nonsymmetric_vaults * 100 / Parameters::max_routing_table_size_for_client;

  while (i != 10 && !healthy) {
    ++i;
    healthy = true;
    int expected_health;
    std::string error_message;
    for (const auto& node : nodes_) {
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
        LOG(kError) << "Bad " << error_message << " Expected: " << expected_health
                    << " Got: " << node_health;
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

testing::AssertionResult GenericNetwork::SendDirect(size_t repeats, size_t message_size) {
  assert(repeats > 0);
  size_t total_num_nodes(this->nodes_.size());

  auto timeout(Parameters::default_response_timeout);
  Parameters::default_response_timeout *= repeats * 3;

  std::shared_ptr<std::mutex> response_mutex(std::make_shared<std::mutex>());
  std::shared_ptr<std::condition_variable> cond_var(std::make_shared<std::condition_variable>());
  std::shared_ptr<unsigned int> reply_count(std::make_shared<unsigned int>(0)),
      expected_count(std::make_shared<unsigned int>(
          static_cast<unsigned int>(repeats * total_num_nodes * total_num_nodes)));
  std::shared_ptr<bool> failed(std::make_shared<bool>(false));

  for (size_t repeat = 0; repeat < repeats; ++repeat) {
    for (const auto& dest : this->nodes_) {
      for (const auto& src : this->nodes_) {
        std::string data(RandomAlphaNumericString(message_size));
        assert(!data.empty() && "Send Data Empty !");
        ResponseFunctor response_functor;
        if (dest->IsClient() &&
            (!src->IsClient() || (src->IsClient() && (dest->node_id() == src->node_id())))) {
          response_functor = [response_mutex, cond_var, reply_count, expected_count, failed](
              std::string /*reply*/) {
            std::lock_guard<std::mutex> lock(*response_mutex);
            ++(*reply_count);
//            EXPECT_TRUE(reply.empty());
//            if (!reply.empty()) {
//              *failed = true;
//              if (*reply_count == *expected_count)
//                cond_var->notify_one();
//              return;
//            }
            if (*reply_count == *expected_count)
              cond_var->notify_one();
          };
        } else if (dest->IsClient() && src->IsClient() && (dest->node_id() != src->node_id())) {
          response_functor = [response_mutex, cond_var, reply_count, expected_count, failed](
              std::string reply) {
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
            }
            catch (const std::exception& ex) {
              ADD_FAILURE() << "Got message with invalid replier ID. Exception: " << ex.what();
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
  if (!cond_var->wait_for(
           lock, std::chrono::seconds(15 * (nodes_.size()) * (nodes_.size() - 1)),
           [reply_count, expected_count]() { return *reply_count == *expected_count; })) {
    ADD_FAILURE() << "Didn't get reply within allowed time!";
    Parameters::default_response_timeout = timeout;
    return testing::AssertionFailure();
  }
  Parameters::default_response_timeout = timeout;

  if (*failed)
    return testing::AssertionFailure();
  return testing::AssertionSuccess();
}

struct SendGroupMonitor {
  explicit SendGroupMonitor(std::vector<NodeId> expected_ids)
      : response_count(0), expected_ids(std::move(expected_ids)) {}

  unsigned int response_count;
  std::vector<NodeId> expected_ids;
};

testing::AssertionResult GenericNetwork::SendGroup(const NodeId& target_id, size_t repeats,
                                                   unsigned int source_index, size_t message_size) {
  assert(repeats > 0);
  std::shared_ptr<std::mutex> response_mutex(std::make_shared<std::mutex>());
  std::shared_ptr<std::condition_variable> cond_var(std::make_shared<std::condition_variable>());
  std::string data(RandomAlphaNumericString(message_size));
  std::shared_ptr<unsigned int> reply_count(std::make_shared<unsigned int>(0)),
      expected_count(std::make_shared<unsigned int>(
          static_cast<unsigned int>(Parameters::group_size * repeats)));
  std::shared_ptr<bool> failed(std::make_shared<bool>(false));

  std::vector<NodeId> target_group(this->GetGroupForId(target_id));
  for (unsigned int repeat(0); repeat < repeats; ++repeat) {
    std::shared_ptr<SendGroupMonitor> monitor(std::make_shared<SendGroupMonitor>(target_group));
    monitor->response_count = 0;
    ResponseFunctor response_functor =
        [response_mutex, cond_var, failed, reply_count, expected_count, target_id, monitor, repeat](
            std::string reply) {
      std::lock_guard<std::mutex> lock(*response_mutex);
      ++(*reply_count);
      monitor->response_count += 1;
      if (monitor->response_count > Parameters::group_size) {
        ADD_FAILURE() << "Received too many replies: " << monitor->response_count;
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
        bool valid_replier(std::find(monitor->expected_ids.begin(), monitor->expected_ids.end(),
                                     replier) != monitor->expected_ids.end());
        monitor->expected_ids.erase(
            std::remove_if(monitor->expected_ids.begin(), monitor->expected_ids.end(),
                           [&](const NodeId& node_id) { return node_id == replier; }),
            monitor->expected_ids.end());
        if (!valid_replier) {
          ADD_FAILURE() << "Got unexpected reply from " << replier << "\tfor target: " << target_id;
          *failed = true;
        }
      }
      catch (const std::exception& /*ex*/) {
        ADD_FAILURE() << "Reply contained invalid node ID.";
        *failed = true;
      }
      if (*reply_count == *expected_count)
        cond_var->notify_one();
    };

    this->nodes_.at(source_index)->SendGroup(target_id, data, false, response_functor);
  }

  std::unique_lock<std::mutex> lock(*response_mutex);
  if (!cond_var->wait_for(
           lock, Parameters::default_response_timeout,
           [reply_count, expected_count]() { return *reply_count == *expected_count; })) {
    ADD_FAILURE() << "Didn't get replies within allowed time!";
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
  std::shared_ptr<unsigned int> reply_count(std::make_shared<unsigned int>(0)),
      expected_count(
          std::make_shared<unsigned int>(static_cast<unsigned int>(this->nodes_.size())));
  std::shared_ptr<bool> failed(std::make_shared<bool>(false));

  size_t message_index(0);
  for (const auto& src : this->nodes_) {
    if (src->IsClient() && destination_node_type != ExpectedNodeType::kExpectVault)
      --*expected_count;
    ResponseFunctor response_functor;
    std::string data(std::to_string(message_index) + "<:>" +
                     RandomAlphaNumericString((RandomUint32() % 255 + 1) * 2 ^ 10));
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
          std::string data_index(
              reply.substr(reply.find(">:<") + 3, reply.find("<:>") - 3 - reply.find(">:<")));
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
        }
        catch (const std::exception& ex) {
          ADD_FAILURE() << "Got message with invalid replier ID. Exception: " << ex.what();
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
      for (auto& elem : nodes_) {
        if (elem->node_id() == destination_node_id) {
          dest = elem;
          break;
        }
      }
      if ((dest != nullptr) &&
          (dest->IsClient() &&
           (!src->IsClient() || (src->IsClient() && (dest->node_id() == src->node_id()))))) {
        response_functor = [response_mutex, cond_var, reply_count, expected_count, failed](
            std::string /*reply*/) {
          std::lock_guard<std::mutex> lock(*response_mutex);
          ++(*reply_count);
//          EXPECT_FALSE(reply.empty());
//          if (reply.empty()) {
//            *failed = true;
//            if (*reply_count == *expected_count)
//              cond_var->notify_one();
//            return;
//         }
          if (*reply_count == *expected_count)
            cond_var->notify_one();
        };
      } else if (dest != nullptr) {
        response_functor = [response_mutex, cond_var, reply_count, expected_count, failed](
            std::string reply) {
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
  if (!cond_var->wait_for(lock, std::chrono::seconds(25), [reply_count, expected_count]() {
         return *reply_count == *expected_count;
       })) {
    LOG(kError) << "Didn't get reply within allowed time!";
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

  std::string data(RandomAlphaNumericString(512 * 2 ^ 10));
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
      }
      catch (const std::exception* /*ex*/) {
        ADD_FAILURE() << "Reply contained invalid node ID!";
        *failed = true;
      }
      cond_var->notify_one();
    };
  } else {
    response_functor = [response_mutex, cond_var, failed, source_node, destination_node_id](
        std::string reply) {
      std::lock_guard<std::mutex> lock(*response_mutex);
      if (source_node->IsClient() && (source_node->node_id() != destination_node_id)) {
        EXPECT_TRUE(reply.empty());
        *failed = true;
      } else {
        if (reply.empty())
          *failed = true;
      }
      cond_var->notify_one();
    };
  }

  source_node->SendDirect(destination_node_id, data, false, response_functor);

  std::unique_lock<std::mutex> lock(*response_mutex);
  if (cond_var->wait_for(lock, std::chrono::seconds(25)) != std::cv_status::no_timeout) {
    ADD_FAILURE() << "Didn't get reply within allowed time!";
    return testing::AssertionFailure();
  }

  if (*failed)
    return testing::AssertionFailure();
  return testing::AssertionSuccess();
}

unsigned int GenericNetwork::NonClientNodesSize() const {
  unsigned int non_client_size(0);
  for (const auto& node : nodes_) {
    if (!node->IsClient())
      non_client_size++;
  }
  return non_client_size;
}

void GenericNetwork::AddPublicKey(const NodeId& node_id, const asymm::PublicKey& public_key) {
  public_keys_.insert(std::make_pair(node_id, public_key));
}

unsigned int GenericNetwork::NonClientNonSymmetricNatNodesSize() const {
  unsigned int non_client_non_sym_size(0);
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
      node->set_expected(
          NetworkStatus(node->IsClient(), std::min(NonClientNonSymmetricNatNodesSize(),
                                                   Parameters::max_routing_table_size_for_client)));
    } else {
      node->set_expected(
          NetworkStatus(node->IsClient(), std::min(NonClientNodesSize(),
                                                   Parameters::max_routing_table_size_for_client)));
    }
    if (node->IsClient()) {
      nodes_.push_back(node);
    } else {
      nodes_.insert(nodes_.begin() + client_index_, node);
      ++client_index_;
    }
  }
  std::weak_ptr<GenericNode> weak_node(node);
  node->functors_.network_status = [cond_var_weak, weak_node](const int& result) {
    std::shared_ptr<std::condition_variable> cond_var(cond_var_weak.lock());
    NodePtr node(weak_node.lock());
    if (node)
      node->SetHealth(result);

    if (!cond_var || !node)
      return;

    ASSERT_GE(result, kSuccess);
    if (result == static_cast<int>(node->expected()) && !node->joined()) {
      node->set_joined(true);
      cond_var->notify_one();
    }
  };
  node->Join();

  std::mutex mutex;
  if (!node->joined()) {
    std::unique_lock<std::mutex> lock(mutex);
    unsigned int maximum_wait(20);
    if (node->has_symmetric_nat_)
      maximum_wait = 30;
    auto result = cond_var->wait_for(lock, std::chrono::seconds(maximum_wait));
    EXPECT_EQ(result, std::cv_status::no_timeout) << descriptor << " node failed to join: "
                                                  << node->node_id();
    Sleep(std::chrono::milliseconds(1000));
  }
  PrintRoutingTables();
}

std::shared_ptr<GenericNetwork> NodesEnvironment::g_env_ = std::make_shared<GenericNetwork>();

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
