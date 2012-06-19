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

/*******************************************************************************
*Guarantees                                                                    *
*______________________________________________________________________________*
*                                                                              *
*1:  Provide NAT traversal techniques where necessary.                         *
*2:  Read and Write configuration file to allow bootstrap from known nodes.    *
*3:  Allow retrieval of bootstrap nodes from known location.                   *
*4:  Remove bad nodes from all routing tables (ban from network).              *
*5:  Inform of changes in data range to be stored and sent to each node        *
*6:  Respond to every send that requires it, either with timeout or reply      *
*******************************************************************************/

#ifndef MAIDSAFE_ROUTING_ROUTING_API_H_
#define MAIDSAFE_ROUTING_ROUTING_API_H_

#include <functional>
#include <memory>
#include <string>

#include "boost/filesystem/path.hpp"
#include "boost/signals2/signal.hpp"

#include "maidsafe/common/rsa.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/api_config.h"


namespace maidsafe {

namespace routing {

struct RoutingPrivate;


/***************************************************************************
*  WARNING THIS CONSTRUCTOR WILL THROW A BOOST::FILESYSTEM_ERROR           *
* if config file is invalid                                                *
* *************************************************************************/
class Routing {
 public:
   // set keys.identity to ANONYMOUS for temporary anonymous connection.
  Routing(const asymm::Keys &keys, bool client_mode);
  ~Routing();

  /**************************************************************************
  * Joins the network                                                       *
  * To force the node to use a specific endpoint for bootstrapping, provide *
  * peer_endpoint & local_endpoint. (i.e. private network)                  *
  ***************************************************************************/
  int Join(Functors functors,
           boost::asio::ip::udp::endpoint peer_endpoint = boost::asio::ip::udp::endpoint(),
           boost::asio::ip::udp::endpoint local_endpoint = boost::asio::ip::udp::endpoint());
  /**************************************************************************
  * returns current network status as int (> 0 is connected)                *
  ***************************************************************************/
  int GetStatus();
  /**************************************************************************
  *The reply or error (timeout) will be passed to this response_functor     *
  *error is passed as negative int (return code) and empty string           *
  * otherwise a positive return code is message type and indicates success. *
  * Sending a message to your own address will send to all connected        *
  * clients with your address (except you). Pass an empty response_functor  *
  * to indicate you do not care about a response.                           *
  ***************************************************************************/
  SendStatus Send(const NodeId &destination_id,  // id of final destination
                  const NodeId &group_id,  // id of sending group
                  const std::string &data,  // message content (serialised data)
                  const int32_t &type,  // user defined message type
                  const ResponseFunctor response_functor,
                  const int16_t &timeout_seconds,
                  const ConnectType &connect_type);  // is this to a close node group or direct

  /***************************************************************************
  * This method should be called by the user in response to                  *
  * NodeValidateFunctor to add the node in routing table.                    *
  ***************************************************************************/
  void ValidateThisNode(const NodeId& node_id,
                        const asymm::PublicKey &public_key,
                        const rudp::EndpointPair &their_endpoint,
                        const rudp::EndpointPair &our_endpoint,
                        const bool &client);

 private:
  Routing(const Routing&);
  Routing(const Routing&&);
  Routing& operator=(const Routing&);
  void ConnectFunctors(Functors functors);
  void DisconnectFunctors();
  int BootStrapFromThisEndpoint(Functors functors,
                                const boost::asio::ip::udp::endpoint& endpoint,
                                const boost::asio::ip::udp::endpoint& local_endpoint);
  int DoJoin(Functors functors, Endpoint local_endpoint = Endpoint());
  void ReceiveMessage(const std::string &message);
  void ConnectionLost(const Endpoint &lost_endpoint);
  void CheckBootStrapFilePath();
  std::unique_ptr<RoutingPrivate> impl_;  // pimpl (data members only)
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_API_H_
