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

#ifdef LOCAL_TEST
std::vector<boost::asio::ip::udp::endpoint> RoutingPrivate::bootstraps_;
std::mutex RoutingPrivate::mutex_;
#endif

RoutingPrivate::RoutingPrivate(const asymm::Keys& keys, bool client_mode)
    : asio_service_(1),
      bootstrap_nodes_(),
      keys_([&keys]()->asymm::Keys {
          if (!keys.identity.empty())
            return keys;
          asymm::Keys keys_temp;
          asymm::GenerateKeyPair(&keys_temp);
          keys_temp.identity = NodeId(NodeId::kRandomId).String();
        return keys_temp;
      }()),
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

void RoutingPrivate::LocalTestUtility(const protobuf::Message message,
                                      uint16_t& expected_connect_response) {
  protobuf::FindNodesResponse findnodes_response;
  if ((message.data_size() > 0) &&
      (findnodes_response.ParseFromString(message.data(0)))) {
    expected_connect_response = findnodes_response.nodes_size();
    LOG(kVerbose) << "expected_connect_response: " << expected_connect_response;
    return;
  }
  protobuf::ConnectResponse connect_response;
  std::lock_guard<std::mutex>  lock(RoutingPrivate::mutex_);
  if ((message.data_size() > 0) &&
      connect_response.ParseFromString(message.data(0)) &&
      (routing_table_.Size() >= expected_connect_response - 1)) {
    Sleep(boost::posix_time::millisec(100));
    rudp::EndpointPair endpoint_pair;
    rudp::NatType nat_type;
    network_.GetAvailableEndpoint(routing_table_.nodes_[0].endpoint,
                                  endpoint_pair,
                                  nat_type);
    RoutingPrivate::bootstraps_.push_back(endpoint_pair.local);
    std::random_shuffle(RoutingPrivate::bootstraps_.begin(), RoutingPrivate::bootstraps_.end());
    for (auto endpoint : RoutingPrivate::bootstraps_)
      LOG(kVerbose) << "bootstrap port: " << endpoint.port();
  }
}

}  // namespace routing

}  // namespace maidsafe
