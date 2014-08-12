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

#include "maidsafe/routing/network.h"


namespace bptime = boost::posix_time;

namespace maidsafe {

namespace routing {

template <>
void Network<ClientNode>::RecursiveSendOn(protobuf::Message message,
                                               NodeInfo last_node_attempted, int attempt_count) {
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
  }
  if (attempt_count >= 3) {
    LOG(kWarning) << " Retry attempts failed to send to ["
                  << HexSubstr(last_node_attempted.id.string())
                  << "] will drop this node now and try with another node."
                  << " id: " << message.id();
    attempt_count = 0;
    {
      std::lock_guard<std::mutex> lock(running_mutex_);
      if (!running_)
        return;
      rudp_.Remove(last_node_attempted.connection_id);
      LOG(kWarning) << " Routing -> removing connection " << last_node_attempted.id.string();
      // FIXME Should we remove this node or let rudp handle that?
      connections_.routing_table.DropNode(last_node_attempted.connection_id, false);
    }
  }

  if (attempt_count > 0)
    Sleep(std::chrono::milliseconds(50));

  const std::string kThisId(connections_.kNodeId().data.string());
  bool ignore_exact_match(!IsDirect(message));
  std::vector<std::string> route_history;
  NodeInfo peer;
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!running_)
      return;
    if (message.route_history().size() > 1)
      route_history = std::vector<std::string>(
          message.route_history().begin(),
          message.route_history().end() -
              static_cast<size_t>(!(message.has_visited() && message.visited())));
    else if ((message.route_history().size() == 1) &&
             (message.route_history(0) != connections_.kNodeId().data.string()))
      route_history.push_back(message.route_history(0));

    peer = connections_.routing_table.GetClosestNode(NodeId(message.destination_id()),
                                                     ignore_exact_match, route_history);
    if (peer.id == NodeId() && connections_.routing_table.size() != 0) {
      peer = connections_.routing_table.GetClosestNode(NodeId(message.destination_id()),
                                                       ignore_exact_match);
    }
    if (peer.id == NodeId()) {
      LOG(kError) << "This node's routing table is empty now.  Need to re-bootstrap.";
      return;
    }
    AdjustRouteHistory(message);
  }

  rudp::MessageSentFunctor message_sent_functor = [=](int message_sent) {
    {
      std::lock_guard<std::mutex> lock(running_mutex_);
      if (!running_)
        return;
    }
    if (rudp::kSuccess == message_sent) {
      LOG(kVerbose) << "  [" << HexSubstr(kThisId) << "] sent : " << MessageTypeString(message)
                    << " to   " << HexSubstr(peer.id.string()) << "   (id: " << message.id() << ")"
                    << " dst : " << HexSubstr(message.destination_id());
    } else if (rudp::kSendFailure == message_sent) {
      LOG(kError) << "Sending type " << MessageTypeString(message) << " message from "
                  << HexSubstr(connections_.kNodeId().data.string()) << " to "
                  << HexSubstr(peer.id.string()) << " with destination ID "
                  << HexSubstr(message.destination_id()) << " failed with code " << message_sent
                  << ".  Will retry to Send.  Attempt count = " << attempt_count + 1
                  << " id: " << message.id();
      RecursiveSendOn(message, peer, attempt_count + 1);
    } else {
      LOG(kError) << "Sending type " << MessageTypeString(message) << " message from "
                  << HexSubstr(kThisId) << " to " << HexSubstr(peer.id.string())
                  << " with destination ID " << HexSubstr(message.destination_id())
                  << " failed with code " << message_sent << "  Will remove node."
                  << " message id: " << message.id();
      {
        std::lock_guard<std::mutex> lock(running_mutex_);
        if (!running_)
          return;
        rudp_.Remove(last_node_attempted.connection_id);
      }
      LOG(kWarning) << " Routing-> removing connection " << DebugId(peer.connection_id);
      connections_.routing_table.DropNode(peer.id, false);
      RecursiveSendOn(message);
    }
  };
  LOG(kVerbose) << "Rudp recursive send message to " << DebugId(peer.connection_id);
  RudpSend(peer.connection_id, message, message_sent_functor);
}

template <>
void Network<ClientNode>::SendToClosestNode(const protobuf::Message& message) {
  // Normal messages
  if (message.has_destination_id() && !message.destination_id().empty()) {
    if (connections_.routing_table.size() > 0) {  // getting closer nodes from routing table
      RecursiveSendOn(message);
    } else {
      LOG(kError) << " No endpoint to send to; aborting send.  Attempt to send a type "
                  << MessageTypeString(message) << " message to " << HexSubstr(message.source_id())
                  << " from " << connections_.kNodeId().data << " id: " << message.id();
    }
    return;
  }

  // Relay message responses only
  if (message.has_relay_id() /*&& (IsResponse(message))*/) {
    protobuf::Message relay_message(message);
    relay_message.set_destination_id(message.relay_id());  // so that peer identifies it as direct
    SendTo(relay_message, NodeId(relay_message.relay_id()),
           NodeId(relay_message.relay_connection_id()));
  } else {
    LOG(kError) << "Unable to work out destination; aborting send."
                << " id: " << message.id() << " message.has_relay_id() ; " << std::boolalpha
                << message.has_relay_id() << " Isresponse(message) : " << std::boolalpha
                << IsResponse(message) << " message.has_relay_connection_id() : " << std::boolalpha
                << message.has_relay_connection_id();
  }
}

}  // namespace routing

}  // namespace maidsafe
