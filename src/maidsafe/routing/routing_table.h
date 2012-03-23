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
#include <set>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include "boost/signals2/signal.hpp"
#include "boost/thread/shared_mutex.hpp"
#include "boost/thread/mutex.hpp"

#include "maidsafe/common/rsa.h"
#include "maidsafe/transport/managed_connection.h"

#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/log.h"


namespace maidsafe {

namespace routing {


struct NodeInfo {
  NodeInfo() :
  node_id(), public_key(), rank(), bucket(99999), endpoint(), dimension_1(),
  dimension_2(), dimension_3(), dimension_4() {}
  NodeId node_id;
  asymm::PublicKey public_key;
  int32_t rank;
  int32_t bucket;
  transport::Endpoint endpoint;
  int32_t dimension_1;
  int32_t dimension_2;
  int32_t dimension_3;
  int32_t dimension_4;
};

class RoutingTable {
 public:
  explicit RoutingTable(const NodeId &node_id,
                        std::shared_ptr<transport::ManagedConnection> transport
                        );
  ~RoutingTable();
  bool AddNode(NodeInfo &node);
  bool CheckNode(NodeInfo &node);
  bool DropNode(const transport::Endpoint &endpoint);
  bool IsMyNodeInRange(const NodeId &node_id, unsigned int closest_nodes);
  bool AmIClosestNode(const NodeId &node_id);
  std::vector<NodeId> GetClosestNodes(const NodeId &from,
                                      unsigned int number_to_get);
  NodeInfo GetClosestNode(const NodeId &from, unsigned int node_number);
  void SendOn(protobuf::Message &message);
  unsigned int Size() {
    return static_cast<uint16_t>(routing_table_nodes_.size());
  }
  NodeId kNodeId() const { return kNodeId_; }
  unsigned int ClosestNodesSize();
 private:
  RoutingTable(const RoutingTable&);
  RoutingTable& operator=(const RoutingTable&);
  bool AddOrCheckNode(NodeInfo &node, bool remove);
  int16_t BucketIndex(const NodeId &rhs) const;
  bool CheckValidParameters(const NodeInfo &node) const;
  bool CheckarametersAreUnique(const NodeInfo &node) const;
  bool MakeSpaceForNodeToBeAdded(NodeInfo &node, bool remove_and_add);
  void SortFromThisNode(const NodeId &from);
  void PartialSortFromThisNode(const NodeId &from, int16_t number_to_sort);
  bool RemoveClosecontact(const NodeId &node_id);
  bool AddcloseContact(const protobuf::Contact &contact);
  bool sorted_;
  const NodeId kNodeId_;
  std::vector<NodeInfo> routing_table_nodes_;
  boost::mutex mutex_;
  std::shared_ptr<transport::ManagedConnection> transport_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_TABLE_H_
