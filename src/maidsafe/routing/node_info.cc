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

#include "maidsafe/routing/node_info.h"

#include <limits>


namespace maidsafe {

namespace routing {

NodeInfo::NodeInfo()
    : node_id(),
      public_key(),
      rank(),
      bucket(kInvalidBucket),
      endpoint(),
      nat_type(rudp::NatType::kUnknown),
      dimension_1(),
      dimension_2(),
      dimension_3(),
      dimension_4() {}

const int32_t NodeInfo::kInvalidBucket(std::numeric_limits<int32_t>::max());

}  // namespace routing

}  // namespace maidsafe
