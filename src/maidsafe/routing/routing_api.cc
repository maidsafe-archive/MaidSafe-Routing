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

#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/routing_impl.h"

namespace maidsafe {

namespace routing {

namespace {
typedef boost::asio::ip::udp::endpoint Endpoint;
}

Routing::Routing() : pimpl_() {
  InitialisePimpl(true, NodeId(RandomString(NodeId::kSize)), asymm::GenerateKeyPair());
}

Routing::~Routing() {
  pimpl_->Stop();
}

void Routing::InitialisePimpl(bool client_mode, const NodeId& node_id, const asymm::Keys& keys) {
  pimpl_.reset(new Impl(client_mode, node_id, keys));
}

void Routing::Join(Functors functors) {
  pimpl_->Join(functors);
}

int Routing::ZeroStateJoin(Functors functors, const Endpoint& local_endpoint,
                           const Endpoint& peer_endpoint, const NodeInfo& peer_info) {
  return pimpl_->ZeroStateJoin(functors, local_endpoint, peer_endpoint, peer_info);
}

// Send methods
template <>
void Routing::Send(const SingleToSingleMessage& message) {
  pimpl_->Send(message);
}

template <>
void Routing::Send(const SingleToGroupMessage& message) {
  pimpl_->Send(message);
}

template <>
void Routing::Send(const GroupToSingleMessage& message) {
  pimpl_->Send(message);
}

template <>
void Routing::Send(const GroupToGroupMessage& message) {
  pimpl_->Send(message);
}

template <>
void Routing::Send(const GroupToSingleRelayMessage& message) {
  pimpl_->Send(message);
}

void Routing::SendDirect(const NodeId& destination_id, const std::string& message, bool cacheable,
                         ResponseFunctor response_functor) {
  return pimpl_->SendDirect(destination_id, message, cacheable, response_functor);
}

void Routing::SendGroup(const NodeId& destination_id, const std::string& message, bool cacheable,
                        ResponseFunctor response_functor) {
  return pimpl_->SendGroup(destination_id, message, cacheable, response_functor);
}

bool Routing::ClosestToId(const NodeId& target_id) { return pimpl_->ClosestToId(target_id); }

NodeId Routing::RandomConnectedNode() { return pimpl_->RandomConnectedNode(); }

bool Routing::EstimateInGroup(const NodeId& sender_id, const NodeId& info_id) const {
  return pimpl_->EstimateInGroup(sender_id, info_id);
}

NodeId Routing::kNodeId() const { return pimpl_->kNodeId(); }

int Routing::network_status() { return pimpl_->network_status(); }


bool Routing::IsConnectedVault(const NodeId& node_id) { return pimpl_->IsConnectedVault(node_id); }

bool Routing::IsConnectedClient(const NodeId& node_id) {
  return pimpl_->IsConnectedClient(node_id);
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
      if (updated_health < current_health)
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
