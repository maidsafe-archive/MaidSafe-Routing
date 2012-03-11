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

#include <algorithm>

#include "boost/thread/locks.hpp"

#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/log.h"

namespace maidsafe {
namespace routing {
  
RoutingTable::RoutingTable(const Contact &my_contact)
    : sorted_(false),
      kMyNodeId_(NodeId(my_contact.node_id())),
      routing_table_nodes_(),
      mutex_() {}

RoutingTable::~RoutingTable() {
  boost::mutex::scoped_lock lock(mutex_);
  routing_table_nodes_.clear();
}

bool RoutingTable::AddNode(const NodeId &node_id) {
  boost::mutex::scoped_lock lock(mutex_);
  if (node_id == kMyNodeId_) {
    return false;
  }
  // if we already have node return true
  if (std::find(routing_table_nodes_.begin(),
                routing_table_nodes_.end(), node_id)
      != routing_table_nodes_.end())
    return true;
  if (Size() < kRoutingTableSize) {
    routing_table_nodes_.push_back(node_id);
    return true;
  } else if (MakeSpaceForNodeToBeAdded()) {
      routing_table_nodes_.push_back(node_id);
      return true;
  }
  return false;
}

bool RoutingTable::AmIClosestNode(const NodeId& node_id)
{
  boost::mutex::scoped_lock lock(mutex_);
  return ((kMyNodeId_ ^ node_id) <
          (node_id ^ routing_table_nodes_[0])) ;
}

bool RoutingTable::MakeSpaceForNodeToBeAdded() {
  if (kRoutingTableSize < routing_table_nodes_.size())
    return false;
  SortFromThisNode(kMyNodeId_);
  int i = 0;
  for (auto it = routing_table_nodes_.begin();
       it < routing_table_nodes_.end();
       ++it) {
    BucketIndex(*it) == BucketIndex(*(++it)) ? ++i : i = 0;
    if (i > kBucketSize) { // TODO (dirvine) do we need to go all the way here?
       routing_table_nodes_.erase(it);
       return true;
    }
  }
  return false;
}

void RoutingTable::SortFromThisNode(const NodeId &from) {
  if ((!sorted_)  || (from != kMyNodeId_))
      std::sort(routing_table_nodes_.begin(),
            routing_table_nodes_.end(),
            [this, from](const NodeId &i, const NodeId &j)
            { return (i ^ from) < (j ^ from); } );
  if (kMyNodeId_ == from)
    sorted_ = true;
  else
    sorted_ = false;
}

bool RoutingTable::IsMyNodeInRange(const NodeId& node_id, uint16_t range) {
  if (routing_table_nodes_.size() < range)
    return true;
  SortFromThisNode(kMyNodeId_);
  return (routing_table_nodes_[range] ^ kMyNodeId_) > (node_id ^ kMyNodeId_);
}

int16_t RoutingTable::BucketIndex(const NodeId &rhs) const {
  uint16_t distance = 0;
  std::string this_id_binary = kMyNodeId_.ToStringEncoded(NodeId::kBinary);
  std::string rhs_id_binary = rhs.ToStringEncoded(NodeId::kBinary);
  auto this_it = this_id_binary.begin();
  auto rhs_it = rhs_id_binary.begin();
  for (; ((this_it != this_id_binary.end()) && (*this_it == *rhs_it));
      ++this_it, ++rhs_it)
    ++distance;
  return (distance + 511) % 511;
}

NodeId RoutingTable::GetClosestNode(const NodeId &from, uint16_t node_number) {
 SortFromThisNode(from);
 return routing_table_nodes_[node_number];
}

std::vector<NodeId> RoutingTable::GetClosestNodes(const NodeId &from,
                                                       uint16_t number_to_get) {
  std::vector<NodeId>close_nodes;
  // routing_table_nodes_.size() should never be over uin16_t cast is safe
  boost::mutex::scoped_lock lock(mutex_);
  int16_t count = std::min(number_to_get,
                           static_cast<uint16_t>(routing_table_nodes_.size()));
  SortFromThisNode(from);
  close_nodes.resize(count);
  std::copy(routing_table_nodes_.begin(),
            routing_table_nodes_.begin() + count,
            close_nodes.begin());
  return close_nodes;
}

}  // namespace routing
}  // namespace maidsafe
