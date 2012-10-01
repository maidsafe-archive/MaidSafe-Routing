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

#include "maidsafe/common/log.h"
#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/routing_pb.h"

namespace maidsafe {

namespace routing {

RoutingPrivate::RoutingPrivate(const asymm::Keys& keys, bool client_mode)
    : asio_service_(2),
      bootstrap_nodes_(),
      keys_([&keys]()->asymm::Keys {
          if (!keys.identity.empty())
            return keys;
          asymm::Keys keys_temp;
          asymm::GenerateKeyPair(&keys_temp);
          keys_temp.identity = NodeId(NodeId::kRandomId).String();
        return keys_temp;
      }()),
      kNodeId_(NodeId(keys_.identity)),
      tearing_down_(false),
      routing_table_(keys_, client_mode),
      non_routing_table_(keys_),  // TODO(Prakash) : don't create NRT for client nodes (wrap both)
      timer_(asio_service_),
      waiting_for_response_(),
      message_handler_(),
      network_(routing_table_, non_routing_table_, timer_),
      joined_(false),
      bootstrap_file_path_(),
      client_mode_(client_mode),
      anonymous_node_(false),
      functors_(),
      random_node_queue_(),
      recovery_timer_(asio_service_.service()),
      setup_timer_(asio_service_.service()),
      random_node_vector_(),
      random_node_mutex_() {
  message_handler_.reset(new MessageHandler(asio_service_, routing_table_, non_routing_table_,
                                            network_, timer_));
  asio_service_.Start();
}

RoutingPrivate::~RoutingPrivate() {
  LOG(kVerbose) << "RoutingPrivate::~RoutingPrivate() " << DebugId(kNodeId_);
  tearing_down_ = true;
  setup_timer_.cancel();
  recovery_timer_.cancel();
  network_.Stop();
  boost::this_thread::disable_interruption disable_interruption;
  asio_service_.Stop();
}

}  // namespace routing

}  // namespace maidsafe
