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

#include "maidsafe/routing/acknowledgement.h"

#include <algorithm>

#include "maidsafe/common/asio_service.h"
#include "boost/date_time.hpp"
#include "maidsafe/common/log.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/network.h"
#include "maidsafe/routing/routing.pb.h"

namespace maidsafe {

namespace routing {

Acknowledgement::Acknowledgement(const NodeId& local_node_id, AsioService& io_service)
    : kNodeId_(local_node_id), ack_id_(RandomInt32()), mutex_(), stop_handling_(false),
      io_service_(io_service), queue_() {}

Acknowledgement::~Acknowledgement() {
  stop_handling_ = true;
  RemoveAll();
}

void Acknowledgement::RemoveAll() {
  std::vector<AckId> ack_ids;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& timer : queue_) {
      ack_ids.push_back(timer.ack_id);
    }
  }
  for (const auto& ack_id : ack_ids) {
    Remove(ack_id);
  }
}

AckId Acknowledgement::GetId() {
  std::lock_guard<std::mutex> lock(mutex_);
  return ++ack_id_;
}

void Acknowledgement::Add(const protobuf::Message& message, Handler handler, int timeout) {
  std::lock_guard<std::mutex> lock(mutex_);
  assert(message.has_ack_id() && "non-existing ack id");
  assert((message.ack_id() != 0) && "invalid ack id");

  AckId ack_id(message.ack_id());

  const auto it(std::find_if(std::begin(queue_), std::end(queue_),
                             [ack_id](const AckTimer& timer) {
                               return ack_id == timer.ack_id;
                             }));
  if (it == std::end(queue_)) {
    TimerPointer timer(new asio::deadline_timer(io_service_.service(),
                                                boost::posix_time::seconds(timeout)));
    timer->async_wait(handler);
    queue_.emplace_back(AckTimer(ack_id, message, timer, 0));
  } else {
    it->quantity++;
    it->timer->expires_from_now(boost::posix_time::seconds(timeout));
    if (it->quantity == Parameters::max_send_retry) {
      it->timer->async_wait([=](const boost::system::error_code&) {
                              Remove(ack_id);
                            });
     } else {
       it->timer->async_wait(handler);
     }
  }
}

void Acknowledgement::Remove(AckId ack_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto const it(std::find_if(std::begin(queue_), std::end(queue_),
                             [ack_id] (const AckTimer& timer)->bool {
                               return ack_id == timer.ack_id;
                             }));
  if (it != std::end(queue_)) {
    it->timer->cancel();
    queue_.erase(it);
  }
}

void Acknowledgement::HandleMessage(AckId ack_id) {
  assert((ack_id != 0) && "Invalid acknowledgement id");
  Remove(ack_id);
}

bool Acknowledgement::IsSendingAckRequired(const protobuf::Message& message,
                                           const NodeId& this_node_id) {
  if (message.ack_id() == 0)
    return false;
  return (message.destination_id() == this_node_id.string()) &&
         (message.destination_id() != message.relay_id());
}

bool Acknowledgement::NeedsAck(const protobuf::Message& message, const NodeId& /*node_id*/) {
  if (message.ack_id() == 0)
    return false;

// Ack messages do not need an ack
  if (IsAck(message))
    return false;

  if (message.source_id() == message.destination_id())
    return false;

  if (IsConnectSuccessAcknowledgement(message))
    return false;

//  communication between two nodes, in which one side is a relay at neither end
//  involves setting a timer.
  if (IsResponse(message) && (message.destination_id() == message.relay_id()))
    return false;

  if (message.source_id().empty())
    return false;
  return true;
}

void Acknowledgement::AdjustAckHistory(protobuf::Message& message) {
  if (message.relay_id() == kNodeId_.string())
    return;
  assert((message.ack_node_ids_size() <= 2) && "size of ack list must be smaller than 3");
  if ((message.ack_node_ids_size() == 0) ||
      ((message.ack_node_ids_size() == 1) &&
       (NodeId(message.ack_node_ids(0)) != kNodeId_)))  {
    message.add_ack_node_ids(kNodeId_.string());
  } else if (message.ack_node_ids_size() == 2) {
    std::string last_node(message.ack_node_ids(1));
    message.clear_ack_node_ids();
    message.add_ack_node_ids(last_node);
    message.add_ack_node_ids(kNodeId_.string());
  }
}

}  // namespace routing

}  // namespace maidsafe
