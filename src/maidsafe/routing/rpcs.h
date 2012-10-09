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

#ifndef MAIDSAFE_ROUTING_RPCS_H_
#define MAIDSAFE_ROUTING_RPCS_H_

#include <string>
#include <vector>

#include "boost/asio/ip/udp.hpp"

#include "maidsafe/common/node_id.h"

#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/api_config.h"


namespace maidsafe {

namespace routing {

namespace protobuf { class Message; }

namespace rpcs {

protobuf::Message Ping(const NodeId& node_id, const std::string& identity);

protobuf::Message Connect(const NodeId& node_id,
    const rudp::EndpointPair& our_endpoint,
    const NodeId& this_node_id,
    const NodeId& this_connection_id,
    bool client_node = false,
    rudp::NatType nat_type = rudp::NatType::kUnknown,
    bool relay_message = false,
    NodeId relay_connection_id = NodeId());

protobuf::Message FindNodes(
    const NodeId& node_id,
    const NodeId& this_node_id,
    const int& num_nodes_requested,
    bool relay_message = false,
    NodeId relay_connection_id = NodeId());

protobuf::Message ProxyConnect(
    const NodeId& node_id,
    const NodeId& this_node_id,
    const rudp::EndpointPair& endpoint_pair,
    bool relay_message = false,
    NodeId relay_connection_id = NodeId());

protobuf::Message ConnectSuccess(
    const NodeId& node_id,
    const NodeId& this_node_id,
    const NodeId& this_connection_id,
    const bool& requestor,
    const bool& client_node);

protobuf::Message ConnectSuccessAcknowledgement(
    const NodeId& node_id,
    const NodeId& this_node_id,
    const NodeId& this_connection_id,
    const bool& requestor,
    const std::vector<NodeId>& close_ids,
    const bool& client_node);

}  // namespace rpcs

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_RPCS_H_




