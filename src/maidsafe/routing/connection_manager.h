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

/*
This class is the API for the messages and routing object. It creates a routing table
and allows that to be managed by itself. This object will provide id's to connect
to that will allow routing to send a message closer to a target. If a close node cannot
be directly connected it will be added, only if, it is within the close nodes range.

This requires that the message_handler
1: On reciept of a message id (source + message id + destination) will
  a: send on to any id provided and firewall the message
  b: If multiple destinations are provided then the same happens
2: Prior to sending the node must check the message is not already firewalled
(outgoing message check)

To maintain close nodes effectivly the message_handler should request a close_group
request to its group when it sees any close group request in it's group. This is
obvious as the destination nodes for a messag ein your close group has multiple
destiations. In that case request a close_group message for this node.
*/

#ifndef MAIDSAFE_ROUTING_CONNECTION_MANAGER_H_
#define MAIDSAFE_ROUTING_CONNECTION_MANAGER_H_

#include <functional>
#include <map>
#include <set>
#include <vector>

#include "asio/io_service.hpp"
#include "boost/optional.hpp"

#include "maidsafe/crux/socket.hpp"
#include "maidsafe/crux/acceptor.hpp"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/types.h"
#include "maidsafe/routing/peer_node.h"
#include "maidsafe/routing/connections.h"

namespace maidsafe {

namespace routing {

class Timer;

class ConnectionManager {
  using PublicPmid = passport::PublicPmid;

 public:
  using OnReceive = std::function<void(Address, const SerialisedMessage&)>;
  using OnAddNode = std::function<void(asio::error_code, boost::optional<CloseGroupDifference>)>;
  using OnConnectionLost = std::function<void(boost::optional<CloseGroupDifference>, Address)>;

 private:
  struct ExpectedAccept {
    NodeInfo node_info;
    OnAddNode handler;
  };

 public:
  ConnectionManager(asio::io_service& ios, Address our_id, OnReceive on_receive,
                    OnConnectionLost on_connection_lost);

  ConnectionManager(const ConnectionManager&) = delete;
  ConnectionManager(ConnectionManager&&) = delete;

  ConnectionManager& operator=(const ConnectionManager&) = delete;
  ConnectionManager& operator=(ConnectionManager&&) = delete;

  bool SuggestNodeToAdd(const Address& node_to_add) const;
  std::vector<NodeInfo> GetTarget(const Address& target_node) const;
  std::set<Address> GetNonRoutingNodes() const;

  boost::optional<CloseGroupDifference> LostNetworkConnection(const Address& node);

  // routing wishes to drop a specific node (may be a node we cannot connect to)
  // boost::optional<CloseGroupDifference> DropNode(const Address& their_id);
  void DropNode(const Address& their_id);

  void AddNode(NodeInfo node_to_add, EndpointPair their_endpoint_pair, OnAddNode);
  void AddNodeAccept(NodeInfo node_to_add, EndpointPair their_endpoint_pair, OnAddNode);

  std::vector<NodeInfo> OurCloseGroup() const { return routing_table_.OurCloseGroup(); }

  size_t CloseGroupBucketDistance() const {
    return routing_table_.BucketIndex(routing_table_.OurCloseGroup().back().id);
  }

  bool AddressInCloseGroupRange(const Address& address) const {
    if (routing_table_.Size() < GroupSize) {
      return true;
    }
    return CloserToTarget(address, routing_table_.OurCloseGroup().back().id,
                          routing_table_.OurId());
  }

  const Address& OurId() const { return routing_table_.OurId(); }

  boost::optional<asymm::PublicKey> GetPublicKey(const Address& node) const {
    return routing_table_.GetPublicKey(node);
  }

  bool CloseGroupMember(const Address& their_id);

  std::size_t Size() { return routing_table_.Size(); }

  template <class Handler /* void (error_code) */>
  void Send(const Address&, const SerialisedMessage&, Handler);

  template <class Handler /* void (error_code) */>
  void SendToNonRoutingNode(const Address&, const SerialisedMessage&, Handler);

  Port AcceptingPort() const { return our_accept_port_; }

  template <class Handler /* void (error_code, Address, Endpoint our_endpoint) */>
  void Connect(asio::ip::udp::endpoint, Handler);

  void Shutdown() { connections_.reset(); }

 private:
  boost::optional<CloseGroupDifference> AddToRoutingTable(NodeInfo node_to_add);
  void StartReceiving();
  void StartAccepting();

  void HandleAddNode(asio::error_code, NodeInfo, OnAddNode);
  void HandleConnectionLost(Address);

  boost::optional<CloseGroupDifference> GroupChanged();

 private:
  asio::io_service& io_service_;
  mutable std::mutex mutex_;
  Port our_accept_port_;
  RoutingTable routing_table_;
  std::set<Address> connected_non_routing_nodes_;  // clients & bootstrapping nodes
  OnReceive on_receive_;
  OnConnectionLost on_connection_lost_;
  std::vector<Address> current_close_group_;
  std::map<Address, ExpectedAccept> expected_accepts_;
  std::shared_ptr<Connections> connections_;
};

template <class Handler /* void (error_code) */>
void ConnectionManager::Send(const Address& addr, const SerialisedMessage& message,
                             Handler handler) {
  std::weak_ptr<Connections> guard = connections_;
  //LOG(kVerbose) << OurId() << " Send to node " << addr << ", msg : " << hex::Substr(message);
  connections_->Send(addr, message, [=](asio::error_code error) mutable {
      handler(error);

    if (!guard.lock())
      return;

    if (error) {
      HandleConnectionLost(std::move(addr));
    }
  });
}

template <class Handler /* void (error_code) */>
void ConnectionManager::SendToNonRoutingNode(const Address& addr,
                                             const SerialisedMessage& message,
                                             Handler handler) {
  auto found = connected_non_routing_nodes_.find(addr);
  if (found != connected_non_routing_nodes_.end()) {
    Send(addr, message, handler);
  } else {
    handler(make_error_code(RoutingErrors::not_connected));
  }
}


template <class Handler /* void (error_code, Address, Endpoint our_endpoint) */>
void ConnectionManager::Connect(asio::ip::udp::endpoint remote_endpoint, Handler handler) {
  connections_->Connect(remote_endpoint,
                        [=](asio::error_code error, Connections::ConnectResult result) {
                          handler(error, result.his_address, result.our_endpoint);
                        });
}

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_CONNECTION_MANAGER_H_
