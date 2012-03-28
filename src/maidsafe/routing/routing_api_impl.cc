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
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/timer.h"
#include "maidsafe/routing/bootstrap_file_handler.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/utils.h"
#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/parameters.h"

namespace maidsafe {

namespace routing {

RoutingPrivate::RoutingPrivate(const NodeValidationFunctor &node_valid_functor,
                 const asymm::Keys &keys, bool client_mode)
    : asio_service_(),
      bootstrap_nodes_(),
      keys_(keys),
      node_local_endpoint_(),
      node_external_endpoint_(),
      transport_(),
      routing_table_(keys_),
      timer_(asio_service_),
      message_handler_(node_valid_functor, routing_table_, transport_, timer_),
      message_received_signal_(),
      network_status_signal_(),
      close_node_from_to_signal_(),
      waiting_for_response_(),
      client_connections_(),
      client_routing_table_(),
      joined_(false),
      node_validation_functor_(node_valid_functor),
      client_mode_(client_mode) {}

}  // namespace routing

}  // namespace maidsafe
