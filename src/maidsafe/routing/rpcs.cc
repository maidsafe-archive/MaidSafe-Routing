/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#include "maidsafe/routing/rpcs.h"

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/routing/node_info.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/message_handler.h"
#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {

namespace rpcs {

// This is maybe not required and might be removed
protobuf::Message Ping(const NodeId& node_id, const std::string& identity) {
  assert(node_id.IsValid() && "Invalid node_id");
  assert(!identity.empty() && "Invalid identity");
  protobuf::Message message;
  protobuf::PingRequest ping_request;
  ping_request.set_ping(true);
#ifdef TESTING
  ping_request.set_timestamp(GetTimeStamp());
#endif
  message.set_destination_id(node_id.string());
  message.set_source_id(identity);
  message.set_routing_message(true);
  message.add_data(ping_request.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kPing));
  message.set_request(true);
  message.set_client_node(false);
  message.set_hops_to_live(Parameters::hops_to_live);
  assert(message.IsInitialized() && "Uninitialised message");
  return message;
}

protobuf::Message Connect(const NodeId& node_id, const rudp::EndpointPair& our_endpoint,
                          const NodeId& this_node_id, const NodeId& this_connection_id,
                          bool client_node, rudp::NatType nat_type, bool relay_message,
                          NodeId relay_connection_id) {
  assert(node_id.IsValid() && "Invalid node_id");
  assert(this_node_id.IsValid() && "Invalid my node_id");
  assert(this_connection_id.IsValid() && "Invalid this_connection_id");
  assert((!our_endpoint.external.address().is_unspecified() ||
          !our_endpoint.local.address().is_unspecified()) &&
         "Unspecified endpoint");
  protobuf::Message message;
  protobuf::ConnectRequest protobuf_connect_request;
  protobuf_connect_request.set_peer_id(node_id.string());
  protobuf::Contact* contact = protobuf_connect_request.mutable_contact();
  SetProtobufEndpoint(our_endpoint.external, contact->mutable_public_endpoint());
  SetProtobufEndpoint(our_endpoint.local, contact->mutable_private_endpoint());
  contact->set_node_id(this_node_id.string());
  contact->set_connection_id(this_connection_id.string());
  contact->set_nat_type(NatTypeProtobuf(nat_type));
#ifdef TESTING
  protobuf_connect_request.set_timestamp(GetTimeStamp());
#endif
  message.set_id(RandomInt32());
  message.set_destination_id(node_id.string());
  message.set_routing_message(true);
  message.add_data(protobuf_connect_request.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kConnect));
  message.set_request(true);
  message.set_client_node(client_node);
  message.set_hops_to_live(Parameters::hops_to_live);
  message.set_ack_id(RandomInt32());

  if (!relay_message) {
    message.set_source_id(this_node_id.string());
  } else {
    message.set_relay_id(this_node_id.string());
    message.set_relay_connection_id(relay_connection_id.string());
  }

  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

protobuf::Message FindNodes(const NodeId& node_id, const NodeId& this_node_id,
                            int num_nodes_requested, bool relay_message,
                            NodeId relay_connection_id) {
  assert(node_id.IsValid() && "Invalid node_id");
  assert(this_node_id.IsValid() && "Invalid my node_id");
  protobuf::Message message;
  protobuf::FindNodesRequest find_nodes;
  find_nodes.set_num_nodes_requested(num_nodes_requested);
#ifdef TESTING
  find_nodes.set_timestamp(GetTimeStamp());
#endif
  message.set_last_id(this_node_id.string());
  message.set_destination_id(node_id.string());
  message.set_routing_message(true);
  message.add_data(find_nodes.SerializeAsString());
  message.set_direct(false);
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kFindNodes));
  message.set_request(true);
  message.add_route_history(this_node_id.string());
  message.set_client_node(false);
  message.set_visited(false);
  message.set_id(RandomInt32());
  if (!relay_message) {
    message.set_source_id(this_node_id.string());
  } else {
    message.set_relay_id(this_node_id.string());
    message.set_relay_connection_id(relay_connection_id.string());
  }
  message.set_hops_to_live(Parameters::hops_to_live);
  message.set_ack_id(RandomInt32());
  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

protobuf::Message ConnectSuccess(const NodeId& node_id, const NodeId& this_node_id,
                                 const NodeId& this_connection_id, bool requestor,
                                 bool client_node) {
  assert(node_id.IsValid() && "Invalid node_id");
  assert(this_node_id.IsValid() && "Invalid my node_id");
  assert(this_connection_id.IsValid() && "Invalid this_connection_id");
  protobuf::Message message;
  protobuf::ConnectSuccess protobuf_connect_success;
  protobuf_connect_success.set_node_id(this_node_id.string());
  protobuf_connect_success.set_connection_id(this_connection_id.string());
  protobuf_connect_success.set_requestor(requestor);
  message.set_destination_id(node_id.string());
  message.set_routing_message(true);
  message.add_data(protobuf_connect_success.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kConnectSuccess));
  message.set_client_node(client_node);
  message.set_hops_to_live(Parameters::hops_to_live);
  message.set_source_id(this_node_id.string());
  message.set_request(requestor);
  message.set_id(RandomInt32());
  message.set_ack_id(RandomInt32());
  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

protobuf::Message ConnectSuccessAcknowledgement(const NodeId& node_id, const NodeId& this_node_id,
                                                const NodeId& this_connection_id,
                                                bool requestor,
                                                const std::vector<NodeInfo>& close_nodes,
                                                bool client_node) {
  assert(node_id.IsValid() && "Invalid node_id");
  assert(this_node_id.IsValid() && "Invalid my node_id");
  assert(this_connection_id.IsValid() && "Invalid this_connection_id");
  protobuf::Message message;
  protobuf::ConnectSuccessAcknowledgement protobuf_connect_success_ack;
  protobuf_connect_success_ack.set_node_id(this_node_id.string());
  protobuf_connect_success_ack.set_connection_id(this_connection_id.string());
  protobuf_connect_success_ack.set_requestor(requestor);
  for (const auto& i : close_nodes) {
    protobuf_connect_success_ack.add_close_ids(i.id.string());
  }
  message.set_destination_id(node_id.string());
  message.set_routing_message(true);
  message.add_data(protobuf_connect_success_ack.SerializeAsString());
  message.set_direct(true);
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kConnectSuccessAcknowledgement));
  message.set_client_node(client_node);
  message.set_hops_to_live(Parameters::hops_to_live);
  message.set_source_id(this_node_id.string());
  message.set_request(false);
  message.set_id(RandomInt32());
  message.set_ack_id(RandomInt32());
  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

protobuf::Message InformClientOfNewCloseNode(const NodeId& node_id, const NodeId& this_node_id,
                                             const NodeId& client_node_id) {
  assert(node_id.IsValid() && "Invalid node_id");
  assert(this_node_id.IsValid() && "Invalid my node_id");
  protobuf::Message message;
  protobuf::InformClientOfhNewCloseNode inform_client_of_new_close_node;
  inform_client_of_new_close_node.set_node_id(node_id.string());
  message.add_data(inform_client_of_new_close_node.SerializeAsString());
  message.set_destination_id(client_node_id.string());
  message.set_source_id(this_node_id.string());
  message.set_routing_message(true);
  message.set_direct(false);
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kInformClientOfNewCloseNode));
  message.set_request(true);
  message.set_client_node(false);
  message.set_hops_to_live(2);
  message.set_visited(false);
  message.set_id(RandomInt32());
  message.set_ack_id(RandomInt32());
  assert(message.IsInitialized() && "Unintialised message");
  return message;
}


protobuf::Message GetGroup(const NodeId& node_id, const NodeId& my_node_id) {
  assert(node_id.IsValid() && "Invalid node_id");
  assert(my_node_id.IsValid() && "Invalid my node_id");
  protobuf::Message message;
  protobuf::GetGroup get_group;
  get_group.set_node_id(node_id.string());
  message.add_data(get_group.SerializeAsString());
  message.set_destination_id(node_id.string());
  message.set_source_id(my_node_id.string());
  message.set_routing_message(true);
  message.set_direct(false);
  message.set_replication(1);
  message.set_type(static_cast<int32_t>(MessageType::kGetGroup));
  message.set_request(true);
  message.set_client_node(false);
  message.set_hops_to_live(Parameters::hops_to_live);
  message.set_visited(false);
  message.set_id(RandomInt32());
  message.set_ack_id(RandomInt32());
  assert(message.IsInitialized() && "Unintialised message");
  return message;
}

protobuf::Message Ack(const NodeId& node_id, const NodeId& my_node_id, int32_t ack_id) {
  assert(node_id.IsValid() && "Invalid node_id");
  assert(my_node_id.IsValid() && "Invalid my node_id");
  assert((ack_id != 0) && "Invalid ack id");
  protobuf::Message message;
  message.set_ack_id(ack_id);
  message.set_type(static_cast<int>(MessageType::kAcknowledgement));
  message.set_source_id(my_node_id.string());
  message.set_destination_id(node_id.string());
  message.set_direct(true);
  message.set_request(false);
  message.set_client_node(true);
  message.set_routing_message(true);
  message.set_hops_to_live(Parameters::hops_to_live);
  message.set_id(RandomInt32());
  return message;
}

}  // namespace rpcs

}  // namespace routing

}  // namespace maidsafe
