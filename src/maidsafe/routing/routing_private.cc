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

#include "maidsafe/routing/routing_private.h"

#include "maidsafe/routing/message_handler.h"


namespace maidsafe {

namespace routing {

RoutingPrivate::RoutingPrivate(const asymm::Keys& keys, bool client_mode)
    : asio_service_(1),
      bootstrap_nodes_(),
      keys_(keys),
      tearing_down_(false),
      routing_table_(keys_, client_mode),
      non_routing_table_(keys_),  // TODO(Prakash) : don't create NRT for client nodes (wrap both)
      timer_(asio_service_),
      waiting_for_response_(),
      message_handler_(),
      network_(routing_table_, non_routing_table_),
      joined_(false),
      bootstrap_file_path_(),
      client_mode_(client_mode),
      anonymous_node_(false),
      functors_(),
      random_node_queue_(),
      recovery_timer_(asio_service_.service()) {
  message_handler_.reset(new MessageHandler(asio_service_, routing_table_, non_routing_table_,
                                            network_, timer_));
  asio_service_.Start();
}

RoutingPrivate::~RoutingPrivate() {
  recovery_timer_.cancel();
  tearing_down_ = true;
  boost::this_thread::disable_interruption disable_interruption;
  asio_service_.Stop();
  network_.Stop();
}

}  // namespace routing

}  // namespace maidsafe
