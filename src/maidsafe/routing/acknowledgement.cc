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
    : kNodeId_(local_node_id), ack_id_(RandomUint32()), mutex_(),
      io_service_(io_service), queue_(), group_queue_() {}

Acknowledgement::~Acknowledgement() {
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
  const auto group_itr(std::find_if(std::begin(group_queue_), std::end(group_queue_),
                       [ack_id](const GroupAckTimer& timer) {
                         return ack_id == timer.ack_id;
                       }));
  if (group_itr != std::end(group_queue_)) {
    group_itr->requested_peers.insert(
        std::make_pair(NodeId(message.destination_id()), GroupMessageAckStatus::kPending));
    LOG(kVerbose) << "Add group entry " << NodeId(message.destination_id());
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
    if (it->quantity == Parameters::max_send_retry) {
      it->timer->async_wait([=](const boost::system::error_code&) {
                              Remove(ack_id);
                            });
     } else {
       it->timer->async_wait(handler);
     }
  }
}

void Acknowledgement::AddGroup(const protobuf::Message& message, Handler handler, int timeout) {
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
    group_queue_.emplace_back(GroupAckTimer(ack_id, message, timer, std::map<NodeId,
                                            GroupMessageAckStatus>()));
    LOG(kVerbose) << "AddAck added a group ack, with id: " << ack_id;
  }
}

void Acknowledgement::Remove(AckId ack_id) {
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

void Acknowledgement::GroupQueueRemove(AckId ack_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  group_queue_.erase(std::remove_if(std::begin(group_queue_), std::end(group_queue_),
                                    [ack_id](const GroupAckTimer& group_ack) {
                                      return group_ack.ack_id == ack_id;
                                    }), std::end(group_queue_));
}

void Acknowledgement::HandleMessage(AckId ack_id) {
  assert((ack_id != 0) && "Invalid acknowledgement id");
  LOG(kVerbose) << "MessageHandler::HandleAckMessage " << ack_id;
  Remove(ack_id);
}

bool Acknowledgement::HandleGroupMessage(const protobuf::Message& message) {
  AckId ack_id(message.ack_id());
  NodeId target_id((NodeId(message.destination_id()) == kNodeId_ &&
                   !message.source_id().empty())
                       ? NodeId(message.source_id())
                       : NodeId(message.destination_id()));
  assert((ack_id != 0) && "Invalid acknowledgement id");
  LOG(kVerbose) << "MessageHandler::HandleGroupMessage " << ack_id;

  std::lock_guard<std::mutex> lock(mutex_);
  auto const it(std::find_if(std::begin(group_queue_), std::end(group_queue_),
                             [ack_id](const GroupAckTimer& timer) {
                               return ack_id == timer.ack_id;
                             }));
  if (it == std::end(group_queue_))
    return false;

  auto group_itr(it->requested_peers.find(target_id));
  if (group_itr != it->requested_peers.end()) {
    group_itr->second = GroupMessageAckStatus::kSuccess;
    LOG(kVerbose) << "Ack group member succeeds " << group_itr->first << " id: " << ack_id;
  } else {
    LOG(kWarning) << "Handling ack for a non-existing peer: " << target_id
                  << " ack id: " << ack_id;
    return true;
  }

  auto expected(std::min(static_cast<unsigned int>(it->requested_peers.size()),
                         Parameters::group_size / 2));
  if (std::count_if(it->requested_peers.begin(), it->requested_peers.end(),
                    [](const std::pair<NodeId, GroupMessageAckStatus>& member) {
                      return member.second == GroupMessageAckStatus::kSuccess;
                    }) == expected) {
    LOG(kVerbose) << "HandleGroupMessage: expected meets: " << ack_id;
    it->timer->cancel();
    group_queue_.erase(it);
    return true;
  }

  return true;
}

NodeId Acknowledgement::AppendGroup(AckId ack_id, std::vector<std::string>& exclusion) {
  assert((ack_id != 0) && "Invalid acknowledgement id");
  LOG(kVerbose) << "MessageHandler::AppendGroup " << ack_id;

  std::lock_guard<std::mutex> lock(mutex_);
  auto const it(std::find_if(std::begin(group_queue_), std::end(group_queue_),
                             [ack_id](const GroupAckTimer& timer) {
                               return ack_id == timer.ack_id;
                             }));
  if (it == std::end(group_queue_)) {
    LOG(kVerbose) << "Not in group queue " << ack_id;
    return NodeId();
  }
  for (const auto&  peer : it->requested_peers) {
    if (std::find(std::begin(exclusion), std::end(exclusion),
                  peer.first.string()) == std::end(exclusion))
    exclusion.push_back(peer.first.string());
  }
  return NodeId(it->message.destination_id());
}

void Acknowledgement::SetAsFailedPeer(AckId ack_id, const NodeId& node_id) {
  assert((ack_id != 0) && "Invalid acknowledgement id");
  LOG(kVerbose) << "MessageHandler::SetAsFailedPeer " << ack_id;

  std::lock_guard<std::mutex> lock(mutex_);
  auto const it(std::find_if(std::begin(group_queue_), std::end(group_queue_),
                             [ack_id](const GroupAckTimer& timer) {
                               return ack_id == timer.ack_id;
                             }));
  if (it == std::end(group_queue_))
    return;
  auto member(it->requested_peers.find(node_id));
  if (member != it->requested_peers.end())
    member->second = GroupMessageAckStatus::kFailure;
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

  if (IsConnectSuccessAcknowledgement(message))
    return false;

//  communication between two nodes, in which one side is a relay at neither end
//  involves setting a timer.
  if (IsResponse(message) && (message.destination_id() == message.relay_id()))
    return false;

  if (message.source_id().empty())
    return false;

  LOG(kVerbose) << PrintMessage(message);
  return true;
}

void Acknowledgement::AdjustAckHistory(protobuf::Message& message) {
  LOG(kVerbose) << "size of acks "  << message.ack_node_ids_size();
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
