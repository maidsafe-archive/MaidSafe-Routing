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

#include "maidsafe/routing/acknowledgement.h"

#include "maidsafe/common/asio_service.h"
#include "boost/date_time.hpp"
#include "maidsafe/common/log.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/routing.pb.h"

namespace maidsafe {

namespace routing {

namespace {

enum class GroupMessageAckStatus {
  kPending = 0,
  kSuccess = 1,
  kFailure = 2
};

}  // no-name namespace

Acknowledgement::Acknowledgement(AsioService& io_service)
    : running_(true), ack_id_(RandomUint32()), mutex_(), queue_(), group_queue_(),
      io_service_(io_service)  {}

Acknowledgement::~Acknowledgement() {
  running_ = false;
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
  LOG(kVerbose) << "Size of list: " << ack_ids.size();
  for (const auto& ack_id : ack_ids) {
    LOG(kVerbose) << "still in list: " << ack_id;
//    Remove(ack_id);
  }
}

AckId Acknowledgement::GetId() {
  std::lock_guard<std::mutex> lock(mutex_);
  return ++ack_id_;
}

void Acknowledgement::Add(const protobuf::Message& message, Handler handler, int timeout) {
  if (!running_)
    return;
  std::lock_guard<std::mutex> lock(mutex_);
  assert(message.has_ack_id() && "non-existing ack id");
  assert((message.ack_id() != 0) && "invalid ack id");

  AckId ack_id(message.ack_id());
  const auto group_itr(std::find_if(std::begin(group_queue_), std::end(group_queue_),
                       [ack_id](const GroupAckTimer& timer) {
                         return ack_id == timer.ack_id;
                       }));
  if (group_itr != std::end(group_queue_)) {
    group_itr->requested_peers.insert(
        std::make_pair(NodeId(message.destination_id()),
                       static_cast<int>(GroupMessageAckStatus::kPending)));
    return;
  }

  const auto it(std::find_if(std::begin(queue_), std::end(queue_),
                             [ack_id](const AckTimer& timer) {
                               return ack_id == timer.ack_id;
                             }));
  if (it == std::end(queue_)) {
    TimerPointer timer(new asio::deadline_timer(io_service_.service(),
                                                boost::posix_time::seconds(timeout)));
    timer->async_wait(handler);
    queue_.emplace_back(AckTimer(ack_id, message, timer, 0));
    LOG(kVerbose) << "AddAck added an ack, with id: " << ack_id;
  } else {
    LOG(kVerbose) << "Acknowledgement re-sends " << message.id();
    it->quantity++;
    it->timer->expires_from_now(boost::posix_time::seconds(timeout));
    if (it->quantity == Parameters::max_ack_attempts) {
      it->timer->async_wait([=](const boost::system::error_code&) {
                              Remove(ack_id);
                            });
     } else {
       it->timer->async_wait(handler);
     }
  }
}

void Acknowledgement::AddGroup(const protobuf::Message& message, Handler handler, int timeout) {
  if (!running_)
    return;
  std::lock_guard<std::mutex> lock(mutex_);
  assert(message.has_ack_id() && "non-existing ack id");
  assert((message.ack_id() != 0) && "invalid ack id");

  AckId ack_id(message.ack_id());
  auto const it(std::find_if(std::begin(group_queue_), std::end(group_queue_),
                             [ack_id](const GroupAckTimer& timer) {
                               return ack_id == timer.ack_id;
                             }));
  if (it == std::end(group_queue_)) {
    TimerPointer timer(new asio::deadline_timer(io_service_.service(),
                                                boost::posix_time::seconds(timeout)));
    timer->async_wait(handler);
    group_queue_.emplace_back(GroupAckTimer(ack_id, message, timer, std::map<NodeId, int>()));
    LOG(kVerbose) << "AddAck added a group ack, with id: " << ack_id;
  }
}

void Acknowledgement::Remove(const AckId& ack_id) {
  if (!running_)
    return;
  std::lock_guard<std::mutex> lock(mutex_);
  auto const it(std::find_if(std::begin(queue_), std::end(queue_),
                             [ack_id] (const AckTimer& timer)->bool {
                               return ack_id == timer.ack_id;
                             }));
  // assert((it != queue_.end()) && "attempt to cancel handler for non existant timer");
  if (it != std::end(queue_)) {
    // ack timed out or ack killed
    it->timer->cancel();
    queue_.erase(it);
    LOG(kVerbose) << "Clean up after ack with id: " << ack_id << " queue size: " << queue_.size();
  } else {
    LOG(kVerbose) << "Attempt to clean up a non existent ack with id" << ack_id
                  << " queue size: " << queue_.size();
  }
}

void Acknowledgement::HandleMessage(int32_t ack_id) {
  assert((ack_id != 0) && "Invalid acknowledgement id");
  LOG(kVerbose) << "MessageHandler::HandleAckMessage " << ack_id;
  Remove(ack_id);
}

bool Acknowledgement::HandleGroupMessage(const protobuf::Message& message) {
  AckId ack_id(message.ack_id());
  NodeId destination_id(message.destination_id());
  assert((ack_id != 0) && "Invalid acknowledgement id");
  LOG(kVerbose) << "MessageHandler::HandleAckMessage " << ack_id;

  std::lock_guard<std::mutex> lock(mutex_);
  auto const it(std::find_if(std::begin(group_queue_), std::end(group_queue_),
                             [ack_id](const GroupAckTimer& timer) {
                               return ack_id == timer.ack_id;
                             }));
  if (it == std::end(group_queue_))
    return false;

  if (std::count_if(it->requested_peers.begin(), it->requested_peers.end(),
                    [](const std::pair<NodeId, int>& member) {
                      return member.second == static_cast<int>(GroupMessageAckStatus::kSuccess);
                    }) == Parameters::group_size / 2) {
    it->timer->cancel();
    group_queue_.erase(it);
    return true;
  }

  auto group_itr(it->requested_peers.find(destination_id));

  if (group_itr != it->requested_peers.end())
    group_itr->second = static_cast<int>(GroupMessageAckStatus::kSuccess);
  return true;
}

bool Acknowledgement::IsSendingAckRequired(const protobuf::Message& message,
                                           const NodeId& this_node_id) {
  return (message.destination_id() == this_node_id.string()) &&
         (message.destination_id() != message.relay_id());
}

bool Acknowledgement::NeedsAck(const protobuf::Message& message, const NodeId& node_id) {
  LOG(kVerbose) << "node_id: " << HexSubstr(node_id.string()); 

// Ack messages do not need an ack
  if (IsAck(message))
    return false;

  if (message.source_id() == message.destination_id())
    return false;

  if (IsGroupUpdate(message))
    return false;

  if (IsConnectSuccessAcknowledgement(message))
    return false;

//  A communication between two nodes, in which one side is a relay at neither end
//  involves setting a timer.
  if (IsResponse(message) && (message.destination_id() == message.relay_id()))
    return false;

  if (message.source_id().empty())
    return false;

  LOG(kVerbose) << PrintMessage(message);
  return true;
}

}  // namespace routing

}  // namespace maidsafe
