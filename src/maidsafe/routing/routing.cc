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
#ifdef __MSVC__
#  pragma warning(push)
#  pragma warning(disable: 4127 4244 4267)
#endif
#include "maidsafe/routing/routing.pb.h"
#ifdef __MSVC__
#  pragma warning(pop)
#endif
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/routing_impl.h"

namespace fs = boost::filesystem;
namespace bs2 = boost::signals2;


namespace maidsafe {

namespace routing {

const unsigned int Parameters::kClosestNodesSize(8);
const unsigned int Parameters::kMaxRoutingTableSize(64);
const unsigned int Parameters::kBucketTargetSize(1);
const unsigned int Parameters::kNumChunksToCache(100);

Message::Message()
    : source_id(),
      destination_id(),
      target_name(),
      cacheable(false),
      data(),
      direct(false),
      response(false),
      replication(0),
      type(0),
      routing_failure(false) {}

Message::Message(const protobuf::Message &protobuf_message)
    : source_id(protobuf_message.source_id()),
      destination_id(protobuf_message.destination_id()),
      target_name(protobuf_message.has_target_name() ?
                protobuf_message.target_name() : ""),
      cacheable(protobuf_message.has_cacheable() ?
                protobuf_message.cacheable() : false),
      data(protobuf_message.data()),
      direct(protobuf_message.has_direct() ?
             protobuf_message.direct() : false),
      response(protobuf_message.response()),
      replication(protobuf_message.replication()),
      type(protobuf_message.type()),
      routing_failure(protobuf_message.has_routing_failure() ?
                      protobuf_message.routing_failure() : false) {}


Routing::Routing(NodeType node_type, const fs::path &config_file)
    : pimpl_(new RoutingImpl(node_type, config_file)) {}

Routing::Routing(NodeType node_type,
                 const fs::path &config_file,
                 const asymm::PrivateKey &private_key,
                 const std::string &node_id)
    : pimpl_(new RoutingImpl(node_type, config_file, private_key, node_id)) {}

void Routing::BootStrapFromThisEndpoint(const transport::Endpoint &endpoint) {
  pimpl_->BootStrapFromThisEndpoint(endpoint);
}

void Routing::Send(const Message &message,
                   const ResponseReceivedFunctor &response_functor) {
  pimpl_->Send(message, response_functor);
}

bs2::signal<void(int, Message)> &Routing::RequestReceivedSignal() {
  return pimpl_->message_received_signal_;
}

bs2::signal<void(int16_t)> &Routing::NetworkStatusSignal() {
  return pimpl_->network_status_signal_;
}

}  // namespace routing

}  // namespace maidsafe
