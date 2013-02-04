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

#ifndef MAIDSAFE_ROUTING_NODE_INFO_H_
#define MAIDSAFE_ROUTING_NODE_INFO_H_

#include <cstdint>
#include <vector>

#include "boost/asio/ip/udp.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/rsa.h"

#include "maidsafe/rudp/managed_connections.h"

namespace maidsafe {

namespace routing {

namespace protobuf { class Contact; }

struct NodeInfo {
  typedef TaggedValue<NonEmptyString, struct SerialisedMessageTag> serialised_type;

  NodeInfo();

  NodeInfo(const NodeInfo& other);
  NodeInfo& operator=(const NodeInfo& other);
  NodeInfo(NodeInfo&& other);
  NodeInfo& operator=(NodeInfo&& other);
  explicit NodeInfo(const serialised_type& serialised_message);

  serialised_type Serialise() const;

  NodeId node_id;
  NodeId connection_id;  // Id of a node as far as rudp is concerned
  asymm::PublicKey public_key;
  int32_t rank;
  int32_t bucket;
  rudp::NatType nat_type;
  std::vector<int32_t> dimension_list;

  static const int32_t kInvalidBucket;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_NODE_INFO_H_
