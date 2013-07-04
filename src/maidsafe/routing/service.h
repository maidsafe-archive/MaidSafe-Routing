/* Copyright 2012 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

#ifndef MAIDSAFE_ROUTING_SERVICE_H_
#define MAIDSAFE_ROUTING_SERVICE_H_

#include <memory>

#include "maidsafe/routing/api_config.h"


namespace maidsafe {

namespace routing {

namespace protobuf { class Message; }

class NetworkUtils;
class ClientRoutingTable;
class RoutingTable;

class Service {
 public:
  Service(RoutingTable& routing_table,
          ClientRoutingTable& client_routing_table,
          NetworkUtils& network);
  virtual ~Service();
  // Handle all incoming requests and send back reply
  virtual void Ping(protobuf::Message& message);
  virtual void Connect(protobuf::Message& message);
  virtual void FindNodes(protobuf::Message& message);
  virtual void ConnectSuccess(protobuf::Message& message);
  virtual void GetGroup(protobuf::Message& message);
  void set_request_public_key_functor(RequestPublicKeyFunctor request_public_key);
  RequestPublicKeyFunctor request_public_key_functor() const;

 private:
  void ConnectSuccessFromRequester(NodeInfo& peer);
  void ConnectSuccessFromResponder(NodeInfo& peer, const bool &client);
  bool CheckPriority(const NodeId& this_node, const NodeId& peer_node);

  RoutingTable& routing_table_;
  ClientRoutingTable& client_routing_table_;
  NetworkUtils& network_;
  RequestPublicKeyFunctor request_public_key_functor_;
};

}  // namespace routing

}  // namespace maidsafe


#endif  // MAIDSAFE_ROUTING_SERVICE_H_
