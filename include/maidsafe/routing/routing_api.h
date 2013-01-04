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
#include <vector>

#include "boost/asio/ip/udp.hpp"
#include "boost/date_time/posix_time/posix_time_config.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/rsa.h"

#include "maidsafe/passport/types.h"

#include "maidsafe/routing/api_config.h"


namespace maidsafe {

namespace routing {

struct NodeInfo;

namespace test { class GenericNode; }

namespace detail {

template<typename FobTypePtr>
struct is_client : public std::true_type {};

template<>
struct is_client<passport::Pmid*> : public std::false_type {};

template<>
struct is_client<const passport::Pmid*> : public std::false_type {};

}  // namespace detail


class Routing {
 public:
  // Providing nullptr key means that, on Join it will join the network anonymously.  This will
  // allow Send/Receive messages to/from network.
  // WARNING: CONNECTION TO NETWORK WILL ONLY STAY FOR 60 SECONDS.
  // Users are expected to recreate routing object with right credentials and call Join method to
  // join the routing network.
  template<typename FobTypePtr>
  explicit Routing(FobTypePtr fob_ptr) : pimpl_() {
    static_assert(std::is_pointer<FobTypePtr>::value, "fob_ptr must be a pointer.");
    asymm::Keys keys;
    keys.private_key = fob_ptr->private_key();
    keys.public_key = fob_ptr->public_key();
    InitialisePimpl(detail::is_client<FobTypePtr>::value, false,
                    NodeId(fob_ptr->name().data.string()), keys);
  }

  // Joins the network.  Valid functor for node validation must be passed to allow node validatation
  // or else no node will be added to routing and will fail to  join the network.  To force the node
  // to use a specific endpoint for bootstrapping, provide peer_endpoint (i.e. private network).
  void Join(Functors functors,
            std::vector<boost::asio::ip::udp::endpoint> peer_endpoints =
                std::vector<boost::asio::ip::udp::endpoint>());

  // WARNING: THIS FUNCTION SHOULD BE ONLY USED TO JOIN FIRST TWO ZERO STATE NODES.
  int ZeroStateJoin(Functors functors,
                    const boost::asio::ip::udp::endpoint& local_endpoint,
                    const boost::asio::ip::udp::endpoint& peer_endpoint,
                    const NodeInfo& peer_info);

  // The reply or error (timeout) will be passed to this response_functor.  Error is passed as
  // negative int (return code) and empty string, otherwise a positive return code is message type
  // and indicates success.  Sending a message to your own address will send to all connected
  // clients with your address (except you).  Pass an empty response_functor to indicate you do not
  // care about a response.
  void Send(const NodeId& destination_id,      // ID of final destination
            const std::string& data,           // message content (serialised data)
            ResponseFunctor response_functor,
            const DestinationType& destination_type,  // whether this is to a direct/close/group
            const bool& cacheable);
  // A queue with recently found nodes that can be extracted for upper layers to communicate with.
  NodeId GetRandomExistingNode() const;

  // returns true if the node id provided is in group range of node.
  bool IsNodeIdInGroupRange(const NodeId& node_id) const;

  // returns Node Id.
  NodeId kNodeId() const;

  int network_status();

  std::vector<NodeInfo> ClosestNodes();

//  bool IsConnectedToVault(const NodeId& node_id);
//  bool IsConnectedToClient(const NodeId& node_id);

  friend class test::GenericNode;

 private:
  Routing(const Routing&);
  Routing(const Routing&&);
  Routing& operator=(const Routing&);
  void InitialisePimpl(bool client_mode,
                       bool anonymous,
                       const NodeId& node_id,
                       const asymm::Keys& keys);

  class Impl;
  std::shared_ptr<Impl> pimpl_;
};

template<>
Routing::Routing(std::nullptr_t);

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_API_H_
