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

#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/routing_impl.h"


namespace maidsafe {

namespace routing {

namespace { typedef boost::asio::ip::udp::endpoint Endpoint; }

template<>
Routing::Routing(std::nullptr_t) : pimpl_() {
  InitialisePimpl(true, true, NodeId(NodeId::kRandomId), asymm::GenerateKeyPair());
}

void Routing::InitialisePimpl(bool client_mode,
                              bool anonymous,
                              const NodeId& node_id,
                              const asymm::Keys& keys) {
  pimpl_.reset(new Impl(client_mode, anonymous, node_id, keys));
}

void Routing::Join(Functors functors, std::vector<Endpoint> peer_endpoints) {
  pimpl_->Join(functors, peer_endpoints);
}

int Routing::ZeroStateJoin(Functors functors,
                           const Endpoint& local_endpoint,
                           const Endpoint& peer_endpoint,
                           const NodeInfo& peer_info) {
  return pimpl_->ZeroStateJoin(functors, local_endpoint, peer_endpoint, peer_info);
}

std::future<std::string> Routing::Send(const NodeId& destination_id,
                                       const std::string& data,
                                       const bool& cacheable) {
  return pimpl_->Send(destination_id, data, cacheable);
}

std::vector<std::future<std::string>> Routing::SendGroup(const NodeId& destination_id,
                                                         const std::string& data,
                                                         const bool& cacheable) {
  return pimpl_->SendGroup(destination_id, data, cacheable);
}

NodeId Routing::GetRandomExistingNode() const {
  return pimpl_->GetRandomExistingNode();
}

bool Routing::IsNodeIdInGroupRange(const NodeId& node_id) const {
  return pimpl_->IsNodeIdInGroupRange(node_id);
}

bool Routing::EstimateInGroup(const NodeId& sender_id, const NodeId& info_id) const {
  return pimpl_->EstimateInGroup(sender_id, info_id);
}

std::future<std::vector<NodeId>> Routing::GetGroup(const NodeId& info_id) {
  return std::move(pimpl_->GetGroup(info_id));
}

NodeId Routing::kNodeId() const {
  return pimpl_->kNodeId();
}

int Routing::network_status() {
  return pimpl_->network_status();
}

std::vector<NodeInfo> Routing::ClosestNodes() {
  return pimpl_->ClosestNodes();
}

bool Routing::IsConnectedVault(const NodeId& node_id) {
  return pimpl_->IsConnectedVault(node_id);
}

uint64_t Routing::NetworkPopulation() const {
  return pimpl_->NetworkPopulation();
}

// bool Routing::IsConnectedToClient(const NodeId& node_id) {
//  return pimpl_->IsConnectedToClient(node_id);
// }

}  // namespace routing

}  // namespace maidsafe
