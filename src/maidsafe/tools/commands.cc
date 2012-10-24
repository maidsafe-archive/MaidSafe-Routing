/***************************************************************************************************
 *  Copyright 2012 maidsafe.net limited                                                            *
 *                                                                                                 *
 *  The following source code is property of maidsafe.net limited and is not meant for external    *
 *  use. The use of this code is governed by the license file LICENSE.TXT found in the root of     *
 *  this directory and also on www.maidsafe.net.                                                   *
 *                                                                                                 *
 *  You are not free to copy, amend or otherwise use this source code without the explicit written *
 *  permission of the board of directors of maidsafe.net.                                          *
 ***********************************************************************************************//**
 * @file  commands.cc
 * @brief Console commands to demo routing stand alone node.
 * @date  2012-10-19
 */

#include "maidsafe/tools/commands.h"

#include <iostream> // NOLINT

#include "boost/format.hpp"
#include "boost/filesystem.hpp"
#ifdef __MSVC__
# pragma warning(push)
# pragma warning(disable: 4127)
#endif
#include "boost/tokenizer.hpp"
#ifdef __MSVC__
# pragma warning(pop)
#endif
#include "boost/lexical_cast.hpp"
#include "maidsafe/common/crypto.h"
#include "maidsafe/common/utils.h"

namespace fs = boost::filesystem;

namespace maidsafe {

namespace routing {

namespace test {

Commands::Commands(DemoNodePtr demo_node,
                   std::vector<maidsafe::Fob> all_fobs,
                   int identity_index) : demo_node_(demo_node),
                                         all_fobs_(all_fobs),
                                         all_ids_(),
                                         identity_index_(identity_index),
                                         bootstrap_peer_ep_(),
                                         data_size_(256 * 1024),
                                         result_arrived_(false),
                                         finish_(false),
                                         wait_mutex_(),
                                         wait_cond_var_(),
                                         mark_results_arrived_() {
  for (auto &fob : all_fobs_)
    all_ids_.push_back(NodeId(fob.identity));

  demo_node->functors_.request_public_key = [this] (const NodeId& node_id,
                                                    GivePublicKeyFunctor give_public_key) {
      this->Validate(node_id, give_public_key);
  };
  demo_node->functors_.message_received = [this] (const std::string &wrapped_message,
                                                  const NodeId &group_claim,
                                                  const ReplyFunctor &reply_functor) {
    std::string reply_msg(wrapped_message + "+++" + demo_node_->fob().identity.string());
    if (std::string::npos != wrapped_message.find("request_routing_table"))
      reply_msg = reply_msg + "---" + demo_node_->SerializeRoutingTable();
    reply_functor(reply_msg);
  };
  mark_results_arrived_ = std::bind(&Commands::MarkResultArrived, this);
}

void Commands::Validate(const NodeId& node_id, GivePublicKeyFunctor give_public_key) {
  if (node_id == NodeId())
    return;

  auto iter(all_fobs_.begin());
  bool find(false);
  while ((iter != all_fobs_.end()) && !find) {
    if (iter->identity.string() == node_id.string())
      find = true;
    else
      ++iter;
  }
  if (iter != all_fobs_.end())
    give_public_key((*iter).keys.public_key);
}

void Commands::Run() {
  PrintUsage();

  if ((!demo_node_->joined()) && (identity_index_ >= 2)/* &&
      (bootstrap_peer_ep_ != boost::asio::ip::udp::endpoint())*/) {
    // All parameters have been setup via cmdline directly, join the node immediately
    std::cout << "Joining the node ......" << std::endl;
    Join();
  }

  while (!finish_) {
    std::cout << std::endl << std::endl << "Enter command > ";
    std::string cmdline;
    std::getline(std::cin, cmdline);
    {
      boost::mutex::scoped_lock lock(wait_mutex_);
      ProcessCommand(cmdline);
      wait_cond_var_.wait(lock, boost::bind(&Commands::ResultArrived, this));
      result_arrived_ = false;
    }
  }
}

void Commands::PrintRoutingTable() {
  demo_node_->PrintRoutingTable();
}

void Commands::GetPeer(const std::string &peer) {
  size_t delim = peer.rfind(':');
  try {
    bootstrap_peer_ep_.port(boost::lexical_cast<uint16_t>(peer.substr(delim + 1)));
    bootstrap_peer_ep_.address(boost::asio::ip::address::from_string(peer.substr(0, delim)));
    std::cout << "Going to bootstrap from endpoint " << bootstrap_peer_ep_ << std::endl;
  }
  catch(...) {
    std::cout << "Could not parse IPv4 peer endpoint from " << peer << std::endl;
  }
}

void Commands::ZeroStateJoin() {
  if (identity_index_ > 1) {
    std::cout << "can't exec ZeroStateJoin as a non-bootstrap node" << std::endl;
    return;
  }
  NodeInfo peer_node_info;
  if (identity_index_ == 0) {
    peer_node_info.node_id = NodeId(all_fobs_[1].identity);
    peer_node_info.public_key = all_fobs_[1].keys.public_key;
  } else {
    peer_node_info.node_id = NodeId(all_fobs_[0].identity);
    peer_node_info.public_key = all_fobs_[0].keys.public_key;
  }
  peer_node_info.connection_id = peer_node_info.node_id;

  auto f1 = std::async(std::launch::async, [=] ()->int {
    return demo_node_->ZeroStateJoin(bootstrap_peer_ep_, peer_node_info);
  });
  EXPECT_EQ(kSuccess, f1.get());
}

void Commands::SendAMsg(int identity_index, bool direct, std::string &data) {
  if ((identity_index >= 0) && (identity_index >= all_fobs_.size())) {
    std::cout << "ERROR : destination index out of range" << std::endl;
    return;
  }
  NodeId dest_id(RandomString(64));
  if (identity_index >= 0)
    dest_id = NodeId(all_fobs_[identity_index].identity);

  std::cout << "Sending a msg from : " << maidsafe::HexSubstr(demo_node_->fob().identity)
            << " to " << (direct ? ": " : "group : ") << maidsafe::HexSubstr(dest_id.string())
            << " , expect receive response from :" << std::endl;
  size_t expected_respodents(direct ? 1 : 4);
  std::vector<NodeId> closests;
  NodeId farthest_closests(CalculateClosests(dest_id, closests, expected_respodents));
  for (auto &node_id : closests)
    std::cout << "\t" << maidsafe::HexSubstr(node_id.string()) << std::endl;

  std::set<NodeId> responsed_nodes;
  std::string received_response_msg;
  std::mutex mutex;
  std::condition_variable cond_var;
  size_t messages_count(0), message_id(0), expected_messages(1);
  auto callable = [&](const std::vector<std::string> &message) {
    if (message.empty())
      return;
    std::lock_guard<std::mutex> lock(mutex);
    messages_count++;
    for (auto &msg : message) {
      std::string response_id(msg.substr(msg.find("+++") + 3, 64));
      responsed_nodes.insert(NodeId(response_id));
      received_response_msg = msg;
    }
    if (responsed_nodes.size() == expected_respodents)
      cond_var.notify_one();
  };

  {
    std::lock_guard<std::mutex> lock(mutex);
    data = ">:<" + boost::lexical_cast<std::string>(++message_id) + "<:>" + data;
  }

  boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
  demo_node_->Send(dest_id, NodeId(), data, callable,
                   boost::posix_time::seconds(12), direct, false);

  std::unique_lock<std::mutex> lock(mutex);
  bool result = cond_var.wait_for(lock, std::chrono::seconds(20),
      [&]()->bool { return messages_count == expected_messages; });

  std::cout << "Response received in "
            << boost::posix_time::microsec_clock::universal_time() - now << std::endl;
  EXPECT_TRUE(result) << "Failure in sending group message";

  std::cout << "Received response from following nodes :" << std::endl;
  for(auto &responsed_node : responsed_nodes) {
    std::cout << "\t" << maidsafe::HexSubstr(responsed_node.string()) << std::endl;
    if (responsed_node != farthest_closests)
      EXPECT_TRUE(NodeId::CloserToTarget(responsed_node, farthest_closests, dest_id));
  }

  if (std::string::npos != received_response_msg.find("request_routing_table")) {
    std::string response_node_list_msg(
        received_response_msg.substr(received_response_msg.find("---") + 3,
            received_response_msg.size() - (received_response_msg.find("---") + 3)));
    std::vector<NodeId> node_list(
        maidsafe::routing::DeserializeNodeIdList(response_node_list_msg));
    std::cout << "Received routing table from peer is :" << std::endl;
    for (auto &node_id : node_list)
      std::cout << "\t" << maidsafe::HexSubstr(node_id.string()) << std::endl;
  }
}

void Commands::Join() {
  if (demo_node_->joined()) {
    std::cout << "Current node already joined" << std::endl;
    return;
  }
  std::condition_variable cond_var;
  std::mutex mutex;

  std::weak_ptr<GenericNode> weak_node(demo_node_);
  demo_node_->functors_.network_status =
      [this, &cond_var, weak_node] (const int& result) {
        if (std::shared_ptr<GenericNode> node = weak_node.lock()) {
          if (!node->anonymous()) {
            ASSERT_GE(result, kSuccess);
          } else  {
            if (!node->joined()) {
              ASSERT_EQ(result, kSuccess);
            } else if (node->joined()) {
              ASSERT_EQ(result, kAnonymousSessionEnded);
            }
          }
          if ((result == node->expected() && !node->joined()) || node->anonymous()) {
            node->set_joined(true);
            cond_var.notify_one();
          } else {
            std::cout << "Network Status Changed" << std::endl;
            this->PrintRoutingTable();
          }
        }
      };
  std::vector<boost::asio::ip::udp::endpoint> bootstrap_endpoints;
  if (bootstrap_peer_ep_ != boost::asio::ip::udp::endpoint())
    bootstrap_endpoints.push_back(bootstrap_peer_ep_);
  demo_node_->Join(bootstrap_endpoints);

  if (!demo_node_->joined()) {
    std::unique_lock<std::mutex> lock(mutex);
    auto result = cond_var.wait_for(lock, std::chrono::seconds(20));
    EXPECT_EQ(result, std::cv_status::no_timeout);
    Sleep(boost::posix_time::millisec(600));
  }
  std::cout << "Current Node joined, following is the routing table :" << std::endl;
  PrintRoutingTable();
}

void Commands::PrintUsage() {
  std::cout << "\thelp Print options.\n";
  std::cout << "\tpeer <endpoint> Set BootStrap peer endpoint.\n";
  std::cout << "\tzerostatejoin ZeroStateJoin.\n";
  std::cout << "\tjoin Normal Join.\n";
  std::cout << "\tprt Print Local Routing Table.\n";
  std::cout << "\trrt <dest_index> Request Routing Table from peer node with the specified identity-index.\n";
  std::cout << "\tsenddirect <dest_index> Send a msg to a node with specified identity-index.\n";
  std::cout << "\tsendgroup <dest_index> Send a msg to group (default is Random GroupId,"
            << " dest_index for using existing identity as a group_id)\n";
  std::cout << "\tsendmultiple <num_msg> Send num of msg to randomly picked-up destination.\n";
  std::cout << "\tdatasize <data_size> Set the data_size for the message.\n";
  std::cout << "\nattype Print the NatType of this node.\n";
  std::cout << "\texit Exit application.\n";
}

void Commands::ProcessCommand(const std::string &cmdline) {
  if (cmdline.empty()) {
    demo_node_->PostTaskToAsioService(mark_results_arrived_);
    return;
  }

  std::string cmd;
  Arguments args;
  try {
    boost::char_separator<char> sep(" ");
    boost::tokenizer<boost::char_separator<char>> tok(cmdline, sep);
    for (auto it = tok.begin(); it != tok.end(); ++it) {
      if (it == tok.begin())
        cmd = *it;
      else
        args.push_back(*it);
    }
  }
  catch(const std::exception &e) {
    LOG(kError) << "Error processing command: " << e.what();
  }

  if (cmd == "help") {
    PrintUsage();
  } else if (cmd == "prt") {
    PrintRoutingTable();
  } else if (cmd == "rrt") {
    std::string data("request_routing_table");
    SendAMsg(boost::lexical_cast<int>(args[0]), true, data);
  } else if (cmd == "peer") {
    GetPeer(args[0]);
  } else if (cmd == "zerostatejoin") {
    ZeroStateJoin();
  } else if (cmd == "join") {
    Join();
  } else if (cmd == "senddirect") {
    std::string data(RandomAlphaNumericString(data_size_));
    SendAMsg(boost::lexical_cast<int>(args[0]), true, data);
  } else if (cmd == "sendgroup") {
    std::string data(RandomAlphaNumericString(data_size_));
    if (args.empty())
      SendAMsg(-1, false, data);
    else
      SendAMsg(boost::lexical_cast<int>(args[0]), false, data);
  } else if (cmd == "sendmultiple") {
    int num_msg(boost::lexical_cast<int>(args[0]));
    std::string data(RandomAlphaNumericString(data_size_));
    boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
    for (int i(0); i < num_msg; ++i) {
      SendAMsg(RandomUint32() % all_fobs_.size(), true, data);
    }
    std::cout << "Sent " << num_msg << " messages to randomly picked-up targets. Finished in :"
              << boost::posix_time::microsec_clock::universal_time() - now << std::endl;
  } else if (cmd == "datasize") {
    data_size_ = boost::lexical_cast<int>(args[0]);
  } else if (cmd == "nattype") {
    std::cout << "NatType for this node is : " << demo_node_->nat_type() << std::endl;
  } else if (cmd == "exit") {
    std::cout << "Exiting application...\n";
    finish_ = true;
  } else {
    std::cout << "Invalid command : " << cmd << std::endl;
    PrintUsage();
  }
  demo_node_->PostTaskToAsioService(mark_results_arrived_);
}

void Commands::MarkResultArrived() {
  boost::mutex::scoped_lock lock(wait_mutex_);
  result_arrived_ = true;
  wait_cond_var_.notify_one();
}

NodeId Commands::CalculateClosests(const NodeId& target_id,
                                   std::vector<NodeId>& closests,
                                   uint16_t num_of_closests) {
  std::sort(all_ids_.begin(), all_ids_.end(),
            [&](const NodeId &i, const NodeId &j) {
              return (NodeId::CloserToTarget(i, j, target_id));
            });
  if (num_of_closests > all_ids_.size())
    num_of_closests = all_ids_.size();
  closests.resize(num_of_closests);
  std::copy(all_ids_.begin(), all_ids_.begin() + num_of_closests, closests.begin());

  // For group msg, sending to an existing node shall exclude that node from expected list
  if (num_of_closests != 1)
    for (auto node_id(closests.begin()); node_id != closests.end(); ++node_id)
      if (*node_id == target_id) {
        closests.erase(node_id);
      if (all_ids_.size() > num_of_closests)
        closests.push_back(all_ids_[num_of_closests]);
      break;
    }

  return closests[closests.size() - 1];
}


} // namespace test

} // namespace routing

} // namespace maidsafe