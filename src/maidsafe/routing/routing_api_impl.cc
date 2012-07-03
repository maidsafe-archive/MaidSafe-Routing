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
    : asio_service_(new AsioService(2)), //Concurrency())),
      bootstrap_nodes_(),
      keys_(keys),
      functors_(),
      rudp_(),
      routing_table_(keys_, CloseNodeReplacedFunctor()),
      timer_(*asio_service_),
      waiting_for_response_(),
      direct_non_routing_table_connections_(),
      message_handler_(asio_service_, routing_table_, rudp_, timer_, MessageReceivedFunctor(),
                       RequestPublicKeyFunctor()),
      joined_(false),
      bootstrap_file_path_(),
      client_mode_(client_mode),
      anonymous_node_(false) {
  asio_service_->Start();
}

RoutingPrivate::~RoutingPrivate() {}

}  // namespace routing

}  // namespace maidsafe
