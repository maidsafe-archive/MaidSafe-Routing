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

#ifndef MAIDSAFE_ROUTING_API_CONFIG_H_
#define MAIDSAFE_ROUTING_API_CONFIG_H_

#include <functional>
#include <string>

#include "boost/asio/ip/udp.hpp"
#include "boost/signals2/signal.hpp"

#include "maidsafe/common/rsa.h"
#include "maidsafe/routing/node_id.h"

namespace maidsafe {

namespace rudp {
struct EndpointPair;
class ManagedConnections;
}  // namespace rudp

namespace routing {

// Send method connection types

enum class ConnectType : int32_t {
    kSingle = 1,
    kClosest,
    kGroup
};

// Send method return codes
enum SendErrors {
  kInvalidDestinatinId = -1,
  kInvalidSourceId = -2,
  kInvalidType = -3,
  kEmptyData = -4
};

/****************************************************************************
* if using boost::bind or std::bind, use **shared_from_this** pointers      *
* to preserve lifetimes of functors. The MessageRecievedFunctor WILL        *
* ensure functors are deleted when the system timeout is reached.           *
****************************************************************************/
typedef std::function<void(int32_t /*return code*/,
                           std::string /*message*/)> MessageReceivedFunctor;
// check and get public key and run ValidateThisNode method.
typedef std::function<void(const std::string& /*node Id*/ ,
                           const rudp::EndpointPair& /*their Node endpoint */,
                           const bool /*client ? */,
                           const rudp::EndpointPair& /*our Node endpoint */)> NodeValidationFunctor;
typedef std::function<void(const std::string& /*node Id*/ ,
                           const asymm::PublicKey &public_key,
                           const rudp::EndpointPair& /*their Node endpoint */,
                           const rudp::EndpointPair& /*our Node endpoint */,
                           const bool /*client ? */)> NodeValidatedFunctor;

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_API_CONFIG_H_
