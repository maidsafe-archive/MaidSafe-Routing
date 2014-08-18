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

#ifndef MAIDSAFE_ROUTING_RESPONSE_HANDLER_H_
#define MAIDSAFE_ROUTING_RESPONSE_HANDLER_H_

#include <mutex>
#include <string>
#include <vector>
#include <utility>
#include <deque>
#include <map>

#include "boost/asio/deadline_timer.hpp"
#include "boost/date_time/posix_time/ptime.hpp"

#include "maidsafe/common/rsa.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/api_config.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

namespace protobuf {
class Message;
}

namespace test {
class ResponseHandlerTest_BEH_ConnectAttempts_Test;
}  // namespace test

class Network;
class ClientRoutingTable;
class RoutingTable;
class GroupChangeHandler;

class ResponseHandler : public std::enable_shared_from_this<ResponseHandler> {
 public:
  ResponseHandler(RoutingTable& routing_table, ClientRoutingTable& client_routing_table,
                  Network& network, PublicKeyHolder& public_key_holder);
  virtual ~ResponseHandler();
  virtual void Ping(protobuf::Message& message);
  virtual void Connect(protobuf::Message& message);
  virtual void FindNodes(const protobuf::Message& message);
  virtual void ConnectSuccessAcknowledgement(protobuf::Message& message);
  void set_request_public_key_functor(RequestPublicKeyFunctor request_public_key);
  RequestPublicKeyFunctor request_public_key_functor() const;
  void GetGroup(Timer<std::string>& timer, protobuf::Message& message);
  void CloseNodeUpdateForClient(protobuf::Message& message);
  void InformClientOfNewCloseNode(protobuf::Message& message);
  void ConnectSuccess(protobuf::Message& message);

  friend class test::ResponseHandlerTest_BEH_ConnectAttempts_Test;

 private:
  void SendConnectRequest(const NodeId peer_node_id);
  void CheckAndSendConnectRequest(const NodeId& node_id);
  void ValidateAndSendConnectRequest(const NodeId& peer_id);
  void HandleSuccessAcknowledgementAsRequestor(const std::vector<NodeId>& close_ids);
  void HandleSuccessAcknowledgementAsReponder(NodeInfo peer, bool client);
  void ValidateAndCompleteConnectionToClient(const NodeInfo& peer, bool from_requestor,
                                             const std::vector<NodeId>& close_ids);
  void ValidateAndCompleteConnectionToNonClient(const NodeInfo& peer, bool from_requestor,
                                                const std::vector<NodeId>& close_ids);

  mutable std::mutex mutex_;
  RoutingTable& routing_table_;
  ClientRoutingTable& client_routing_table_;
  Network& network_;
  RequestPublicKeyFunctor request_public_key_functor_;
  PublicKeyHolder& public_key_holder_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_RESPONSE_HANDLER_H_
