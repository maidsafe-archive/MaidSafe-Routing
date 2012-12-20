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

#ifndef MAIDSAFE_ROUTING_TESTS_ROUTING_NETWORK_H_
#define MAIDSAFE_ROUTING_TESTS_ROUTING_NETWORK_H_

#include <chrono>
#include <future>
#include <map>
#include <string>
#include <vector>
#include <algorithm>

#include "boost/asio/ip/udp.hpp"
#include "boost/thread/future.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/rudp/nat_type.h"

#include "maidsafe/passport/passport.h"
#include "maidsafe/passport/types.h"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_api.h"


namespace args = std::placeholders;

namespace maidsafe {

namespace routing {

class Routing;
namespace protobuf { class Message; }

namespace test {

bool IsPortAvailable(boost::asio::ip::udp::endpoint endpoint);

struct NodeInfoAndPrivateKey;

const uint32_t kClientSize(5);
const uint32_t kServerSize(20);
const uint32_t kNetworkSize = kClientSize + kServerSize;

class GenericNetwork;
class NodesEnvironment;

class GenericNode {
 public:
  explicit GenericNode(bool client_mode = false);
  GenericNode(bool client_mode, const rudp::NatType& nat_type);
  GenericNode(bool client_mode, const NodeInfoAndPrivateKey& node_info);
  virtual ~GenericNode();
  int GetStatus() const;
  NodeId node_id() const;
  size_t id() const;
  NodeId connection_id() const;
  boost::asio::ip::udp::endpoint endpoint() const;
  std::shared_ptr<Routing> routing() const;
  NodeInfo node_info() const;
  void set_joined(const bool node_joined);
  bool joined() const;
  bool IsClient() const;
  bool anonymous() { return anonymous_; }
  // void set_client_mode(const bool& client_mode);
  int expected();
  void set_expected(const int& expected);
  int ZeroStateJoin(const boost::asio::ip::udp::endpoint& peer_endpoint,
                    const NodeInfo& peer_node_info);
  void Join(const std::vector<boost::asio::ip::udp::endpoint>& peer_endpoints =
                std::vector<boost::asio::ip::udp::endpoint>());
  void Send(const NodeId& destination_id,
            const NodeId& group_claim,
            const std::string& data,
            const ResponseFunctor& response_functor,
            const boost::posix_time::time_duration& timeout,
            const DestinationType& destination_type,
            const bool& cache);
  void SendToClosestNode(const protobuf::Message& message);
  void RudpSend(const NodeId& peer_endpoint,
                const protobuf::Message& message,
                rudp::MessageSentFunctor message_sent_functor);
  void PrintRoutingTable();
  void PrintGroupMatrix();
  bool RoutingTableHasNode(const NodeId& node_id);
  bool NonRoutingTableHasNode(const NodeId& node_id);
  NodeInfo GetRemovableNode();
  NodeInfo GetNthClosestNode(const NodeId& target_id, uint16_t node_number);
  testing::AssertionResult DropNode(const NodeId& node_id);
  std::vector<NodeInfo> RoutingTable() const;
  NodeId GetRandomExistingNode() const;
  void AddNodeToRandomNodeHelper(const NodeId& node_id);
  void RemoveNodeFromRandomNodeHelper(const NodeId& node_id);
  bool NodeSubscriedForGroupUpdate(const NodeId& node_id);

  void PostTaskToAsioService(std::function<void()> functor);
  rudp::NatType nat_type();
  std::string SerializeRoutingTable();

  static size_t next_node_id_;
  size_t MessagesSize() const;
  void ClearMessages();
  asymm::PublicKey public_key();
  int Health();
  void SetHealth(const int& health);
  friend class GenericNetwork;
  Functors functors_;

 protected:
  size_t id_;
  std::shared_ptr<NodeInfoAndPrivateKey> node_info_plus_;
  std::mutex mutex_;
  bool client_mode_;
  bool anonymous_;
  bool joined_;
  int expected_;
  rudp::NatType nat_type_;
  boost::asio::ip::udp::endpoint endpoint_;
  std::vector<std::string> messages_;
  std::shared_ptr<Routing> routing_;

 private:
  std::mutex health_mutex_;
  int health_;
  void InitialiseFunctors();
  void InjectNodeInfoAndPrivateKey();
};

class GenericNetwork {
 public:
  typedef std::shared_ptr<GenericNode> NodePtr;
  GenericNetwork();
  virtual ~GenericNetwork();

  bool ValidateRoutingTables();
  void AddNode(const bool& client_mode, const NodeId& node_id, bool anonymous = false);
  virtual void SetUp();
  virtual void TearDown();
  void SetUpNetwork(const size_t& non_client_size, const size_t& client_size = 0);
  void AddNode(const bool& client_mode, const rudp::NatType& nat_type);
  bool RemoveNode(const NodeId& node_id);
  void Validate(const NodeId& node_id, GivePublicKeyFunctor give_public_key);
  void SetNodeValidationFunctor(NodePtr node);
  std::vector<NodeId> GroupIds(const NodeId& node_id);
  void PrintRoutingTables();
  size_t RandomNodeIndex();
  size_t RandomClientIndex();
  size_t RandomVaultIndex();
  NodePtr RandomClientNode();
  NodePtr RandomVaultNode();
  void RemoveRandomClient();
  void RemoveRandomVault();
  void ClearMessages();
  int NodeIndex(const NodeId& node_id);
  size_t ClientIndex() { return client_index_; }
  std::vector<NodeId> GetGroupForId(const NodeId& node_id);
  std::vector<NodeInfo> GetClosestNodes(const NodeId& target_id, const uint32_t& quantity);
  bool RestoreComposition();
  bool WaitForHealthToStabilise();
  testing::AssertionResult Send(const size_t& messages);
  testing::AssertionResult GroupSend(const NodeId& node_id,
                                     const size_t& messages,
                                     uint16_t source_index = 0);
  testing::AssertionResult Send(const NodeId& node_id);
  testing::AssertionResult Send(std::shared_ptr<GenericNode> source_node,
                                const NodeId& node_id,
                                bool no_response_expected = false);

  friend class NodesEnvironment;


 private:
  uint16_t NonClientNodesSize() const;
  void AddNodeDetails(NodePtr node);

  mutable std::mutex mutex_, fobs_mutex_;
  std::vector<boost::asio::ip::udp::endpoint> bootstrap_endpoints_;
  boost::filesystem::path bootstrap_path_;
  std::map<NodeId, asymm::PublicKey> public_keys_;
  size_t client_index_;

 public:
  std::vector<NodePtr> nodes_;
};

class NodesEnvironment : public testing::Environment {
 public:
  NodesEnvironment(size_t num_server_nodes, size_t num_client_nodes)
    : num_server_nodes_(num_server_nodes),
      num_client_nodes_(num_client_nodes) {}

  void SetUp() {
    g_env_->GenericNetwork::SetUp();
    g_env_->SetUpNetwork(num_server_nodes_, num_client_nodes_);
  }
  void TearDown() {
    g_env_->GenericNetwork::TearDown();
  }

  static std::shared_ptr<GenericNetwork> g_environment() {
    return g_env_;
  }
 private:
  size_t num_server_nodes_;
  size_t num_client_nodes_;
  static std::shared_ptr<GenericNetwork> g_env_;
};

}  // namespace test

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TESTS_ROUTING_NETWORK_H_
