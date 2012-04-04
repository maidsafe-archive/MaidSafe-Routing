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

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>  // for pair
#include <map>
#include <vector>
#include "boost/signals2/signal.hpp"
#include "boost/filesystem/path.hpp"
#include "maidsafe/common/rsa.h"
#include "maidsafe/rudp/managed_connections.h"
#include "maidsafe/routing/version.h"

#if MAIDSAFE_ROUTING_VERSION != 100
#  error This API is not compatible with the installed library.\
  Please update the maidsafe_routing library.
#endif

namespace maidsafe {

namespace routing {

namespace protobuf { class Message; }
class RoutingPrivate;

int8_t GetMajorVersion(); // API changes between Major versions
int8_t GetMinorVersion(); // internal additions to logic
int8_t GetPatchVersion(); // bugfixes

// Send method return codes
enum SendErrors {
  kInvalidDestinatinId = -1,
  kInvalidSourceId = -2,
  kInvalidType = -3,
  kEmptyData = -4
};

// required for Send method 
struct Message {
 public:
  Message();
  explicit Message(const protobuf::Message &protobuf_message);
  int32_t type;  // message type identifier
                 // if type == 100 then this is cachable data
                 // Data field must then contain serialised data only
                 // cachable data must hash (sha512) to content
  std::string destination_id;  //id of final destination
  std::string data;  // message content (serialised data)
  uint16_t timeout;  // in seconds
  bool direct;  // is this to a close node group or direct
  int32_t replication;  // defaults to 1 for direct and CloseNodes otherwise
};

/****************************************************************************
* if using boost::bind or std::bind, use **shared_from_this** pointers      *
* to preserve lifetimes of functors. The MessageRecievedFunctor WILL        *
* ensure functors are deleted when the system timeout is reached.           *
****************************************************************************/
typedef std::function<void(int /*message type*/,
                           std::string /*message*/ )> MessageReceivedFunctor;
// check and get public key and run ValidateThisNode method.
typedef std::function<void(const std::string& /*node Id*/ ,
                           const boost::asio::ip::udp::endpoint& /*their Node endpoint */,
                           const bool /*client ? */,
                           const boost::asio::ip::udp::endpoint& /*our Node endpoint */)>
                                                 NodeValidationFunctor;
typedef std::function<void(const std::string& /*node Id*/ ,
                           const asymm::PublicKey &public_key,
                           const boost::asio::ip::udp::endpoint& /*their Node endpoint */,
                           const boost::asio::ip::udp::endpoint& /*our Node endpoint */,
                           const bool /*client ? */
                          )> NodeValidatedFunctor;
/***************************************************************************
*  WARNING THIS CONSTRUCTOR WILL THROW A BOOST::FILESYSTEM_ERROR           *
* if config file is invalid                                                *
* *************************************************************************/
class Routing {
 public:
   // set keys.identity to ANONYMOUS for temporary anonymous connection.
  Routing(const asymm::Keys &keys,
          const boost::filesystem::path &full_path_and_name,
          bool client_mode);
  ~Routing();
  /**************************************************************************
  *Useful in stand alone mode (first network node)                          * 
  ***************************************************************************/
  boost::asio::ip::udp::endpoint GetEndoint();
  /**************************************************************************
  * returns current network status as int (> 0 is connected)                *
  ***************************************************************************/
  int16_t GetStatus();
  /**************************************************************************
  *To force the node to use a specific endpoint for bootstrapping           *
  *(i.e. private network)                                                   *
  ***************************************************************************/
  void BootStrapFromThisEndpoint(const boost::asio::ip::udp::endpoint& endpoint);
  /**************************************************************************
  *The reply or error (timeout) will be passed to this response_functor     *
  *error is passed as negative int (return code) and empty string           *
  * otherwise a positive return code is message type and indicates success  *
  **************************************************************************/
  int Send(const Message &message,
            const MessageReceivedFunctor response_functor);
  /**************************************************************************
  * this will return all known connections made by *your* client ID at this *
  * time. Sending a message to your own address will send to all connected  *
  * clients with your address (except you). If you are a node and not a     *
  * client this method will return false.                                   *
  ***************************************************************************/
  bool GetAllMyClientConnections(std::vector<boost::asio::ip::udp::endpoint> *clients);
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
                               std::string /*current furthest node*/ )>
                                           &CloseNodeReplacedOldNewSignal();
  boost::signals2::signal<void(const std::string& /*node Id*/ ,
                           const boost::asio::ip::udp::endpoint& /*their Node */,
                           const bool /*client ? */,
                           const boost::asio::ip::udp::endpoint& /*our Node */,
                           NodeValidatedFunctor & )> &NodeValidationSignal();
 private:
  Routing(const Routing&);  // no copy
  Routing& operator=(const Routing&);  // no assign
  void Init();
  void Join();
  void ReceiveMessage(const std::string &message);
  void ConnectionLost(boost::asio::ip::udp::endpoint &lost_endpoint);
  void ValidateThisNode(const std::string &node_id,
                      const asymm::PublicKey &public_key,
                      const boost::asio::ip::udp::endpoint &their_endpoint,
                      const boost::asio::ip::udp::endpoint &our_endpoint,
                      bool client);
  std::unique_ptr<RoutingPrivate> impl_;  // pimpl (data members only)
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_API_H_
