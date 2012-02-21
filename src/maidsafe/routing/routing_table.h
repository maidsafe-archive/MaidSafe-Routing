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
 * to ensure connections are fair and calculable. i.e. to accept a connection
 * there has to be a reason to, otherwise connections will imbalance.
 * The algorithm flips the MSB recursively (all the way in a full table to the
 * LSB)  e.g. 010101011 - we would first find node closest to 110101011 then
 * 000101011 then 011101011 and so on. Each flip represents an ever decreasing
 * part of the network (getting closer with more knowledge). For number of nodes
 * per 'bucket' then simply search the next bits that represent bucket size
 * (i.e.) for 4 nodes per bucket search 1[00 -> 11]101011 which will find any
 * nodes in this area.
 * As each bucket is searched and populated there will be a stop which is
 * natural unless the address space is full. At the point this stop happens we
 * go back up again adding more nodes to the buckets (by same method)
 * we can till we have the
 * min nodes in our routing table. (say 64). This balances our RT as fair across
 * the address range as possible, even when almost empty. On start-up of course
 * the algorithm will detect the distance between our nodes will not even allow
 * us to reach a full routing table.
 * This routing table uses only rUDP and managed connections, no other protocol
 * will work.
 * To achieve this we need to be able to manipulate the node class a little
 * more. This node object will be updated to allow this kind of traversal of
 * MSB flipping.
 * All of this is NOT in the DHT API, although 'get all nodes' may be supplied
 * as impl if required, best not to though and let this object internally
 * handle all routing on it's own. There should not be a requirement for
 * any other library to access these internals (AFAIK).
 */

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
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/log.h"


namespace maidsafe {

namespace transport { struct Info; }

namespace routing {
  typedef protobuf::Contact Contact;
  class ManagedConnections;
class RoutingTable {
 public:
  RoutingTable(const Contact &my_contact);
  ~RoutingTable();
  int AddContact(const Contact &contact);
  int GetContact(const NodeId &node_id, Contact *contact);
  protobuf::ClosestContacts GetMyClosestContacts();
  int16_t Size() { return routing_table_nodes_.size(); }
 private:
  RoutingTable operator =(const RoutingTable &assign_object);
  RoutingTable(const RoutingTable &copy_object);
  bool isClose(const NodeId &node_id) const;
  void InsertContact(const Contact &contact);
  int16_t BucketIndex(const NodeId &rhs) const;
  bool IsSpaceForNodeToBeAdded();
  void SortFromMe();
  bool RemoveClosecontact(const NodeId &node_id);
  bool AddcloseContact(const Contact &contact);
  NodeId DistanceTo(const NodeId &rhs) const;
  const NodeId kMyNodeId_;
  std::vector<NodeId> routing_table_nodes_;
  std::queue<NodeId> unvalidated_contacts_;
  boost::shared_mutex shared_mutex_;
  boost::mutex closest_contacts_mutex_;
  boost::mutex routing_table_nodes_mutex_;
//   ManagedConnections MC_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_TABLE_H_
