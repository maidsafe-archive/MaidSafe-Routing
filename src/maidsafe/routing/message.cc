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
#include "maidsafe/routing/message.h"
#include "maidsafe/routing/rpcs.h"

#include "maidsafe/common/utils.h"
#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/routing_pb.h"

namespace maidsafe {
namespace routing {

Message::Message(const std::string &message,
                 RoutingTable &routing_table,
                 NonRoutingTable &non_routing_table) :
  routing_table_(routing_table),
  non_routing_table_(non_routing_table),
  message_(),
  my_id_as_string_(routing_table_.kKeys().identity),
  my_id_(NodeId(my_id_as_string_)),
  closest_to_me_(false),
  closest_to_me_set_(false),
  in_closest_nodes_(false),
  in_closest_nodes_set_(false),
  in_managed_group_(false),
  in_managed_group_set_(false)
{
  if (message_.ParseFromString(message)) {
    LOG(kInfo) << " Message received, type: " << Type()
               << " from " << HexSubstr(SourceId().String())
               << " I am " << HexSubstr(my_id_as_string_);
    valid_ = true;
  } else {
    valid_ = false;
  }
}

bool Message::ParseMessage() {

}

bool Message::Valid() {
  return valid_ && message_.IsInitialized();
}

int32_t Message::Type() {
  return message_.type() ? message_.has_type() : 0;
}

bool Message::ClosestToMe() {
  if (!in_closest_nodes_set_)
   in_closest_nodes_ = routing_table_.AmIClosestNode(my_id_);
  closest_to_me_set_ = true;
  return closest_to_me_;
}

bool Message::InClosestNodes() {
  if (!closest_to_me_set_)
    closest_to_me_ =  routing_table_.IsMyNodeInRange(my_id_, Parameters::closest_nodes_size);
  closest_to_me_set_ = true;
  return closest_to_me_;
}

bool Message::InManagedGroup() {
  if (!in_managed_group_set_)
    in_managed_group_ =  routing_table_.IsMyNodeInRange(my_id_, Parameters::managed_group_size);
  in_managed_group_set_ = true;
  return in_managed_group_;
}

bool Message::ForMe() {
  return (Direct() == ConnectType::kSingle &&  DestinationId() == my_id_ ) ||
         (Direct() != ConnectType::kSingle &&  ClosestToMe());
}

bool Message::ForNonRoutingNode() {
  // nodes with ID
}

bool Message::ForBootstrapNode() {
  // nodes with no ID but has endpoint !! (only kind that does)
  // replace endpoitn with our SourceId and keep endpoint in vector.
}

bool Message::HasRelay() {
  return message_.has_relay();
}

Endpoint Message::RelayEndpoint() {
  Endpoint endpoint;
  endpoint.address(boost::asio::ip::address::from_string(message_.relay().ip()));
  endpoint.port(static_cast<unsigned short>(message_.relay().port()));
  return endpoint;
}

void Message::SetMeAsLast() {
  message_.set_last_id(my_id_as_string_);
}

void Message::SetMeAsSource() {
  message_.set_source_id(my_id_as_string_);
}

void Message::SetDestination(std::string destination) {
  message_.set_destination_id(destination);
}

void Message::SetData(std::string data) {
  message_.set_data(data);
}

void Message::SetRelayEndpoint(Endpoint endpoint) {
  protobuf::Endpoint *pbendpoint;
  pbendpoint = message_.mutable_relay();
  pbendpoint->set_ip(endpoint.address().to_string().c_str());
  pbendpoint->set_port(endpoint.port());
}

void Message::SetDirect(ConnectType connect_type) {
  message_.set_direct(static_cast<int32_t>(connect_type));
}

void Message::SetSignature(std::string signature) {
  message_.set_signature(signature);
}

void Message::SetType(int32_t type) {
  message_.set_type(type);
}

std::string Message::Serialise() {
  return message_.SerializeAsString();
}

int32_t Message::Id() {
  return message_.id();
}

NodeId Message::LastId() {
  if (message_.has_last_id())
    return NodeId(message_.last_id());
  else
    return NodeId();
}

NodeId Message::SourceId() {
  if (message_.has_source_id()) {
    return NodeId(message_.source_id());
  } else {
    // TODO check non-routing table !!! see if its on of ours else valid_=false;
    return NodeId();
  }
}

NodeId Message::DestinationId() {
  if (message_.has_destination_id()) {
    return NodeId(message_.destination_id());
  } else {
    valid_ = false;
    return NodeId();
  }
}

std::string Message::Signature() {
  return  message_.has_signature() ? message_.signature() : "";
}

ConnectType Message::Direct() {
  return static_cast<ConnectType>(message_.direct());
}

}  // namespace routing
}  // namespace maidsafe
