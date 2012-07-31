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

#include <memory>
#include <string>

#include "boost/asio/ip/udp.hpp"
#include "boost/date_time/posix_time/posix_time_config.hpp"
#include "boost/filesystem/path.hpp"

#include "maidsafe/common/rsa.h"

#include "maidsafe/routing/api_config.h"


namespace maidsafe {

namespace routing {

namespace test { class GenericNode; }

struct RoutingPrivate;
struct NodeInfo;
class NodeId;

class Routing {
 public:
  // Providing empty key means that, on Join it will join the network anonymously.  This will allow
  // Send/Recieve messages to/from network.
  // WARNING: CONNECTION TO NETWORK WILL ONLY STAY FOR 60 SECONDS.
  // Users are expected to recreate routing object with right credentials and call Join method to
  // join the routing network.
  Routing(const asymm::Keys& keys, const bool& client_mode);

  ~Routing();

  // Joins the network.  Valid functor for node validation must be passed to allow node validatation
  // or else no node will be added to routing and will fail to  join the network.  To force the node
  // to use a specific endpoint for bootstrapping, provide peer_endpoint (i.e. private network).
  void Join(Functors functors,
            boost::asio::ip::udp::endpoint peer_endpoint = boost::asio::ip::udp::endpoint());

  // WARNING: THIS FUNCTION SHOULD BE ONLY USED TO JOIN FIRST TWO ZERO STATE NODES.
  int ZeroStateJoin(Functors functors,
                    const boost::asio::ip::udp::endpoint& local_endpoint,
                    const NodeInfo& peer_node_info);

  // Returns current network status as int (> 0 is connected).
  int GetStatus() const;

  // The reply or error (timeout) will be passed to this response_functor.  Error is passed as
  // negative int (return code) and empty string, otherwise a positive return code is message type
  // and indicates success.  Sending a message to your own address will send to all connected
  // clients with your address (except you).  Pass an empty response_functor to indicate you do not
  // care about a response.
  void Send(const NodeId& destination_id,      // ID of final destination
            const NodeId& group_id,            // ID of sending group
            const std::string& data,           // message content (serialised data)
            const int32_t& type,               // user defined message type
            ResponseFunctor response_functor,
            const boost::posix_time::time_duration& timeout,
            const ConnectType& connect_type);  // whether this is to a close node group or direct

  // Confirm (if we can) two nodes are within a group range.  For small networks or new node on
  // network, this function may yield many false negatives.  In the case of a negative, actual
  // confirmation can be achieved by sending an indirect message to the node address and checking
  // all returned NodeIds.
  bool ConfirmGroupMembers(const NodeId& node1, const NodeId& node2);

  friend class test::GenericNode;

 private:
  Routing(const Routing&);
  Routing(const Routing&&);
  Routing& operator=(const Routing&);

  bool CheckBootstrapFilePath() const;
  void ConnectFunctors(const Functors& functors);
  void DisconnectFunctors();
  void BootstrapFromThisEndpoint(const Functors& functors,
                                 const boost::asio::ip::udp::endpoint& endpoint);
  void DoJoin(const Functors& functors);
  int DoBootstrap(const Functors& functors);
  int DoFindNode();
  void ReceiveMessage(const std::string& message, std::weak_ptr<RoutingPrivate> impl);
  void ConnectionLost(const boost::asio::ip::udp::endpoint& lost_endpoint);

  // pimpl (data members only)
  std::shared_ptr<RoutingPrivate> impl_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_API_H_
