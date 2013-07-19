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

#ifndef MAIDSAFE_ROUTING_API_CONFIG_H_
#define MAIDSAFE_ROUTING_API_CONFIG_H_

#include <functional>
#include <string>
#include <vector>

#include "boost/asio/ip/udp.hpp"

#include "maidsafe/common/rsa.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/matrix_change.h"

namespace maidsafe {

namespace routing {

struct NodeInfo;

enum class DestinationType : int {
  kDirect = 0,
  kClosest,
  kGroup
};

typedef std::function<void(std::string)> ResponseFunctor;

// They are passed as a parameter by MessageReceivedFunctor and should be called for responding to
// the received message. Passing an empty message will mean you don't want to reply.
typedef std::function<void(const std::string& /*message*/)> ReplyFunctor;

// This is called on any message received that is NOT a reply to a request made by the Send method.
typedef std::function<void(const std::string& /*message*/,
                           const bool& /*cache_lookup*/,
                           ReplyFunctor /*reply functor*/)> MessageReceivedFunctor;

// This is fired to validate a new peer node. User is supposed to validate the node and call
// ValidateThisNode() method with valid public key.
typedef std::function<void(asymm::PublicKey /*public_key*/)> GivePublicKeyFunctor;
typedef std::function<void(NodeId /*node Id*/, GivePublicKeyFunctor)> RequestPublicKeyFunctor;

typedef std::function<bool(std::string& /*data*/)> HaveCacheDataFunctor;
typedef std::function<void(const std::string& /*data*/)> StoreCacheDataFunctor;

// This functor fires a number from 0 to 100 and represents % network health.
typedef std::function<void(const int& /*network_health*/)> NetworkStatusFunctor;

// This functor fires a validated endpoint which is usable for bootstrapping
typedef std::function<void(const boost::asio::ip::udp::endpoint& /*new_endpoint*/)>
    NewBootstrapEndpointFunctor;

// This functor fires when a new close node is inserted in routing table. Upper layers responsible
// for storing key/value pairs should send all key/values between itself and the new node's address
// to the new node. Keys further than the furthest node can safely be deleted (if any).
typedef std::function<void(const std::vector<NodeInfo>& /*new_close_nodes*/)>
    CloseNodeReplacedFunctor;

typedef std::function<void(std::shared_ptr<MatrixChange> /*matrix_change*/)>
    MatrixChangedFunctor;

// This functor fires when routing table size is over greedy limit. The furthest unnecessary
// node in routing table is dropped. Unnecessary is defined as a node who does not have us in
// it clsoest nodes.
typedef std::function<void()> RemoveFurthestUnnecessaryNode;


struct Functors {
  Functors()
      : message_received(),
        network_status(),
        close_node_replaced(),
        matrix_changed(),
        set_public_key(),
        request_public_key(),
        have_cache_data(),
        store_cache_data(),
        new_bootstrap_endpoint() {}

  MessageReceivedFunctor message_received;
  NetworkStatusFunctor network_status;
  CloseNodeReplacedFunctor close_node_replaced;
  MatrixChangedFunctor matrix_changed;
  GivePublicKeyFunctor set_public_key;
  RequestPublicKeyFunctor request_public_key;
  HaveCacheDataFunctor have_cache_data;
  StoreCacheDataFunctor store_cache_data;
  NewBootstrapEndpointFunctor new_bootstrap_endpoint;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_API_CONFIG_H_
