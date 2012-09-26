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

#ifdef LOCAL_TEST
#include "maidsafe/routing/utils.h"
#endif

#include "maidsafe/routing/routing_private.h"

#include "maidsafe/common/log.h"
#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/routing_pb.h"

namespace maidsafe {

namespace routing {

#ifdef LOCAL_TEST
std::vector<boost::asio::ip::udp::endpoint> RoutingPrivate::bootstraps_;
std::set<NodeId> RoutingPrivate::bootstrap_nodes_id_;
std::mutex RoutingPrivate::mutex_;
uint16_t RoutingPrivate::network_size_(0);
typedef boost::asio::ip::udp::endpoint Endpoint;
#endif

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
#ifdef LOCAL_TEST
  std::lock_guard<std::mutex> lock(RoutingPrivate::mutex_);
  ++RoutingPrivate::network_size_;
#endif
}

RoutingPrivate::~RoutingPrivate() {
  recovery_timer_.cancel();
  tearing_down_ = true;
  network_.Stop();
//  boost::this_thread::disable_interruption disable_interruption;
  asio_service_.Stop();
#ifdef LOCAL_TEST
  std::lock_guard<std::mutex> lock(RoutingPrivate::mutex_);
  if (--RoutingPrivate::network_size_ == 0) {
    RoutingPrivate::bootstraps_.clear();
    RoutingPrivate::bootstrap_nodes_id_.clear();
    LOG(kVerbose) << "static RoutingPrivate::bootstraps_ cleared.";
  }
#endif
}

#ifdef LOCAL_TEST
void RoutingPrivate::LocalTestUtility(const protobuf::Message message) {
  protobuf::ConnectResponse connect_response;
  std::lock_guard<std::mutex>  lock(RoutingPrivate::mutex_);
  if ((message.data_size() > 0) &&
      connect_response.ParseFromString(message.data(0)) &&
      (message.destination_id() == routing_table_.kKeys().identity) &&
      (std::find(RoutingPrivate::bootstrap_nodes_id_.begin(),
                 RoutingPrivate::bootstrap_nodes_id_.end(),
                 NodeId(keys_.identity)) == RoutingPrivate::bootstrap_nodes_id_.end())) {
    Endpoint endpoint;
    if (connect_response.answer()) {
        protobuf::ConnectRequest oirginal_request;
        oirginal_request.ParseFromString(connect_response.original_request());
        endpoint = GetEndpointFromProtobuf(oirginal_request.contact().private_endpoint());
        LOG(kVerbose) << "LocalTestUtility: endpoint: " << endpoint;
    } else {
      return;
    }

    if (std::find_if(RoutingPrivate::bootstraps_.begin(), RoutingPrivate::bootstraps_.end(),
                    [=](const Endpoint& ep) {
                      return endpoint == ep;
                    }) == RoutingPrivate::bootstraps_.end()) {
      RoutingPrivate::bootstraps_.push_back(endpoint);
    }
    for (auto endpoint : RoutingPrivate::bootstraps_)
      LOG(kVerbose) << "bootstrap port: " << endpoint.port();
  }
}

void RoutingPrivate::RemoveConnectionFromBootstrapList(
    const NodeId& /*node_id*/) {
//  std::remove_if(RoutingPrivate::bootstraps_.begin(),
//                 RoutingPrivate::bootstraps_.end(),
//                 [=](const boost::asio::ip::udp::endpoint& endpoint)->bool {
//                   bool result(lost_endpoint == endpoint);
//                   if (result) {
//                     LOG(kVerbose) << "Connection Removed from bootstrap list: " << lost_endpoint;
//                   }
//                   return result;
//                 });
}
#endif

}  // namespace routing

}  // namespace maidsafe
