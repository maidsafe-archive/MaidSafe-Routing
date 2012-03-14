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
#ifdef __MSVC__
#  pragma warning(push)
#  pragma warning(disable: 4127 4244 4267)
#endif
#include "maidsafe/routing/routing.pb.h"
#ifdef __MSVC__
#  pragma warning(pop)
#endif
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/log.h"

namespace maidsafe {

namespace routing {

typedef protobuf::Contact Contact;

class RoutingTable {
 public:
  explicit RoutingTable(const Contact &my_contact);
  ~RoutingTable();
  bool AddNode(const NodeId &node_id);
  bool IsMyNodeInRange(const NodeId &node_id, uint16_t closest_nodes);
  bool AmIClosestNode(const NodeId &node_id);
  std::vector<NodeId> GetClosestNodes(const NodeId &from,
                                      unsigned int number_to_get);
  NodeId GetClosestNode(const NodeId &from, unsigned int node_number);
  unsigned int Size() {
    return static_cast<unsigned int>(routing_table_nodes_.size());
  }

 private:
  RoutingTable(const RoutingTable&);
  RoutingTable& operator=(const RoutingTable&);
  void InsertContact(const Contact &contact);
  int16_t BucketIndex(const NodeId &rhs) const;
  bool MakeSpaceForNodeToBeAdded(const NodeId &node_id);
  void SortFromThisNode(const NodeId &from);
  void PartialSortFromThisNode(const NodeId &from, int16_t number_to_sort);
  bool RemoveClosecontact(const NodeId &node_id);
  bool AddcloseContact(const Contact &contact);
  bool sorted_;
  const NodeId kMyNodeId_;
  std::vector<NodeId> routing_table_nodes_;
  boost::mutex mutex_;
//   ManagedConnections MC_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_TABLE_H_
