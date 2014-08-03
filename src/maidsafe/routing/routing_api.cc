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

#include <boost/variant.hpp>
#include "maidsafe/routing/routing_api.h"

namespace maidsafe {

namespace routing {

namespace detail {

class JoinVisitor : public boost::static_visitor<> {
 public:
  JoinVisitor(Functors functors, BootstrapContacts bootstrap_contacts)
      : functors_(functors), bootstrap_contacts_(bootstrap_contacts) {}

  template <typename ImplType>
  void operator()(std::shared_ptr<ImplType> impl) {
    impl->Join(functors_);
  }

 private:
  Functors functors_;
  const BootstrapContacts bootstrap_contacts_;
};

class ZeroStateJoinVisitor : public boost::static_visitor<int> {
 public:
  ZeroStateJoinVisitor(Functors functors, const Endpoint& local_endpoint,
                       const Endpoint& peer_endpoint, const NodeInfo& peer_info)
      : functors_(functors),
        local_endpoint_(LocalEndpoint(local_endpoint)),
        peer_endpoint_(PeerEndpoint(peer_endpoint)),
        peer_info_(PeerNodeInfo(peer_info)) {}

  template <typename ImplType>
  result_type operator()(ImplType& impl) {
    return impl->ZeroStateJoin(functors_, local_endpoint_, peer_endpoint_, peer_info_);
  }

 private:
  Functors functors_;
  const LocalEndpoint local_endpoint_;
  const PeerEndpoint peer_endpoint_;
  const PeerNodeInfo peer_info_;
};

template <typename MessageType>
class SendVisitor : public boost::static_visitor<> {
 public:
  SendVisitor(const MessageType& message) : message_(message) {}

  template <typename ImplType>
  void operator()(ImplType& impl) {
    impl->Send(message_);
  }

 private:
  MessageType message_;
};

class SendDirectVisitor : public boost::static_visitor<> {
 public:
  SendDirectVisitor(const PeerNodeId& destination_id, const std::string& message,
                    IsCacheable cacheable, ResponseFunctor response_functor)
      : destination_id_(destination_id),
        message_(message),
        cacheable_(cacheable),
        response_functor_(response_functor) {}

  template <typename ImplType>
  void operator()(ImplType& impl) {
    impl->SendDirect(destination_id_, message_, cacheable_, response_functor_);
  }

 private:
  const PeerNodeId destination_id_;
  const std::string message_;
  const IsCacheable cacheable_;
  ResponseFunctor response_functor_;
};

class SendGroupVisitor : public boost::static_visitor<> {
 public:
  SendGroupVisitor(const NodeId& destination_id, const std::string& message, bool cacheable,
                   ResponseFunctor response_functor)
      : destination_id_(destination_id),
        message_(message),
        cacheable_(cacheable),
        response_functor_(response_functor) {}

  template <typename ImplType>
  void operator()(ImplType& impl) {
    impl->SendGroup(destination_id_, message_, cacheable_, response_functor_);
  }

 private:
  NodeId destination_id_;
  std::string message_;
  bool cacheable_;
  ResponseFunctor response_functor_;
};

class ClosestToIdVisitor : public boost::static_visitor<bool> {
 public:
  ClosestToIdVisitor(const NodeId& target_id) : target_id_(target_id) {}

  template <typename ImplType>
  result_type operator()(ImplType& impl) {
    return impl->ClosestToId(target_id_);
  }

 private:
  NodeId target_id_;
};

class RandomConnectedNodeVisitor : public boost::static_visitor<NodeId> {
 public:
  RandomConnectedNodeVisitor() {}

  template <typename ImplType>
  result_type operator()(ImplType& impl) {
    return impl->RandomConnectedNode();
  }
};

class EstimateInGroupVisitor : public boost::static_visitor<bool> {
 public:
  EstimateInGroupVisitor(const NodeId& sender_id, const NodeId& info_id)
      : sender_id_(sender_id), info_id_(info_id) {}

  template <typename ImplType>
  result_type operator()(ImplType& impl) {
    return impl->EstimateInGroup(sender_id_, info_id_);
  }

 private:
  NodeId sender_id_;
  NodeId info_id_;
};

class NodeIdVisitor : public boost::static_visitor<NodeId> {
 public:
  NodeIdVisitor() {}

  template <typename ImplType>
  result_type operator()(ImplType& impl) {
    return impl->kNodeId();
  }
};

class NetworkStatusVisitor : public boost::static_visitor<int> {
 public:
  NetworkStatusVisitor() {}

  template <typename ImplType>
  result_type operator()(ImplType& impl) {
    return impl->network_status();
  }
};

class IsConnectedVaultVisitor : public boost::static_visitor<bool> {
 public:
  IsConnectedVaultVisitor(const NodeId& node_id) : node_id_(node_id) {}

  template <typename ImplType>
  result_type operator()(ImplType& impl) {
    return impl->IsConnectedVault(node_id_);
  }

 private:
  NodeId node_id_;
};

class IsConnectedClientVisitor : public boost::static_visitor<bool> {
 public:
  IsConnectedClientVisitor(const NodeId& node_id) : node_id_(node_id) {}

  template <typename ImplType>
  result_type operator()(ImplType& impl) {
    return impl->IsConnectedClient(node_id_);
  }

 private:
  NodeId node_id_;
};

}  // detail namespace


Routing::Routing() : pimpl_() {
  InitialisePimpl(NodeId(NodeId::IdType::kRandomId), asymm::GenerateKeyPair(), ClientNode());
}

void Routing::InitialisePimpl(const NodeId& node_id, const asymm::Keys& keys, VaultNode) {
  pimpl_ = std::make_shared<RoutingImpl<VaultNode>>(LocalNodeId(node_id), keys);
}

void Routing::InitialisePimpl(const NodeId& node_id, const asymm::Keys& keys, ClientNode) {
  pimpl_ = std::make_shared<RoutingImpl<ClientNode>>(LocalNodeId(node_id), keys);
}

void Routing::Join(Functors functors, BootstrapContacts bootstrap_contacts) {
  detail::JoinVisitor join_visitor(functors, bootstrap_contacts);
  boost::apply_visitor(join_visitor, pimpl_);
}

int Routing::ZeroStateJoin(Functors functors, const Endpoint& local_endpoint,
                           const Endpoint& peer_endpoint, const NodeInfo& peer_info) {
  detail::ZeroStateJoinVisitor zero_state_join_visitor(functors, local_endpoint, peer_endpoint,
                                                       peer_info);
  return boost::apply_visitor(zero_state_join_visitor, pimpl_);
}

// Send methods
template <>
void Routing::Send(const SingleToSingleMessage& message) {
  detail::SendVisitor<SingleToSingleMessage> send_visitor(message);
  boost::apply_visitor(send_visitor, pimpl_);
}

template <>
void Routing::Send(const SingleToGroupMessage& message) {
  detail::SendVisitor<SingleToGroupMessage> send_visitor(message);
  boost::apply_visitor(send_visitor, pimpl_);
}

template <>
void Routing::Send(const GroupToSingleMessage& message) {
  detail::SendVisitor<GroupToSingleMessage> send_visitor(message);
  boost::apply_visitor(send_visitor, pimpl_);
}

template <>
void Routing::Send(const GroupToGroupMessage& message) {
  detail::SendVisitor<GroupToGroupMessage> send_visitor(message);
  boost::apply_visitor(send_visitor, pimpl_);
}

template <>
void Routing::Send(const GroupToSingleRelayMessage& message) {
  detail::SendVisitor<GroupToSingleRelayMessage> send_visitor(message);
  boost::apply_visitor(send_visitor, pimpl_);
}

void Routing::SendDirect(const NodeId& destination_id, const std::string& message, bool cacheable,
                         ResponseFunctor response_functor) {
  detail::SendDirectVisitor send_direct_visitor(PeerNodeId(destination_id), message,
                                                IsCacheable(cacheable), response_functor);
  boost::apply_visitor(send_direct_visitor, pimpl_);
}

void Routing::SendGroup(const NodeId& destination_id, const std::string& message, bool cacheable,
                        ResponseFunctor response_functor) {
  detail::SendGroupVisitor send_group_visitor(destination_id, message, cacheable, response_functor);
  boost::apply_visitor(send_group_visitor, pimpl_);
}

bool Routing::ClosestToId(const NodeId& target_id) {
  detail::ClosestToIdVisitor closest_to_id_visitor(target_id);
  return boost::apply_visitor(closest_to_id_visitor, pimpl_);
}

NodeId Routing::RandomConnectedNode() {
  detail::RandomConnectedNodeVisitor random_connected_node_visitor;
  return boost::apply_visitor(random_connected_node_visitor, pimpl_);
}

bool Routing::EstimateInGroup(const NodeId& sender_id, const NodeId& info_id) const {
  detail::EstimateInGroupVisitor estimate_in_group_visitor(sender_id, info_id);
  return boost::apply_visitor(estimate_in_group_visitor, pimpl_);
}

NodeId Routing::kNodeId() const {
  detail::NodeIdVisitor node_id_visitor;
  return boost::apply_visitor(node_id_visitor, pimpl_);
}

int Routing::network_status() {
  detail::NetworkStatusVisitor network_status_visitor;
  return boost::apply_visitor(network_status_visitor, pimpl_);
}

bool Routing::IsConnectedVault(const NodeId& node_id) {
  detail::IsConnectedVaultVisitor is_connected_vault_visitor(node_id);
  return boost::apply_visitor(is_connected_vault_visitor, pimpl_);
}

bool Routing::IsConnectedClient(const NodeId& node_id) {
  detail::IsConnectedClientVisitor is_connected_client_visitor(node_id);
  return boost::apply_visitor(is_connected_client_visitor, pimpl_);
}

void UpdateNetworkHealth(int updated_health, int& current_health, std::mutex& mutex,
                         std::condition_variable& cond_var, const NodeId& this_node_id) {
  {
    std::lock_guard<std::mutex> lock{mutex};
#if USE_LOGGING
    if (updated_health >= 0) {
      std::string message{DebugId(this_node_id) + " - Network health is " +
                          std::to_string(updated_health) + "% (was " +
                          std::to_string(current_health) + "%)"};
      if (updated_health >= current_health)
        LOG(kVerbose) << message;
      else
        LOG(kWarning) << message;
    } else {
      LOG(kWarning) << DebugId(this_node_id) << " - Network is down (" << updated_health << "%)";
    }
#endif
    current_health = updated_health;
  }
  cond_var.notify_one();
  static_cast<void>(this_node_id);
}

}  // namespace routing

}  // namespace maidsafe
