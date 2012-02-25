/* Copyright (c) 2009 maidsafe.net limited
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    * Neither the name of the maidsafe.net limited nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <memory>
#include <queue>
#include <vector>

#include "boost/thread/locks.hpp"
#include "boost/asio/io_service.hpp"
#include "boost/filesystem.hpp"
#include "boost/filesystem/fstream.hpp"

#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/maidsafe_routing.h"
#include "maidsafe/routing/log.h"

#include "maidsafe/transport/rudp_transport.h"
#include "maidsafe/transport/transport.h"
#include "maidsafe/transport/utils.h"

#include "maidsafe/common/rsa.h"
#include "maidsafe/common/utils.h"

namespace maidsafe {

namespace routing {

  namespace bfs = boost::filesystem3;
  typedef bfs::ifstream ifs;
  typedef bfs::ofstream ofs;
  typedef protobuf::Contact Contact;

  // check correctness of config settings  
  static_assert(kReplicationSize <= kClosestNodes,
                "Cannot set replication factor larger than closest nodes");
  static_assert(kClosestNodes <= kRoutingTableSize,
                "Cannot set closest nodes larger than routing table");

class RoutingPrivate {
public:
   RoutingPrivate();
   bool ReadConfigFile();
   bool WriteConfigFile();
   transport::Endpoint GetLocalEndpoint();
   void RecieveMessage(std::string &message);
   void ProcessMessage(protobuf::Message &message);
   void doFindNodeResponse(protobuf::Message &message);
   void doFindNodeRequest(protobuf::Message &message);
   void doConnectResponse(protobuf::Message &message);
   void doConnectRequest(protobuf::Message &message);
   void SendOn(const protobuf::Message &message, NodeId &node);
public: // members
   std::vector<Contact> bootstrap_nodes_;
   NodeId my_node_id_;
   asymm::PrivateKey my_private_key_;
   Contact my_contact_;
   RoutingTable routing_table_;
   bfs::path config_file_;
   bool private_key_is_set_;
   bool node_is_set_;
   boost::signals2::signal<void(uint16_t, std::string)> message_recieved_sig_;
   boost::signals2::signal<void(int16_t)> network_status_sig_;
 private:
   boost::asio::io_service service_;
   uint16_t cache_size_hint_;
   std::unique_ptr<transport::RudpTransport> transport_;
   std::map<NodeId, asymm::PublicKey> public_keys_;
   std::vector<std::pair<std::string, std::string> > cache_chunks_;
};

RoutingPrivate::RoutingPrivate() :
  bootstrap_nodes_(),
  my_node_id_(),
  my_private_key_(),
  my_contact_(),
  routing_table_(my_contact_), // TODO FIXME contact is empty here
  config_file_("dht_config"),
  private_key_is_set_(false),
  node_is_set_(false),
  service_(),
  cache_size_hint_(kNumChunksToCache),
  transport_ (new  transport::RudpTransport(service_)),
  public_keys_(), 
  cache_chunks_()
  {}

bool RoutingPrivate::ReadConfigFile() {
  protobuf::ConfigFile protobuf;
  if (!bfs::exists(config_file_) || !bfs::is_regular_file(config_file_))
    return false;
  try {
    ifs config_file_stream(config_file_);
    if (!protobuf.ParseFromString(config_file_.c_str()))
      return false;
    if(!private_key_is_set_) {
      if(!protobuf.has_private_key()) {
        return false;
      } else {
        asymm::DecodePrivateKey(protobuf.private_key(), &my_private_key_);
      }
    }
    if (!node_is_set_) {
      if(protobuf.has_node_id()) {
         my_node_id_ = NodeId(protobuf.node_id());
       } else {
        return false;
       }
    }
    for (auto i = 0; i != protobuf.contact_size(); ++i) 
       bootstrap_nodes_.push_back(protobuf.contact(i));
  }  catch(const std::exception &e) {
    // TODO fix DLOG - need to update common log types
       DLOG(ERROR) << "Exception: " << e.what();
    return false;
  }
  return true;
}

bool RoutingPrivate::WriteConfigFile() {
  // TODO implement 
return false;
}


transport::Endpoint RoutingPrivate::GetLocalEndpoint() {
  std::vector<transport::IP> local_ips(transport::GetLocalAddresses());
  transport::Port  port = RandomInt32() % 1600 + 30000;
// TODO we must only listen on the correct local port
  // this is a very old issue.
  bool breakme(false);
  for (uint16_t i = port; i < 35000; ++i) {
    for (auto it = local_ips.begin(); it != local_ips.end(); ++it) {
      transport::Endpoint ep;
      ep.ip = *it;
      ep.port = i;
      if (transport_->StartListening(ep) == transport::kSuccess) {
        // TODO check we can get to at least a bootsrap node !!! then we
        // have the correct ep
//         if (send and recieve)  // maybe connect is enough !!
//          break; ou of both loops - set a var
            breakme = true;
//         else
//           transport_->StopListening();
      }
      if (breakme)
        break;
    }
  }
}


void RoutingPrivate::RecieveMessage(std::string &message) {
  protobuf::Message msg;
  if(msg.ParseFromString(message))
    ProcessMessage(msg);
}

bool isCacheable(protobuf::Message &message) {
 return (message.has_cachable() && message.cachable());
}

bool isDirect(protobuf::Message &message) {
  return (message.has_direct() && message.direct());
}

void RoutingPrivate::ProcessMessage(protobuf::Message& message) {
  // handle cache data
  if (isCacheable(message)) {
    if (message.response()) {
      std::pair<std::string, std::string> data;
      try {
        // check data is valid TODO FIXME - ask CAA
        if (crypto::Hash<crypto::SHA512>(message.data()) != message.source_id())
          return;
        data = std::make_pair(message.source_id(), message.data());
        cache_chunks_.push_back(data);
        while (cache_chunks_.size() > cache_size_hint_)
          cache_chunks_.erase(cache_chunks_.begin());
      } catch (std::exception &e) {
        // oohps reduce cache size quickly
        cache_size_hint_ = cache_size_hint_ / 2;
        while (cache_chunks_.size() > cache_size_hint_)
          cache_chunks_.erase(cache_chunks_.begin()+1);
      }
    }
  } else  { // request
     for(auto it = cache_chunks_.begin(); it != cache_chunks_.end(); ++it) {
       if ((*it).first == message.source_id()) {
          message.set_destination_id(message.source_id());
          message.set_cachable(true);
          message.set_data((*it).second);
          message.set_source_id(my_node_id_.String());
          message.set_direct(true);
          message.set_response(false);
          NodeId next_node =
              routing_table_.GetClosestNode(NodeId(message.destination_id()));
          SendOn(message, next_node);
          return; // our work here is done - send it home !!
       }
     }
  }
  // is it for us ??
  if (!routing_table_.AmIClosestNode(NodeId(message.destination_id()))) {
    NodeId next_node =
              routing_table_.GetClosestNode(NodeId(message.destination_id()));
    SendOn(message, next_node);
    return;
  } else { // I am closest
    if (isDirect(message)) {
      if (message.destination_id() != my_node_id_.String()) {
      // TODO send back a failure I presume !!
      } else {
        message_recieved_sig_(message.type(), message.data());
        return;
      }
    }
    if (message.type() == 1) {// find_nodes
      if (message.has_response() && message.response()) {
        doFindNodeResponse(message);
        return; // Job done !!
      } else {
        doFindNodeRequest(message);
        return; 
      }
    }
    // I am closest so will send to all my replicant nodes
    message.set_direct(true);
    message.set_source_id(my_node_id_.String());
    auto close =
          routing_table_.GetClosestNodes(NodeId(message.destination_id()),
                                         kReplicationSize);
     for (auto it = close.begin(); it != close.end(); ++it) {
       message.set_destination_id((*it).String());
       NodeId send_to = routing_table_.GetClosestNode((*it));
       SendOn(message, send_to);
     }
     message_recieved_sig_(message.type(), message.data());
     return;
   }
}

void RoutingPrivate::SendOn(const protobuf::Message& message, NodeId& node) {
  std::string message_data(message.SerializeAsString());
  NodeId send_to = routing_table_.GetClosestNode(node);
  // TODO managed connections get this !! post to asio_service !!
}

void RoutingPrivate::doFindNodeResponse(protobuf::Message& message)
{
   protobuf::FindNodesResponse find_nodes;
   if (! find_nodes.ParseFromString(message.data()))
    return;
   for (int i = 0; i < find_nodes.nodes().size(); ++i)
     routing_table_.AddNode(NodeId(find_nodes.nodes(i)));
}

void RoutingPrivate::doFindNodeRequest(protobuf::Message& message)
{
  protobuf::FindNodesRequest find_nodes;
  protobuf::FindNodesResponse found_nodes;
  std::vector<NodeId> nodes;
  nodes = routing_table_.GetClosestNodes(NodeId(message.destination_id()),
                                         find_nodes.num_nodes_requested());
  
  for (auto it = nodes.begin(); it != nodes.end(); ++it) 
      found_nodes.add_nodes((*it).String());
  message.set_destination_id(message.source_id());
  message.set_source_id(my_node_id_.String());
  message.set_cachable(false);
  message.set_data(found_nodes.SerializeAsString());
  message.set_direct(true);
  message.set_response(true);
  message.set_type(1);
  message.set_failure(false);
  NodeId send_to(message.destination_id());
  SendOn(message, send_to);
}

void RoutingPrivate::doConnectResponse(protobuf::Message& message)
{
  // TODO - check contact for direct conencted node - i.e try a
  // quick connect / ping to the remote endpoint and if reachable
  // store in bootstrap_nodes_ and do a WriteConfigFile()
  // this may be where we need a ping command to iterate and remove
  // any long dead nodes from the table.
  // Keep at least 1000 nodes in table and drop any dead beyond this
}

// ********************API implementation* *************************************
Routing::Routing() : pimpl_(new RoutingPrivate())  {}

bool Routing::StartVault(boost::asio::io_service& service) { // NOLINT
   if (! pimpl_->ReadConfigFile())
     return false;
//    pimpl_->transport_ = (transport::RudpTransport(service));
   pimpl_->routing_table_ = RoutingTable(pimpl_->my_contact_);
   return false; // not implemented need to start network and routing table
}

bool Routing::StartClient(boost::asio::io_service& service) {
  //TODO client will join network using pmid BUT will request a
  // relay conenction. Vaults (i.e. routing table) will accept a range
  // of these, Initially set to 64 but we shoudl make this dynamic later
}

void Routing::Send(const protobuf::Message &msg) {
 NodeId next_node =
        pimpl_->routing_table_.GetClosestNode(NodeId(msg.destination_id()));
 pimpl_->SendOn(msg, next_node);
}

/// Setters

bool Routing::setConfigFilePath(boost::filesystem3::path &config_file) {
   if (bfs::exists(config_file) && !bfs::is_regular_file(config_file))
    return false;
   pimpl_->config_file_ = config_file;
   return true;
}

bool Routing::setMyPrivateKey(asymm::PrivateKey& key) {
  pimpl_->my_private_key_ = key;
  pimpl_->private_key_is_set_ = true;
  return true;
}

bool Routing::setMyNodeId(NodeId& node) {
  pimpl_->my_node_id_ = node;
  pimpl_->node_is_set_ = true;
  return true;
}

/// Getters

asymm::PrivateKey Routing::MyPrivateKey() {
  return pimpl_->my_private_key_ ;
}

NodeId Routing::MyNodeID() {
  return pimpl_->my_node_id_;
}

bfs::path Routing::ConfigFilePath() {
  return pimpl_->config_file_;
}

/// Signals
boost::signals2::signal<void(uint16_t, std::string)> & Routing::MessageReceivedSignal() {
  return  pimpl_->message_recieved_sig_;
}

boost::signals2::signal<void(int16_t)> & Routing::NetworkStatusSignal() {

  return pimpl_->network_status_sig_;
}


// ******************** END Of API implementations *****************************

// TODO get messages from transport

}  // namespace routing
}  // namespace maidsafe