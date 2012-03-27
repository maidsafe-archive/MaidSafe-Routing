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
#include "maidsafe/transport/managed_connections.h"
#include "maidsafe/routing/version.h"

#if MAIDSAFE_ROUTING_VERSION != 100
#  error This API is not compatible with the installed library.\
  Please update the maidsafe_routing library.
#endif

namespace maidsafe {

namespace routing {

class MessageHandler;


namespace protobuf { class Message; }

class RoutingTable;
class NodeId;
class NodeInfo;
class Service;
class Rpcs;
class Timer;
class CacheManager;
class ResponseHandler;

struct Message {
 public:
  Message();
  explicit Message(const protobuf::Message &protobuf_message);
  int32_t type;
  std::string source_id;  // your id
  std::string destination_id;  //id of final destination or address
  std::string data;  // message content (serialised)
  uint16_t timeout;  // in seconds
  bool cacheable;  // can this data be cached as is
  bool direct;  // is this to a close node group or direct
  int32_t replication;
};

typedef std::function<void(int /*message type*/,
                           std::string /*message*/ )> MessageReceivedFunctor;
typedef std::function<void(const std::string& /*node Id*/ ,
                           const transport::Endpoint& /*Node endpoint */,
                           const bool)/*client ? */>  NodeValidationFunctor;



class Routing {
 public:
  Routing(bool client_mode,
          const asymm::Keys &keys);
  ~Routing();
  /****************************************************************************
  *To force the node to use a specific endpoint for bootstrapping             *
  *(i.e. private network)                                                     *
  *****************************************************************************/
  void BootStrapFromThisEndpoint(const maidsafe::transport::Endpoint& endpoint);
   /***************************************************************************
   *Set routing layer encryption on all messages (uses keys you pass)         *
   * *************************************************************************/
  bool setEncryption(bool encryption_required);
  /****************************************************************************
   *Used to set location of config files - Default "MaidSafe"                 *
   *Cannot be empty string !!                                                 *
   * *************************************************************************/
  bool setCompanyName(const std::string &company) const;
   /***************************************************************************
   *Used to set location of config files - Default "Routing"                  *
   *Cannot be empty string !!                                                 *
   * *************************************************************************/
  bool setApplicationName(const std::string &application_name) const;
  /****************************************************************************
   *Defaults are local file called bootstrap.endpoints or will use operating  *
   * system application cache directories for multi user then single user     *
   * Apple                                                                    *
   * ~/Library/Application Support/<company_name>/<application_name>/         *
   *                                                       bootstrap.endpoints*
   *  /Library/Application Support/<company_name>/<application_name>/         *
   *                                                       bootstrap.endpoints*
   * Windows                                                                  *
   * %appdata%\<company_name>\<application_name>\bootstrap.endpoints          *
   *                                                                          *
   * Linux                                                                    *
   * ~/config/<company_name>/<application_name>/bootstrap.endpoints           *
   * /var/cache/<company_name>/<application_name>/bootstrap.endpoints         *
   * Cannot be empty string !!, RECOMMEND THIS IS NOT ALTERED                 *
   * **************************************************************************/
  bool setBoostrapFilePath(const boost::filesystem::path &path) const;
  /****************************************************************************
  *The reply or error (timeout) will be passed to this response_functor       *
  *error is passed as negative int (return code) and empty string             *
  * otherwise a positive return code is message type and indicates success    *
  ****************************************************************************/
  int Send(const Message &message,
            const MessageReceivedFunctor response_functor);
  /****************************************************************************
  *this object will not start unless this functor is set !!                   *
  *this functor MUST call ValideThisNode with result                          *
  ****************************************************************************/
  void setNodeValidationFunctor(NodeValidationFunctor &node_validation_functor);
  /***************************************************************************
   * on completion of above functor this method MUST be passed the results   *
   **************************************************************************/
  void ValidateThisNode(const std::string &node_id,
                        const asymm::PublicKey &public_key,
                        const transport::Endpoint &endpoint,
                        bool client);
  /****************************************************************************
  * this will return all known connections made by *your* client ID at this   *
  * time. Sending a message to your own address will send to all connected    *
  * clients with your address (except you). If you are a node and not a client*
  * this method will return false.                                            *
  *****************************************************************************/
  bool GetAllMyClientConnections(std::vector<transport::Endpoint> *clients);
  /***************************************************************************
  * This signal is fired on any message received that is NOT a reply to a    *
  * request made by the Send method.                                         *
  ****************************************************************************/
  boost::signals2::signal<void(int, std::string)> &MessageReceivedSignal();
  /****************************************************************************
  * This signal fires a number from 0 to 100 and represents % network health  *
  ****************************************************************************/
  boost::signals2::signal<void(unsigned int)> &NetworkStatusSignal();
  /****************************************************************************
  * This signal fires when a new close node is inserted in routing table.     *
  * upper layers responsible for storing key/value pairs should send all      *
  * key/values between itself and the new nodes address to the new node.      *
  * Keys further than the furthest node can safely be deleted (if any)        *
  *****************************************************************************/
  boost::signals2::signal<void(std::string /*new node*/,
                               std::string /*current furthest node*/ )>
                                           &CloseNodeReplacedOldNewSignal();
 private:
  Routing(const Routing&);  // no copy
  Routing& operator=(const Routing&);  // no assign
  void Init();
  void Join();
  void ReceiveMessage(const std::string &message);
  AsioService asio_service_;
  std::vector<transport::Endpoint> bootstrap_nodes_;
  asymm::Keys keys_;
  transport::Endpoint node_local_endpoint_;
  transport::Endpoint node_external_endpoint_;
  std::shared_ptr<transport::ManagedConnections> transport_;
  std::shared_ptr<RoutingTable> routing_table_;
  std::shared_ptr<Timer> timer_;
  std::shared_ptr<MessageHandler> message_handler_;
  boost::signals2::signal<void(int, std::string)> message_received_signal_;
  boost::signals2::signal<void(unsigned int)> network_status_signal_;
  boost::signals2::signal<void(std::string, std::string)>
                                                    close_node_from_to_signal_;
  std::map<uint32_t, std::pair<std::shared_ptr<boost::asio::deadline_timer>,
                              MessageReceivedFunctor> > waiting_for_response_;
  std::vector<NodeInfo> client_connections_;  // hold connections to clients only
  std::vector<NodeInfo> client_routing_table_;  // when node is client this is
  // closest nodes to the client.
  bool joined_;
  NodeValidationFunctor node_validation_functor_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_API_H_
