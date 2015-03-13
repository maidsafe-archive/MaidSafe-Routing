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

namespace maidsafe {

namespace routing {

class ConnectionManager {
  using PublicPmid = passport::PublicPmid;

  class Comparison {
   public:
    explicit Comparison(Address our_id) : our_id_(std::move(our_id)) {}

    bool operator()(const Address& lhs, const Address& rhs) const {
      return CloserToTarget(lhs, rhs, our_id_);
    }

   private:
    const Address our_id_;
  };

 public:
  ConnectionManager(boost::asio::io_service& ios, PublicPmid our_fob);

  ConnectionManager(const ConnectionManager&) = delete;
  ConnectionManager(ConnectionManager&&) = delete;
  ~ConnectionManager() = default;
  ConnectionManager& operator=(const ConnectionManager&) = delete;
  ConnectionManager& operator=(ConnectionManager&&) = delete;

  bool IsManaged(const Address& node_to_add) const;
  std::set<Address, Comparison> GetTarget(const Address& target_node) const;
  //boost::optional<CloseGroupDifference> LostNetworkConnection(const Address& node);
  // routing wishes to drop a specific node (may be a node we cannot connect to)
  boost::optional<CloseGroupDifference> DropNode(const Address& their_id);
  void AddNode(boost::optional<NodeInfo> node_to_add, EndpointPair);

  std::vector<PublicPmid> OurCloseGroup() const {
    std::vector<PublicPmid> result;
    result.reserve(GroupSize);
    size_t i = 0;
    for (const auto& pair : peers_) {
      if (++i > GroupSize)
        break;
      result.push_back(pair.second.node_info().dht_fob);
    }
    return result;
  }

  //size_t CloseGroupBucketDistance() const {
  //  return routing_table_.BucketIndex(routing_table_.OurCloseGroup().back().id);
  //}

  bool AddressInCloseGroupRange(const Address& address) const {
    if (peers_.size() < GroupSize)
      return true;
    return (static_cast<std::size_t>(std::distance(peers_.begin(), peers_.upper_bound(address))) <
            GroupSize);
  }

  const Address& OurId() const { return our_id_; }

  boost::optional<asymm::PublicKey> GetPublicKey(const Address& node) const {
    auto found_i = peers_.find(node);
    if (found_i == peers_.end()) { return boost::none; }
    return found_i->second.node_info().dht_fob.public_key();
  }

  //bool CloseGroupMember(const Address& their_id);

  uint32_t Size() { return static_cast<uint32_t>(peers_.size()); }

  PeerNode* FindPeer(Address addr) {
    auto i = peers_.find(addr);
    if (i == peers_.end())
      return nullptr;
    return &i->second;
  }

  void StartAccepting(unsigned short port);

  template<class Handler /* void(NodeId) */>
  void SetOnConnectionAdded(Handler handler) {
    on_connection_added_ = std::move(handler);
  }

  template<class Handler /* void(NodeId, SerialisedMessage) */>
  void SetOnReceive(Handler handler) {
    on_receive_ = std::move(handler);
  }

  void Shutdown() {
    acceptors_.clear();
    being_connected_.clear();
    peers_.clear();
  }

 private:
  boost::optional<CloseGroupDifference> GroupChanged();
  void InsertPeer(PeerNode&&);
  std::weak_ptr<boost::none_t> DestroyGuard() { return destroy_indicator_; }
  void StartReceiving(PeerNode&);

 private:
  boost::asio::io_service& io_service_;

  std::function<void(Address)> on_connection_added_;
  std::function<void(Address, const SerialisedMessage&)> on_receive_;

  PublicPmid our_fob_;
  Address our_id_;

  std::map<unsigned short, std::unique_ptr<crux::acceptor>> acceptors_;
  std::map<crux::endpoint, std::shared_ptr<crux::socket>> being_connected_;
  std::map<Address, PeerNode, Comparison> peers_;

  std::vector<Address> current_close_group_;

  std::shared_ptr<boost::none_t> destroy_indicator_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_CONNECTION_MANAGER_H_
