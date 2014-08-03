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

#include "maidsafe/routing/service.h"

namespace maidsafe {

namespace routing {

template <>
void Service<ClientNode>::Connect(protobuf::Message& message) {
  if (message.destination_id() != connections_.kNodeId().data.string()) {
    // Message not for this node and we should not pass it on.
    LOG(kError) << "Message not for this node.";
    message.Clear();
    return;
  }
  protobuf::ConnectRequest connect_request;
  protobuf::ConnectResponse connect_response;
  if (!connect_request.ParseFromString(message.data(0))) {
    LOG(kVerbose) << "Unable to parse connect request.";
    message.Clear();
    return;
  }

  if (connect_request.peer_id() != connections_.kNodeId().data.string()) {
    LOG(kError) << "Message not for this node.";
    message.Clear();
    return;
  }

  NodeInfo peer_node;
  peer_node.id = NodeId(connect_request.contact().node_id());
  peer_node.connection_id = NodeId(connect_request.contact().connection_id());
  LOG(kVerbose) << "[" << connections_.kNodeId() << "]"
                << " received Connect request from " << peer_node.id;
  rudp::EndpointPair this_endpoint_pair, peer_endpoint_pair;
  peer_endpoint_pair.external =
      GetEndpointFromProtobuf(connect_request.contact().public_endpoint());
  peer_endpoint_pair.local = GetEndpointFromProtobuf(connect_request.contact().private_endpoint());

  if (peer_endpoint_pair.external.address().is_unspecified() &&
      peer_endpoint_pair.local.address().is_unspecified()) {
    LOG(kWarning) << "Invalid endpoint pair provided in connect request.";
    message.Clear();
    return;
  }

  // Prepare response
  connect_response.set_answer(protobuf::ConnectResponseType::kRejected);
#ifdef TESTING
  connect_response.set_timestamp(GetTimeStamp());
#endif
  connect_response.set_original_request(message.data(0));
  connect_response.set_original_signature(message.signature());

  message.clear_route_history();
  message.clear_data();
  message.set_direct(true);
  message.set_replication(1);
  message.set_client_node(ClientNode::value);
  message.set_request(false);
  message.set_hops_to_live(Parameters::hops_to_live);
  if (message.has_source_id())
    message.set_destination_id(message.source_id());
  else
    message.clear_destination_id();
  message.set_source_id(connections_.kNodeId().data.string());

  // Check rudp & routing
  LOG(kVerbose) << "Server connect request - will check routing table.";
  bool check_node_succeeded(connections_.routing_table.CheckNode(peer_node));

  if (check_node_succeeded) {
    LOG(kVerbose) << "CheckNode(node) for " << (message.client_node() ? "client" : "server")
                  << " node succeeded.";
    rudp::NatType this_nat_type(rudp::NatType::kUnknown);
    int ret_val = network_.GetAvailableEndpoint(peer_node.connection_id, peer_endpoint_pair,
                                                this_endpoint_pair, this_nat_type);
    if (ret_val != rudp::kSuccess && ret_val != rudp::kBootstrapConnectionAlreadyExists) {
      if (rudp::kUnvalidatedConnectionAlreadyExists != ret_val &&
          rudp::kConnectAttemptAlreadyRunning != ret_val) {
        LOG(kError) << "[" << connections_.kNodeId() << "] Service: "
                    << "Failed to get available endpoint for new connection to node id : "
                    << peer_node.id << ", Connection id :" << DebugId(peer_node.connection_id)
                    << ". peer_endpoint_pair.external = " << peer_endpoint_pair.external
                    << ", peer_endpoint_pair.local = " << peer_endpoint_pair.local
                    << ". Rudp returned :" << ret_val;
        message.add_data(connect_response.SerializeAsString());
        return;
      } else {  // Resolving collision by giving priority to lesser node id.
        if (!CheckPriority(peer_node.id, connections_.kNodeId())) {
          LOG(kInfo) << "Already ongoing attempt with : " << DebugId(peer_node.connection_id);
          connect_response.set_answer(protobuf::ConnectResponseType::kConnectAttemptAlreadyRunning);
          message.add_data(connect_response.SerializeAsString());
          return;
        }
      }
    }

    assert((!this_endpoint_pair.external.address().is_unspecified() ||
            !this_endpoint_pair.local.address().is_unspecified()) &&
           "Unspecified endpoint after GetAvailableEndpoint success.");

    int add_result(AddToRudp(network_, connections_.kNodeId(), connections_.kConnectionId(),
                             PeerNodeId(peer_node.id), PeerConnectionId(peer_node.connection_id),
                             PeerEndpointPair(peer_endpoint_pair), IsRequestor(false)));
    if (rudp::kSuccess == add_result) {
      connect_response.set_answer(protobuf::ConnectResponseType::kAccepted);

      connect_response.mutable_contact()->set_node_id(connections_.kNodeId().data.string());
      connect_response.mutable_contact()->set_connection_id(
          connections_.kConnectionId().data.string());
      connect_response.mutable_contact()->set_nat_type(NatTypeProtobuf(this_nat_type));

      SetProtobufEndpoint(this_endpoint_pair.local,
                          connect_response.mutable_contact()->mutable_private_endpoint());
      SetProtobufEndpoint(this_endpoint_pair.external,
                          connect_response.mutable_contact()->mutable_public_endpoint());
    }
  } else {
    LOG(kVerbose) << "CheckNode(node) for " << (message.client_node() ? "client" : "server")
                  << " node failed.";
  }

  message.add_data(connect_response.SerializeAsString());
  assert(message.IsInitialized() && "unintialised message");
}

}  // namespace routing

}  // namespace maidsafe
