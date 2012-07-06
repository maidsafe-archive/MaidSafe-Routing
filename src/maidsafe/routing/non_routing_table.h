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

#ifndef MAIDSAFE_ROUTING_NON_ROUTING_TABLE_H_
#define MAIDSAFE_ROUTING_NON_ROUTING_TABLE_H_

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

namespace protobuf { class Contact; }  //  namespace protobuf

class NonRoutingTable {
 public:
  explicit NonRoutingTable(const asymm::Keys &keys);
  bool AddNode(NodeInfo &node, const NodeId &furthest_close_node_id);
  bool CheckNode(NodeInfo &node, const NodeId &furthest_close_node_id);
  int16_t DropNodes(const NodeId &node_id);
  NodeInfo DropNode(const Endpoint &endpoint);
  NodeInfo GetNodeInfo(const Endpoint &endpoint);
  std::vector<NodeInfo> GetNodesInfo(const NodeId &node_id);
  bool AmIConnectedToEndpoint(const Endpoint& endpoint);
  uint16_t Size();
  asymm::Keys kKeys() const;
  bs2::signal<void(std::string, std::string)> &CloseNodeReplacedOldNewSignal();
  void set_keys(asymm::Keys keys);

 private:
  NonRoutingTable(const NonRoutingTable&);
  NonRoutingTable& operator=(const NonRoutingTable&);
  bool AddOrCheckNode(NodeInfo &node, const NodeId &furthest_close_node_id, const bool &add);
  bool CheckValidParameters(const NodeInfo &node) const;
  bool CheckParametersAreUnique(const NodeInfo &node) const;
  bool CheckRangeForNodeToBeAdded(NodeInfo &node, const NodeId &furthest_close_node_id,
                                  const bool &add);
  bool IsMyNodeInRange(const NodeId &node_id, const NodeId &furthest_close_node_id);
  uint16_t NonRoutingTableSize();

  asymm::Keys keys_;
  const NodeId kNodeId_;
  std::vector<NodeInfo> non_routing_table_nodes_;
  std::mutex mutex_;
  bs2::signal<void(std::string, std::string)> close_node_from_to_signal_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_NON_ROUTING_TABLE_H_
