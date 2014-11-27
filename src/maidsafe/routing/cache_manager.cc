/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#include <future>

#include "maidsafe/routing/cache_manager.h"

#include "maidsafe/routing/network.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/utils.h"


namespace maidsafe {

namespace routing {

CacheManager::CacheManager(const NodeId& node_id, Network& network)
    : kNodeId_(node_id),
      network_(network),
      message_and_caching_functors_(),
      typed_message_and_caching_functors_() {}

void CacheManager::InitialiseFunctors(const MessageAndCachingFunctors&
                                      message_and_caching_functors) {
#ifndef TESTING
  assert(message_and_caching_functors.message_received);
  assert(message_and_caching_functors.have_cache_data);
  assert(message_and_caching_functors.store_cache_data);
#endif
  message_and_caching_functors_ = message_and_caching_functors;
}

void CacheManager::InitialiseFunctors(const TypedMessageAndCachingFunctor&
    typed_message_and_caching_functors) {
  assert(!message_and_caching_functors_.message_received);
  assert(!message_and_caching_functors_.have_cache_data);
  assert(!message_and_caching_functors_.store_cache_data);
  typed_message_and_caching_functors_ = typed_message_and_caching_functors;
}

void CacheManager::AddToCache(const protobuf::Message& message) {
//  assert(!message.request());
  if (message_and_caching_functors_.store_cache_data) {
    message_and_caching_functors_.store_cache_data(message.data(0));
  } else {
    TypedMessageAddtoCache(message);
  }
}

void CacheManager::TypedMessageAddtoCache(const protobuf::Message& message) {
  assert(!(message.has_relay_id() || message.has_relay_connection_id()));
  if ((!message.has_group_source() && !message.has_group_destination()) &&
          typed_message_and_caching_functors_.single_to_single.put_cache_data) {
    typed_message_and_caching_functors_.single_to_single.put_cache_data(
        CreateSingleToSingleMessage(message));
  } else if ((!message.has_group_source() && message.has_group_destination()) &&
                typed_message_and_caching_functors_.single_to_group.put_cache_data) {
    typed_message_and_caching_functors_.single_to_group.put_cache_data(
        CreateSingleToGroupMessage(message));
  } else if ((message.has_group_source() && !message.has_group_destination()) &&
                typed_message_and_caching_functors_.group_to_single.put_cache_data) {
    typed_message_and_caching_functors_.group_to_single.put_cache_data(
        CreateGroupToSingleMessage(message));
  } else if ((message.has_group_source() && message.has_group_destination()) &&
                typed_message_and_caching_functors_.group_to_group.put_cache_data) {
    typed_message_and_caching_functors_.group_to_group.put_cache_data(
        CreateGroupToGroupMessage(message));
  }
}

// TODO(Mahmoud): In the current implementation, typed and untyped messages are handled slighlty
// differently, which needs to become the same after discussions.
// 1) Typed message cache handling is blocking
// 2) Untyped message cache handling is blocking, however, there is a deadline of
//    Parameter::local_retreival_timeout seconds for a reply. If the reply is not received at this
//    deadline the function returns "false".
// The advantages of the second approach (the one for untyped messages) are:
//     a) not allowing a slow node to slows down the flow
//     b) to customise the timeout in such a way to avoid the potential conflicts with acks.
bool CacheManager::HandleGetFromCache(protobuf::Message& message) {
  assert(IsRequest(message));
  assert(IsCacheableGet(message));
  auto cache_hit(std::make_shared<std::promise<bool>>());
  auto future(cache_hit->get_future());
  if (message_and_caching_functors_.have_cache_data) {
    if (IsRequest(message)) {
      ReplyFunctor response_functor = [=](const std::string& reply_message) {
          if (reply_message.empty()) {
            cache_hit->set_value(false);
            return;
          }

          //  Responding with cached response
          protobuf::Message message_out;
          message_out.set_request(false);
          message_out.set_ack_id(RandomInt32());
          message_out.set_hops_to_live(Parameters::hops_to_live);
          message_out.set_destination_id(message.source_id());
          message_out.set_type(message.type());
          message_out.set_direct(true);
          message_out.clear_data();
          message_out.set_client_node(message.client_node());
          message_out.set_routing_message(message.routing_message());
          message_out.add_data(reply_message);
          message_out.set_last_id(kNodeId_.string());
          message_out.set_source_id(kNodeId_.string());
          if (message.has_cacheable())
            message_out.set_cacheable(static_cast<int32_t>(Cacheable::kPut));
          if (message.has_id())
            message_out.set_id(message.id());
          else
            LOG(kWarning) << "Message to be sent back had no ID.";

          if (message.has_relay_id())
            message_out.set_relay_id(message.relay_id());

          if (message.has_relay_connection_id()) {
            message_out.set_relay_connection_id(message.relay_connection_id());
          }
          network_.SendToClosestNode(message_out);
          cache_hit->set_value(true);
      };

      if (message_and_caching_functors_.have_cache_data)
        message_and_caching_functors_.have_cache_data(message.data(0), response_functor);
    }
  } else {
    return TypedMessageHandleGetFromCache(message);
  }
  if (future.wait_for(Parameters::local_retreival_timeout) != std::future_status::ready)
    return false;
  return future.get();
}

bool CacheManager::TypedMessageHandleGetFromCache(protobuf::Message& message) {
  assert(!(message.has_relay_id() || message.has_relay_connection_id()));
  if ((!message.has_group_source() && !message.has_group_destination()) &&
      typed_message_and_caching_functors_.single_to_single.get_cache_data) {
    return typed_message_and_caching_functors_.single_to_single.get_cache_data(
        CreateSingleToSingleMessage(message));
  } else if ((!message.has_group_source() && message.has_group_destination()) &&
             typed_message_and_caching_functors_.single_to_group.get_cache_data) {
    return typed_message_and_caching_functors_.single_to_group.get_cache_data(
        CreateSingleToGroupMessage(message));
  } else if ((message.has_group_source() && !message.has_group_destination()) &&
             typed_message_and_caching_functors_.group_to_single.get_cache_data) {
    return typed_message_and_caching_functors_.group_to_single.get_cache_data(
        CreateGroupToSingleMessage(message));
  } else if ((message.has_group_source() && message.has_group_destination()) &&
             typed_message_and_caching_functors_.group_to_group.get_cache_data) {
    return typed_message_and_caching_functors_.group_to_group.get_cache_data(
        CreateGroupToGroupMessage(message));
  }
  return false;
}

}  // namespace routing

}  // namespace maidsafe
