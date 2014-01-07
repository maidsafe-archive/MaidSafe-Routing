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

struct AckTimer {
  AckTimer(AckId ack_id_in, const protobuf::Message& message_in, TimerPointer timer_in,
           uint16_t quantity_in) : ack_id(ack_id_in), message(message_in), timer(timer_in),
                                   quantity(quantity_in) {}
  AckId ack_id;
  protobuf::Message message;
  TimerPointer timer;
  uint16_t quantity;
};

struct GroupAckTimer {
  GroupAckTimer(AckId ack_id_in, const protobuf::Message& message_in, TimerPointer timer_in,
                const std::map<NodeId, int> requested_peers_in)
      : ack_id(ack_id_in), message(message_in), timer(timer_in),
        requested_peers(requested_peers_in) {}
  AckId ack_id;
  protobuf::Message message;
  TimerPointer timer;
  std::map<NodeId, int> requested_peers;
};

class RoutingPrivate;

class Acknowledgement {
 public:
  explicit Acknowledgement(AsioService& io_service);
  ~Acknowledgement();
  AckId GetId();
  void Add(const protobuf::Message& message, Handler handler, int timeout);
  void AddGroup(const protobuf::Message& message, Handler handler, int timeout);
  void Remove(const AckId& ack_id);
  void GroupQueueRemove(const AckId& ack_id);
  void HandleMessage(int32_t ack_id);
  bool HandleGroupMessage(const protobuf::Message& message);
  bool NeedsAck(const protobuf::Message& message, const NodeId& node_id);
  bool IsSendingAckRequired(const protobuf::Message& message, const NodeId& this_node_id);
  NodeId AppendGroup(AckId ack_id, std::vector<std::string>& exclusion);
  void SetAsFailedPeer(AckId ack_id, const NodeId& node_id);

  friend class RoutingPrivate;
  friend class test::GenericNode;

 private:
  Acknowledgement& operator=(const Acknowledgement&);
  Acknowledgement(const Acknowledgement&);
  Acknowledgement(const Acknowledgement&&);
  void RemoveAll();

  bool running_;
  AckId ack_id_;
  std::mutex mutex_;
  std::vector<AckTimer> queue_;
  std::vector<GroupAckTimer> group_queue_;
  AsioService& io_service_;
};

}  // namespace routing

}  // namespace maidsafe


#endif  // MAIDSAFE_ROUTING_ACKNOWLEDGEMENT_H_

