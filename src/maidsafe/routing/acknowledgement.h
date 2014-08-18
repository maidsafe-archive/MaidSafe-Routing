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


#ifndef MAIDSAFE_ROUTING_ACKNOWLEDGEMENT_H_
#define MAIDSAFE_ROUTING_ACKNOWLEDGEMENT_H_

#include<mutex>
#include <map>
#include <string>
#include <utility>
#include <tuple>
#include <vector>

#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/asio.hpp"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/client_routing_table.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/common/asio_service.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/routing/utils.h"

namespace asio = boost::asio;

namespace maidsafe {

namespace routing {

typedef uint32_t AckId;

namespace protobuf { class Message;}  // namespace protobuf
namespace test {
  class GenericNode;
}

typedef std::shared_ptr<asio::deadline_timer> TimerPointer;
typedef std::function<void(const boost::system::error_code& error)> Handler;

enum class GroupMessageAckStatus {
  kPending = 0,
  kSuccess = 1,
  kFailure = 2
};

struct AckTimer {
  AckTimer(AckId ack_id_in, const protobuf::Message& message_in, TimerPointer timer_in,
           unsigned int quantity_in) : ack_id(ack_id_in), message(message_in), timer(timer_in),
                                       quantity(quantity_in) {}
  AckId ack_id;
  protobuf::Message message;
  TimerPointer timer;
  unsigned int quantity;
};

struct GroupAckTimer {
  GroupAckTimer(AckId ack_id_in, const protobuf::Message& message_in, TimerPointer timer_in,
                const std::map<NodeId, GroupMessageAckStatus> requested_peers_in)
      : ack_id(ack_id_in), message(message_in), timer(timer_in),
        requested_peers(requested_peers_in) {}
  AckId ack_id;
  protobuf::Message message;
  TimerPointer timer;
  std::map<NodeId, GroupMessageAckStatus> requested_peers;
};

class Acknowledgement {
 public:
  explicit Acknowledgement(const NodeId& local_node_id, AsioService& io_service);
  ~Acknowledgement();
  AckId GetId();
  void Add(const protobuf::Message& message, Handler handler, int timeout);
  void AddGroup(const protobuf::Message& message, Handler handler, int timeout);
  void Remove(const AckId& ack_id);
  void GroupQueueRemove(AckId ack_id);
  void HandleMessage(AckId ack_id);
  bool HandleGroupMessage(const protobuf::Message& message);
  bool NeedsAck(const protobuf::Message& message, const NodeId& node_id);
  bool IsSendingAckRequired(const protobuf::Message& message, const NodeId& local_node_id);
  NodeId AppendGroup(AckId ack_id, std::vector<std::string>& exclusion);
  void SetAsFailedPeer(AckId ack_id, const NodeId& node_id);
  void AdjustAckHistory(protobuf::Message& message);

  friend class test::GenericNode;

 private:
  Acknowledgement& operator=(const Acknowledgement&);
  Acknowledgement(const Acknowledgement&);
  Acknowledgement(const Acknowledgement&&);
  void RemoveAll();

  bool running_;
  const NodeId kNodeId_;
  AckId ack_id_;
  std::mutex mutex_;
  AsioService& io_service_;
  std::vector<AckTimer> queue_;
  std::vector<GroupAckTimer> group_queue_;
};

}  // namespace routing

}  // namespace maidsafe


#endif  // MAIDSAFE_ROUTING_ACKNOWLEDGEMENT_H_

