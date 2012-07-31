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

#include "boost/asio/ip/udp.hpp"

#include "maidsafe/common/rsa.h"

#include "maidsafe/routing/node_id.h"


namespace maidsafe {

namespace routing {

namespace protobuf { class Contact; }

struct NodeInfo {
  NodeInfo();
  NodeId node_id;
  asymm::PublicKey public_key;
  int32_t rank;
  int32_t bucket;
  boost::asio::ip::udp::endpoint endpoint;
  int32_t dimension_1;
  int32_t dimension_2;
  int32_t dimension_3;
  int32_t dimension_4;
  static const int32_t kInvalidBucket;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_NODE_INFO_H_
