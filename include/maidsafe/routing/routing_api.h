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
*1:  Find any node by key.                                                     *
*2:  Find any value by key.                                                    *
*3:  Ensure messages are sent to all closest nodes in order (close to furthest)*.
*4:  Provide NAT traversal techniques where necessary.                         *
*5:  Read and Write configuration file to allow bootstrap from known nodes.    *
*6:  Allow retrieval of bootstrap nodes from known location.                   *
*7:  Remove bad nodes from all routing tables (ban from network).              *
*8:  Inform of close node changes in routing table.                            *
*9:  Respond to every send that requires it, either with timeout or reply      *
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
  Routing(const asymm::Keys &keys,
          const boost::filesystem::path &full_path_and_name,
          NodeValidationFunctor node_validation_functor,
          bool client_mode);
  ~Routing();
  /**************************************************************************
  * returns current network status as int (> 0 is connected)                *
  ***************************************************************************/
  int GetStatus();
  /**************************************************************************
  *To force the node to use a specific endpoint for bootstrapping           *
  *(i.e. private network)                                                   *
  ***************************************************************************/
  bool BootStrapFromThisEndpoint(const boost::asio::ip::udp::endpoint& endpoint,
                                boost::asio::ip::udp::endpoint local_endpoint =
                                boost::asio::ip::udp::endpoint());
  /**************************************************************************
  *The reply or error (timeout) will be passed to this response_functor     *
  *error is passed as negative int (return code) and empty string           *
  * otherwise a positive return code is message type and indicates success. *
  * Sending a message to your own address will send to all connected        *
  * clients with your address (except you). Pass an empty response_functor  *
  * to indicate you do not care about a response.                           *
  ***************************************************************************/
  int Send(const NodeId destination_id,  // id of final destination
           const std::string data,  // message content (serialised data)
           const int32_t type,  // user defined message type
           const MessageReceivedFunctor response_functor,
           const int16_t timeout_seconds,
           const ConnectType);  // is this to a close node group or direct
  /**************************************************************************
  * This signal is fired on any message received that is NOT a reply to a   *
  * request made by the Send method.                                        *
  ***************************************************************************/
  boost::signals2::signal<void(int, std::string)> &MessageReceivedSignal();
  /**************************************************************************
  * This signal fires a number from 0 to 100 and represents % network health*
  **************************************************************************/
  boost::signals2::signal<void(int16_t)> &NetworkStatusSignal();
  /**************************************************************************
  * This signal fires when a new close node is inserted in routing table.   *
  * upper layers responsible for storing key/value pairs should send all    *
  * key/values between itself and the new nodes address to the new node.    *
  * Keys further than the furthest node can safely be deleted (if any)      *
  ***************************************************************************/
  boost::signals2::signal<void(std::string /*new node*/,
                               std::string /*current furthest node*/)>
                                           &CloseNodeReplacedOldNewSignal();
  boost::signals2::signal<void(const std::string& /*node Id*/,
                           const Endpoint& /*their Node */,
                           const bool /*client ? */,
                           const Endpoint& /*our Node */,
                           NodeValidatedFunctor &)> &NodeValidationSignal();

 private:
  Routing(const Routing&);
  Routing(const Routing&&);
  Routing& operator=(const Routing&);
  void Init();
  bool Join(Endpoint local_endpoint = Endpoint());
  void ReceiveMessage(const std::string &message);
  void ConnectionLost(const Endpoint &lost_endpoint);
  void ValidateThisNode(const std::string &node_id,
                      const asymm::PublicKey &public_key,
                      const Endpoint &their_endpoint,
                      const Endpoint &our_endpoint,
                      bool client);
  std::unique_ptr<RoutingPrivate> impl_;  // pimpl (data members only)
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_API_H_
