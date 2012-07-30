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

#include "maidsafe/routing/routing_api_impl.h"

#include "maidsafe/routing/bootstrap_file_handler.h"
#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/timer.h"
#include "maidsafe/routing/utils.h"

namespace fs = boost::filesystem;

namespace maidsafe {

namespace routing {

RoutingPrivate::RoutingPrivate(const asymm::Keys &keys,
                               bool client_mode)
    : asio_service_(2),
      bootstrap_nodes_(),
      keys_(keys),
      tearing_down_(false),
      routing_table_(keys_, client_mode, CloseNodeReplacedFunctor()),
      non_routing_table_(keys_),  // TODO(Prakash) : don't create NRT for client nodes (wrap both)
      timer_(asio_service_),
      waiting_for_response_(),
      network_(routing_table_, non_routing_table_),
      message_handler_(asio_service_, routing_table_, non_routing_table_,
                       network_, timer_),
      joined_(false),
      bootstrap_file_path_(),
      client_mode_(client_mode),
      anonymous_node_(false),
      functors_() {
  asio_service_.Start();
}

RoutingPrivate::~RoutingPrivate() {
  asio_service_.Stop();
  network_.Stop();
  message_handler_.set_tearing_down();
  tearing_down_ = true;
}

}  // namespace routing

}  // namespace maidsafe
