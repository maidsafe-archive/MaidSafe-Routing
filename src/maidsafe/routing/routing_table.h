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

#ifndef MAIDSAFE_ROUTING_ROUTING_TABLE_H_
#define MAIDSAFE_ROUTING_ROUTING_TABLE_H_

#include <cstdint>
#include <mutex>
#include <vector>
#include <string>

#include "boost/asio/ip/udp.hpp"
#include "boost/filesystem/path.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/rsa.h"

#include "maidsafe/routing/api_config.h"

namespace maidsafe {

namespace routing {

namespace test { class GenericNode; }

namespace protobuf { class Contact; }

struct NodeInfo;

class RoutingTable {
 public:
  RoutingTable(const asymm::Keys& keys, const bool& client_mode);
  bool AddNode(NodeInfo& peer);
  bool CheckNode(NodeInfo& peer);
  NodeInfo DropNode(const NodeId &node_to_drop);
  bool GetNodeInfo(const NodeId& node_id, NodeInfo& node_info) const;
  bool IsThisNodeInRange(const NodeId& target_id, const uint16_t range);
  bool IsThisNodeClosestTo(const NodeId& target_id);
  bool IsConnected(const NodeId& node_id) const;
  bool ConfirmGroupMembers(const NodeId& node1, const NodeId& node2);
  // Returns default-constructed NodeId if routing table size is zero
  NodeInfo GetClosestNode(const NodeId& target_id, bool ignore_exact_match = false);
  NodeInfo GetClosestNode(const NodeId& target_id, const std::vector<std::string>& exclude,
                          bool ignore_exact_match = false);
  // Returns max NodeId if routing table size is less than requested node_number
  NodeInfo GetNthClosestNode(const NodeId& target_id, const uint16_t& node_number);
  std::vector<NodeId> GetClosestNodes(const NodeId& target_id, const uint16_t& number_to_get);
  uint16_t Size() const;
  asymm::Keys kKeys() const;
  NodeId kNodeId() const;
  void set_network_status_functor(NetworkStatusFunctor network_status_functor);
  void set_close_node_replaced_functor(CloseNodeReplacedFunctor close_node_replaced_functor);
  void set_remove_node_functor(std::function<void(const NodeInfo&,
                                                  const bool&)> remove_node_functor);
  bool client_mode() const { return client_mode_; }
  friend class test::GenericNode;
#ifdef LOCAL_TEST
  friend struct RoutingPrivate;
#endif

 private:
  RoutingTable(const RoutingTable&);
  RoutingTable& operator=(const RoutingTable&);
  bool AddOrCheckNode(NodeInfo& node, const bool& remove);
  int16_t BucketIndex(const NodeId& rhs) const;
  bool CheckValidParameters(const NodeInfo& node) const;
  bool CheckParametersAreUnique(const NodeInfo& node) const;
  NodeInfo ResolveConnectionDuplication(const NodeInfo& new_duplicate_node,
                                        const bool& local_endpoint,
                                        NodeInfo& existing_node);
  std::vector<NodeInfo> CheckGroupChange();
  bool MakeSpaceForNodeToBeAdded(NodeInfo& node, const bool& remove, NodeInfo& removed_node);
  void PartialSortFromTarget(const NodeId& target, const uint16_t& number);
  void NthElementSortFromTarget(const NodeId& target, const uint16_t& nth_element);
  NodeId FurthestCloseNode();
  std::vector<NodeInfo> GetClosestNodeInfo(const NodeId& from, const uint16_t& number_to_get,
                                           bool ignore_exact_match = false);
  void update_network_status(const uint16_t& size) const;
  std::string PrintRoutingTable();
  const uint16_t max_size_;
  bool client_mode_;
  const asymm::Keys keys_;
  bool sorted_;
  const NodeId kNodeId_;
  NodeId furthest_group_node_id_;
  mutable std::mutex mutex_;
  std::function<void(const NodeInfo&, const bool&)> remove_node_functor_;
  NetworkStatusFunctor network_status_functor_;
  CloseNodeReplacedFunctor close_node_replaced_functor_;
  std::vector<NodeInfo> nodes_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_TABLE_H_
