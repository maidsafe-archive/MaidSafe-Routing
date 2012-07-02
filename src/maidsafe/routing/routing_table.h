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

#include <mutex>
#include <string>
#include <vector>

#include "boost/signals2/signal.hpp"

#include "maidsafe/common/rsa.h"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/parameters.h"

namespace bs2 = boost::signals2;

namespace maidsafe {

namespace routing {

namespace test {

class FindNode;

}  // namespace test


namespace protobuf { class Contact; }  //  namespace protobuf

class RoutingTable {
 public:
  explicit RoutingTable(const asymm::Keys &keys,
                        const bool &client_mode,
                        CloseNodeReplacedFunctor close_node_replaced_functor);
  bool AddNode(NodeInfo &node);
  bool CheckNode(NodeInfo &node);
  bool DropNode(const Endpoint &endpoint);
  bool GetNodeInfo(const Endpoint &endpoint, NodeInfo *node_info);
  bool IsMyNodeInRange(const NodeId &node_id, const uint16_t range);
  bool AmIClosestNode(const NodeId &node_id);
  bool AmIConnectedToEndpoint(const Endpoint& endpoint);
  std::vector<NodeId> GetClosestNodes(const NodeId &from, const uint16_t &number_to_get);
  NodeInfo GetClosestNode(const NodeId &from, const uint16_t &node_number);
  uint16_t Size();
  asymm::Keys kKeys() const;
  void set_close_node_replaced_functor(CloseNodeReplacedFunctor close_node_replaced);
  void set_keys(asymm::Keys keys);
  bool client_mode() { return client_mode_; }

  friend class test::FindNode;

 private:
  RoutingTable(const RoutingTable&);
  RoutingTable& operator=(const RoutingTable&);
  bool AddOrCheckNode(NodeInfo &node, const bool &remove);
  void UpdateGroupChangeAndNotify();
  int16_t BucketIndex(const NodeId &rhs) const;
  bool CheckValidParameters(const NodeInfo &node) const;
  bool CheckParametersAreUnique(const NodeInfo &node) const;
  bool MakeSpaceForNodeToBeAdded(NodeInfo &node, const bool &remove);
  void SortFromThisNode(const NodeId &from);
  void PartialSortFromThisNode(const NodeId &from, const uint16_t &number);
  void NthElementSortFromThisNode(const NodeId &from, const uint16_t &nth_element);
  bool RemoveClosecontact(const NodeId &node_id);
  bool AddcloseContact(const protobuf::Contact &contact);
  uint16_t RoutingTableSize();
  std::vector<NodeInfo> GetClosestNodeInfo(const NodeId &from, const uint16_t &number_to_get);

  bool client_mode_;
  asymm::Keys keys_;
  bool sorted_;
  const NodeId kNodeId_;
  NodeId furthest_group_node_id_;
  std::vector<NodeInfo> routing_table_nodes_;
  std::mutex mutex_;
  CloseNodeReplacedFunctor close_node_replaced_functor_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_TABLE_H_
