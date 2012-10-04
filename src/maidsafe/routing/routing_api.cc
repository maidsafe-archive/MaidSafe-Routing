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

#include "maidsafe/routing/routing_api.h"

#include <functional>
#include <future>
#include "boost/asio/deadline_timer.hpp"
#include "boost/thread/future.hpp"

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"

#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/rudp/return_codes.h"

#include "maidsafe/routing/bootstrap_file_handler.h"
#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_private.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/timer.h"
#include "maidsafe/routing/utils.h"


namespace args = std::placeholders;
namespace fs = boost::filesystem;

namespace maidsafe {

namespace routing {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // unnamed namespace

Routing::Routing(const asymm::Keys& keys, const bool& client_mode)
    : impl_(new RoutingPrivate(keys, client_mode)) {
}

Routing::~Routing() {
//  impl_.Stop()
}

void Routing::Join(Functors functors, std::vector<Endpoint> peer_endpoints) {
//  if (impl_)
//    impl_->Join(functors, peer_endpoints);
}

void Routing::DisconnectFunctors() {  // TODO(Prakash) : fix race condition when functors in use
//  if (impl_)
//    impl_->DisconnectFunctors();
}

int Routing::ZeroStateJoin(Functors functors,
                           const Endpoint& local_endpoint,
                           const Endpoint& peer_endpoint,
                           const NodeInfo& peer_node) {
//  if (impl_)
//    return impl_->ZeroStateJoin(functors, local_endpoint, peer_endpoint, peer_node);
//else
    return kGeneralError;
}

void Routing::Send(const NodeId& destination_id,
                   const NodeId& group_claim,
                   const std::string& data,
                   ResponseFunctor response_functor,
                   const boost::posix_time::time_duration& timeout,
                   bool direct,
                   bool cache) {
//  if (impl_)
//    impl_->Send(destination_id, group_claim, data, response_functor, timeout, direct, cache);
}

NodeId Routing::GetRandomExistingNode() {
//  if (impl_) {
//    return  impl_->GetRandomExistingNode();
//  } else {
//    assert(false);
    return NodeId();
//  }
}

}  // namespace routing

}  // namespace maidsafe
