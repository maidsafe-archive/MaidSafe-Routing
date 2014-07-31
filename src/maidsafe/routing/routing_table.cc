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

#include "maidsafe/routing/routing_table.h"

#include <algorithm>
#include <bitset>
#include <limits>
#include <map>

#include "maidsafe/common/log.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/tools/network_viewer.h"

#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/close_nodes_change.h"

namespace maidsafe {

namespace routing {

template <>
RoutingTable<ClientNode>::RoutingTable(const SelfNodeId& node_id, const asymm::Keys& keys)
    : kNodeId_(node_id), kConnectionId_(NodeId(NodeId::IdType::kRandomId)), kKeys_(keys), mutex_(),
      routing_table_change_functor_(), nodes_() {
}

template <>
bool RoutingTable<ClientNode>::AddOrCheckNode(NodeInfo peer, bool remove) {
  if (peer.id.IsZero() || peer.id == kNodeId_) {
    LOG(kError) << "Attempt to add an invalid node " << peer.id;
    return false;
  }
  if (remove && !asymm::ValidateKey(peer.public_key)) {
    LOG(kInfo) << "Invalid public key for node " << DebugId(peer.id);
    return false;
  }

  bool return_value(false);
  NodeInfo removed_node;
  uint16_t routing_table_size(0);
  std::shared_ptr<CloseNodesChange> close_nodes_change;

  if (remove)
    SetBucketIndex(peer);
  {
    std::unique_lock<std::mutex> lock(mutex_);
    auto found(Find(peer.id, lock));
    if (found.first) {
      LOG(kVerbose) << "Node " << peer.id << " already in routing table.";
      return false;
    }

    PartialSortFromTarget(kNodeId_, Parameters::max_routing_table_size, lock);

    if (MakeSpaceForNodeToBeAdded(peer, remove, removed_node, lock)) {
      if (remove) {
        assert(peer.bucket != NodeInfo::kInvalidBucket);
        nodes_.push_back(peer);
      }
      return_value = true;
    }
    routing_table_size = static_cast<uint16_t>(nodes_.size());
  }

  if (return_value && remove) {  // Firing functors on Add only
    if (routing_table_change_functor_) {
      routing_table_change_functor_(
          RoutingTableChange(peer, RoutingTableChange::Remove(removed_node, false), true,
                             close_nodes_change, NetworkStatus(routing_table_size)));
    }

    if (peer.nat_type == rudp::NatType::kOther) {  // Usable as bootstrap endpoint
                                                   // if (new_bootstrap_endpoint_)
                                                   // new_bootstrap_endpoint_(peer.endpoint);
    }
    LOG(kInfo) << PrintRoutingTable();
  }
  return return_value;
}

template <>
NodeInfo RoutingTable<ClientNode>::DropNode(const NodeId& node_to_drop, bool routing_only) {
  NodeInfo dropped_node;
  uint16_t routing_table_size(0);
  std::shared_ptr<CloseNodesChange> close_nodes_change;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    PartialSortFromTarget(kNodeId_, Parameters::closest_nodes_size + 1, lock);
    auto found(Find(node_to_drop, lock));
    if (found.first) {
      dropped_node = *found.second;
      nodes_.erase(found.second);
      routing_table_size = static_cast<uint16_t>(nodes_.size());
    }
  }

  if (!dropped_node.id.IsZero()) {
    if (routing_table_change_functor_) {
      routing_table_change_functor_(
          RoutingTableChange(NodeInfo(), RoutingTableChange::Remove(dropped_node, routing_only),
                             false, close_nodes_change, NetworkStatus(routing_table_size)));
    }
  }
  LOG(kInfo) << PrintRoutingTable();
  return dropped_node;
}

template <>
bool RoutingTable<ClientNode>::MakeSpaceForNodeToBeAdded(const NodeInfo& node, bool remove,
                                                         NodeInfo& removed_node,
                                                         std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());

  if (remove && !CheckPublicKeyIsUnique(node, lock))
   return false;

  if (nodes_.size() < Params<ClientNode>::max_routing_table_size)
   return true;

  assert(nodes_.size() == Params<ClientNode>::max_routing_table_size);
  if (NodeId::CloserToTarget(node.id, nodes_.at(Params<ClientNode>::max_routing_table_size - 1).id,
                             kNodeId())) {
    removed_node = *nodes_.rbegin();
    nodes_.pop_back();
    return true;
  }
  return false;
}

}  // namespace routing

}  // namespace maidsafe
