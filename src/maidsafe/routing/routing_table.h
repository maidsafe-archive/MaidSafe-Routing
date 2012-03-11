/* Copyright (c) 2009 maidsafe.net limited
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    * Neither the name of the maidsafe.net limited nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * The purpose of this object is to dynamically manage a routing table.
 * Based on managed connections which require a connection agreement algorithm
 * This algorithm is simple in current state. We fill routing table with 64
 * or more accurately kRoutingTableSize nodes (we get closest nodes with
 * every find_node). When we have max nodes we itterate through routing
 * tabel and find the closest bucket the node wil fit in and test it
 * for existing kBucketSize nodes. If full we drop node. If there is space we
 * add the node. Any close node is added and this is tested during the AddNode
 * public method of routing, which is itself bhind the API and not part
 * of the library API.
 * All of this is NOT in the DHT API, although 'get all nodes' may be supplied
 * as impl if required, best not to though and let this object internally
 * handle all routing on it's own. There should not be a requirement for
 * any other library to access these internals (AFAIK).
 */

#ifndef MAIDSAFE_ROUTING_TABLE_H_
#define MAIDSAFE_ROUTING_TABLE_H_

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
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/log.h"

namespace maidsafe {

namespace routing {
  typedef protobuf::Contact Contact;
  
class RoutingTable {
 public:
  RoutingTable(const Contact &my_contact);
  ~RoutingTable();
  bool AddNode(const NodeId &node_id);
  bool IsMyNodeInRange(const NodeId &node_id, uint16_t = kReplicationSize);
  bool AmIClosestNode(const NodeId &node_id);
  std::vector<NodeId> GetClosestNodes(const NodeId &from,
                               uint16_t number_to_get = kClosestNodes);
  NodeId GetClosestNode(const NodeId &from, uint16_t node_number = 0);
  RoutingTable operator =(const RoutingTable &assign_object);
  int16_t Size() { return routing_table_nodes_.size(); }
 private:
  RoutingTable(const RoutingTable &copy_object);
  void InsertContact(const Contact &contact);
  int16_t BucketIndex(const NodeId &rhs) const;
  bool MakeSpaceForNodeToBeAdded();
  void SortFromThisNode(const NodeId &from);
  void PartialSortFromThisNode(const NodeId &from,
                               int16_t number_to_sort = kClosestNodes);
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

#endif  // MAIDSAFE_ROUTING_TABLE_H_
