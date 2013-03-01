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
Routing::Routing(const NodeId& node_id) : pimpl_() {
  InitialisePimpl(true, node_id, asymm::GenerateKeyPair());
}

void Routing::InitialisePimpl(bool client_mode,
                              const NodeId& node_id,
                              const asymm::Keys& keys) {
  pimpl_.reset(new Impl(client_mode, node_id, keys));
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

void Routing::SendDirect(const NodeId& destination_id,
                   const std::string& data,
                   const bool& cacheable,
                   ResponseFunctor response_functor) {
  return pimpl_->SendDirect(destination_id, data, cacheable, response_functor);
}

void Routing::SendGroup(const NodeId& destination_id,
                        const std::string& data,
                        const bool& cacheable,
                        ResponseFunctor response_functor) {
  return pimpl_->SendGroup(destination_id, data, cacheable, response_functor);
}

NodeId Routing::GetRandomExistingNode() const {
  return pimpl_->GetRandomExistingNode();
}

bool Routing::ClosestToId(const NodeId& node_id) {
  return pimpl_->ClosestToId(node_id);
}

GroupRangeStatus Routing::IsNodeIdInGroupRange(const NodeId& node_id) const {
  return pimpl_->IsNodeIdInGroupRange(node_id);
}

NodeId Routing::RandomConnectedNode() {
  return pimpl_->RandomConnectedNode();
}

bool Routing::EstimateInGroup(const NodeId& sender_id, const NodeId& info_id) const {
  return pimpl_->EstimateInGroup(sender_id, info_id);
}

std::future<std::vector<NodeId>> Routing::GetGroup(const NodeId& info_id) {
  return pimpl_->GetGroup(info_id);
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

bool Routing::IsConnectedClient(const NodeId& node_id) {
  return pimpl_->IsConnectedClient(node_id);
}

}  // namespace routing

}  // namespace maidsafe
