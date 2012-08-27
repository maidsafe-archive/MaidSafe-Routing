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

#include "maidsafe/common/rsa.h"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/node_id.h"


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
  NodeInfo DropNode(const boost::asio::ip::udp::endpoint& endpoint);
  bool GetNodeInfo(const boost::asio::ip::udp::endpoint& endpoint, NodeInfo& peer) const;
  bool GetNodeInfo(const NodeId& node_id, NodeInfo& peer) const;
  bool IsThisNodeInRange(const NodeId& target_id, const uint16_t range);
  bool IsThisNodeClosestTo(const NodeId& target_id);
  bool IsConnected(const boost::asio::ip::udp::endpoint& endpoint) const;
  bool IsConnected(const NodeId& node_id) const;
  bool ConfirmGroupMembers(const NodeId& node1, const NodeId& node2);
  // Returns default-constructed NodeId if routing table size is zero
  NodeInfo GetClosestNode(const NodeId& target_id, bool ignore_exact_match = false);
  NodeInfo GetClosestNode(const NodeId& target_id, const std::vector<std::string>& exclude,
                          bool ignore_exact_match, bool ignore_symmetric = false);
  // Returns max NodeId if routing table size is less than requested node_number
  NodeInfo GetNthClosestNode(const NodeId& target_id, const uint16_t& node_number);
  std::vector<NodeId> GetClosestNodes(const NodeId& target_id, const uint16_t& number_to_get);
  uint16_t Size() const;
  asymm::Keys kKeys() const;
  void set_network_status_functor(NetworkStatusFunctor network_status_functor);
  void set_close_node_replaced_functor(CloseNodeReplacedFunctor close_node_replaced_functor);
  void set_keys(const asymm::Keys& keys);
  bool client_mode() const { return client_mode_; }
  void set_bootstrap_file_path(const boost::filesystem::path& path);
  friend class test::GenericNode;

 private:
  RoutingTable(const RoutingTable&);
  RoutingTable& operator=(const RoutingTable&);
  bool AddOrCheckNode(NodeInfo& node, const bool& remove);
  void UpdateGroupChangeAndNotify();
  int16_t BucketIndex(const NodeId& rhs) const;
  bool CheckValidParameters(const NodeInfo& node) const;
  bool CheckParametersAreUnique(const NodeInfo& node) const;
  bool MakeSpaceForNodeToBeAdded(NodeInfo& node, const bool& remove);
  void PartialSortFromTarget(const NodeId& target, const uint16_t& number);
  void NthElementSortFromTarget(const NodeId& target, const uint16_t& nth_element);
  NodeId FurthestCloseNode();
  std::vector<NodeInfo> GetClosestNodeInfo(const NodeId& from, const uint16_t& number_to_get,
                                           bool ignore_exact_match = false);
  std::vector<NodeInfo> GetClosestNodeInfo(const NodeId& from, const uint16_t& number_to_get,
                                           bool ignore_exact_match,
                                           bool ignore_symmetric);

  void update_network_status() const;

  const uint16_t max_size_;
  bool client_mode_;
  asymm::Keys keys_;
  bool sorted_;
  const NodeId kNodeId_;
  NodeId furthest_group_node_id_;
  mutable std::mutex mutex_;
  NetworkStatusFunctor network_status_functor_;
  CloseNodeReplacedFunctor close_node_replaced_functor_;
  boost::filesystem::path bootstrap_file_path_;
  std::vector<NodeInfo> nodes_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_TABLE_H_
